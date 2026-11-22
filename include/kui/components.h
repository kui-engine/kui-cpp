#pragma once

// Included by <kui/full_profile.h>. Do not include this file directly.
// Section 18 of kui_c.h (3.61.8): the native components with their native names —
// registerBuiltinComponents, ThemeManager, VirtualList, ListView, TreeView.

#include <optional>
#include <set>

KUI_CPP_HIDDEN_BEGIN
namespace kui {

inline void registerBuiltinComponents(Document* doc) {
    detail::check(kui_document_register_builtin_components(doc->cHandle()),
                  "registerBuiltinComponents");
}

// --- Themes ----------------------------------------------------------------------------
struct Theme {
    String name;
    std::unordered_map<String, String> variables;
};

class ThemeManager {
public:
    ThemeManager() { detail::check(kui_theme_manager_create(&raw_), "ThemeManager"); }
    ~ThemeManager() { if (raw_) kui_theme_manager_release(raw_); }
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

    void registerTheme(const String& name, const Theme& theme) {
        std::vector<KuiCKeyValue> vars;
        vars.reserve(theme.variables.size());
        for (const auto& entry : theme.variables)
            vars.push_back({entry.first.data(), entry.first.size(), entry.second.data(), entry.second.size()});
        detail::check(kui_theme_manager_register(raw_, name.data(), name.size(),
                                                 vars.empty() ? nullptr : vars.data(), vars.size()),
                      "ThemeManager::registerTheme");
    }
    // An unknown theme is ignored, as in native.
    void apply(const String& themeName, Document* doc) {
        const KuiStatus status = kui_theme_manager_apply(raw_, themeName.data(), themeName.size(), doc->cHandle());
        if (status != KUI_ERR_NOT_FOUND) detail::check(status, "ThemeManager::apply");
    }
    String activeTheme() const {
        return detail::readString([this](char* buffer, std::size_t capacity, std::size_t* size) {
            return kui_theme_manager_get_active(raw_, buffer, capacity, size);
        }, "ThemeManager::activeTheme");
    }
    // nullptr when not registered; the pointer stays valid until the next getTheme call.
    const Theme* getTheme(const String& name) const {
        std::size_t count = 0;
        if (kui_theme_manager_get_variable_count(raw_, name.data(), name.size(), &count) != KUI_OK)
            return nullptr;
        lookup_.name = name;
        lookup_.variables.clear();
        for (std::size_t index = 0; index < count; ++index) {
            std::size_t nameLen = 0;
            std::size_t valueLen = 0;
            kui_theme_manager_get_variable(raw_, name.data(), name.size(), index, nullptr, 0, &nameLen,
                                           nullptr, 0, &valueLen);
            String key(nameLen + 1, '\0');
            String value(valueLen + 1, '\0');
            detail::check(kui_theme_manager_get_variable(raw_, name.data(), name.size(), index, key.data(),
                                                         key.size(), &nameLen, value.data(), value.size(),
                                                         &valueLen),
                          "ThemeManager::getTheme");
            key.resize(nameLen);
            value.resize(valueLen);
            lookup_.variables.emplace(std::move(key), std::move(value));
        }
        return &lookup_;
    }
    std::vector<String> themeNames() const {
        std::size_t count = 0;
        detail::check(kui_theme_manager_get_theme_count(raw_, &count), "ThemeManager::themeNames");
        std::vector<String> names;
        for (std::size_t index = 0; index < count; ++index) {
            names.push_back(detail::readString([this, index](char* buffer, std::size_t capacity, std::size_t* size) {
                return kui_theme_manager_get_theme_name(raw_, index, buffer, capacity, size);
            }, "ThemeManager::themeNames"));
        }
        return names;
    }
    static Theme lightTheme() { return builtin("light"); }
    static Theme darkTheme() { return builtin("dark"); }
    KuiThemeManager* cHandle() const noexcept { return raw_; }

private:
    static Theme builtin(const String& name) {
        ThemeManager manager;
        const Theme* theme = manager.getTheme(name);
        return theme ? *theme : Theme{name, {}};
    }
    KuiThemeManager* raw_ = nullptr;
    mutable Theme lookup_;
};

// --- VirtualList -----------------------------------------------------------------------
enum class ScrollUnit : u8 { Pixel = 0, Line = 1 };

struct VirtualListConfig {
    u32 totalItems = 0;
    f32 itemHeight = 32.0f;
    u32 overscan = 5;
    u32 recyclePoolMax = 0;
    using RenderFn = std::function<RefPtr<Element>(Document*, u32)>;
    RenderFn renderItem;
    using HeightFn = std::function<f32(u32 index)>;
    HeightFn heightForItem;
    using FocusChangeFn = std::function<void(i32 oldIndex, i32 newIndex)>;
    using ActivateFn = std::function<void(u32 index)>;
    FocusChangeFn onFocusChange;
    ActivateFn onActivate;
    bool enableKeyboard = false;
    ScrollUnit scrollUnit = ScrollUnit::Pixel;
};

namespace detail {

// What a component's C callbacks reach; freed by KUI through release_fn.
template <typename Config>
struct ComponentState {
    Config config;
    Document* document = nullptr;
    static void release(void* userData) noexcept { delete static_cast<ComponentState*>(userData); }
};

inline KuiCScrollUnit scrollUnitToC(ScrollUnit unit) {
    return unit == ScrollUnit::Line ? KUI_C_SCROLL_LINE : KUI_C_SCROLL_PIXEL;
}
inline ScrollUnit scrollUnitFromC(KuiCScrollUnit unit) {
    return unit == KUI_C_SCROLL_LINE ? ScrollUnit::Line : ScrollUnit::Pixel;
}

}  // namespace detail

class VirtualList {
public:
    VirtualList() = default;
    ~VirtualList() { reset(); }
    VirtualList(const VirtualList&) = delete;
    VirtualList& operator=(const VirtualList&) = delete;

    void setup(Element* container, const VirtualListConfig& config) {
        const auto docState = container ? container->wrapperState() : nullptr;
        if (!docState || !config.renderItem) throw Error(KUI_ERR_INVALID_ARG, "VirtualList::setup");
        reset();
        auto* state = new State{config, docState->owner};
        KuiCVirtualListConfig c{};
        c.total_items = config.totalItems;
        c.item_height = config.itemHeight;
        c.overscan = config.overscan;
        c.recycle_pool_max = config.recyclePoolMax;
        c.scroll_unit = detail::scrollUnitToC(config.scrollUnit);
        c.enable_keyboard = config.enableKeyboard ? 1 : 0;
        c.render_item = &VirtualList::render;
        if (config.heightForItem) c.height_for_item = &VirtualList::height;
        if (config.onFocusChange) c.on_focus_change = &VirtualList::focus;
        if (config.onActivate) c.on_activate = &VirtualList::activate;
        c.user_data = state;
        c.release_fn = &State::release;
        const KuiStatus status = kui_virtual_list_create(container->cHandle(), &c, &raw_);
        if (status != KUI_OK) delete state;
        detail::check(status, "VirtualList::setup");
        state_ = state;
        container_ = container;
    }
    void setTotalItems(u32 count) { call(kui_virtual_list_set_total_items(raw_, count), "VirtualList::setTotalItems"); }
    void insertItems(u32 index, u32 count) {
        call(kui_virtual_list_insert_items(raw_, index, count), "VirtualList::insertItems");
    }
    void removeItems(u32 index, u32 count) {
        call(kui_virtual_list_remove_items(raw_, index, count), "VirtualList::removeItems");
    }
    void setRenderItem(VirtualListConfig::RenderFn fn) {
        if (state_) state_->config.renderItem = std::move(fn);
        refresh();
    }
    void refresh() { call(kui_virtual_list_refresh(raw_), "VirtualList::refresh"); }
    void scrollToIndex(u32 index) { call(kui_virtual_list_scroll_to_index(raw_, index), "VirtualList::scrollToIndex"); }
    void scrollToTop() { call(kui_virtual_list_scroll_to_top(raw_), "VirtualList::scrollToTop"); }
    void scrollToBottom() { call(kui_virtual_list_scroll_to_bottom(raw_), "VirtualList::scrollToBottom"); }

    u32 visibleStart() const { return state().visible_start; }
    u32 visibleEnd() const { return state().visible_end; }
    u32 renderedCount() const { const auto s = state(); return s.visible_end - s.visible_start; }
    f32 totalHeight() const { return state().total_height; }
    u32 totalItems() const { return state().total_items; }
    f32 itemHeight() const { return state().item_height; }
    u32 recyclePoolSize() const { return state().recycle_pool_size; }
    bool isVariableHeight() const { return state_ && state_->config.heightForItem; }
    ScrollUnit scrollUnit() const { return detail::scrollUnitFromC(state().scroll_unit); }
    void setScrollUnit(ScrollUnit unit) {
        call(kui_virtual_list_set_scroll_unit(raw_, detail::scrollUnitToC(unit)), "VirtualList::setScrollUnit");
    }
    i32 focusedIndex() const { return state().focused_index; }
    void setFocusedIndex(i32 index) {
        call(kui_virtual_list_set_focused_index(raw_, index), "VirtualList::setFocusedIndex");
    }
    void moveFocus(i32 delta) { call(kui_virtual_list_move_focus(raw_, delta), "VirtualList::moveFocus"); }
    void focusFirst() { setFocusedIndex(0); }
    void focusLast() { setFocusedIndex(static_cast<i32>(totalItems()) - 1); }
    Element* container() const { return container_; }
    const VirtualListConfig& config() const { return state_ ? state_->config : empty_; }
    KuiVirtualList* cHandle() const noexcept { return raw_; }

private:
    using State = detail::ComponentState<VirtualListConfig>;

    void reset() {
        if (raw_) kui_virtual_list_release(raw_);
        raw_ = nullptr;
        state_ = nullptr;  // freed by KUI through release_fn
        container_ = nullptr;
    }
    void call(KuiStatus status, const char* context) const {
        if (raw_) detail::check(status, context);
    }
    KuiCVirtualListState state() const {
        KuiCVirtualListState s{};
        if (raw_) detail::check(kui_virtual_list_get_state(raw_, &s), "VirtualList::state");
        return s;
    }
    static KuiElement* render(KuiDocument*, uint32_t index, void* userData) noexcept {
        auto* state = static_cast<State*>(userData);
        try {
            const RefPtr<Element> element = state->config.renderItem(state->document, index);
            if (!element || kui_element_retain(element->cHandle()) != KUI_OK) return nullptr;
            return element->cHandle();  // KUI takes this reference
        } catch (...) {
            return nullptr;
        }
    }
    static float height(uint32_t index, void* userData) noexcept {
        try { return static_cast<State*>(userData)->config.heightForItem(index); }
        catch (...) { return 0.0f; }
    }
    static void focus(int32_t oldIndex, int32_t newIndex, void* userData) noexcept {
        try { static_cast<State*>(userData)->config.onFocusChange(oldIndex, newIndex); }
        catch (...) {}
    }
    static void activate(uint32_t index, void* userData) noexcept {
        try { static_cast<State*>(userData)->config.onActivate(index); }
        catch (...) {}
    }

    KuiVirtualList* raw_ = nullptr;
    State* state_ = nullptr;
    Element* container_ = nullptr;
    VirtualListConfig empty_;
};

// --- ListView --------------------------------------------------------------------------
struct ListViewItem {
    String text;
    String subtitle;
    String icon;
    bool disabled = false;
};

struct ListViewConfig {
    u32 itemCount = 0;
    f32 itemHeight = 36.0f;
    u32 overscan = 5;
    ScrollUnit scrollUnit = ScrollUnit::Pixel;
    enum SelectionMode : u8 { None, Single, Multi };
    SelectionMode selectionMode = Single;
    String itemBg = "transparent";
    String itemHoverBg = "rgba(255,255,255,0.06)";
    String itemSelectedBg = "#1d4ed8";
    String itemTextColor = "#e2e8f0";
    String itemSubColor = "#64748b";
    using ItemProvider = std::function<ListViewItem(u32 index)>;
    ItemProvider itemProvider;
    using SelectCallback = std::function<void(u32 index)>;
    using ActivateCallback = std::function<void(u32 index)>;
    SelectCallback onSelect;
    ActivateCallback onActivate;
};

class ListView {
public:
    ListView() = default;
    ~ListView() { reset(); }
    ListView(const ListView&) = delete;
    ListView& operator=(const ListView&) = delete;

    void setup(Element* container, const ListViewConfig& config) {
        if (!container || !config.itemProvider) throw Error(KUI_ERR_INVALID_ARG, "ListView::setup");
        reset();
        auto* state = new State{config, nullptr};
        KuiCListViewConfig c{};
        c.item_count = config.itemCount;
        c.item_height = config.itemHeight;
        c.overscan = config.overscan;
        c.scroll_unit = detail::scrollUnitToC(config.scrollUnit);
        c.selection_mode = static_cast<KuiCSelectionMode>(config.selectionMode);
        c.item_bg = state->config.itemBg.c_str();
        c.item_hover_bg = state->config.itemHoverBg.c_str();
        c.item_selected_bg = state->config.itemSelectedBg.c_str();
        c.item_text_color = state->config.itemTextColor.c_str();
        c.item_sub_color = state->config.itemSubColor.c_str();
        c.item_provider = &ListView::provide;
        if (config.onSelect) c.on_select = &ListView::selected;
        if (config.onActivate) c.on_activate = &ListView::activated;
        c.user_data = state;
        c.release_fn = &State::release;
        const KuiStatus status = kui_list_view_create(container->cHandle(), &c, &raw_);
        if (status != KUI_OK) delete state;
        detail::check(status, "ListView::setup");
    }
    void select(u32 index) { call(kui_list_view_select(raw_, index), "ListView::select"); }
    void deselect(u32 index) { call(kui_list_view_deselect(raw_, index), "ListView::deselect"); }
    void clearSelection() { call(kui_list_view_clear_selection(raw_), "ListView::clearSelection"); }
    // Adds [start, end), as in native.
    void selectRange(u32 start, u32 end) { call(kui_list_view_select_range(raw_, start, end), "ListView::selectRange"); }
    bool isSelected(u32 index) const {
        int value = 0;
        call(kui_list_view_is_selected(raw_, index, &value), "ListView::isSelected");
        return value != 0;
    }
    std::vector<u32> selectedIndices() const {
        std::size_t count = 0;
        if (!raw_) return {};
        detail::check(kui_list_view_get_selected(raw_, nullptr, 0, &count), "ListView::selectedIndices");
        std::vector<u32> result(count);
        if (count)
            detail::check(kui_list_view_get_selected(raw_, result.data(), result.size(), &count),
                          "ListView::selectedIndices");
        result.resize(count);
        return result;
    }
    i32 focusedIndex() const {
        int32_t value = -1;
        call(kui_list_view_get_focused_index(raw_, &value), "ListView::focusedIndex");
        return value;
    }
    void setItemCount(u32 count) { call(kui_list_view_set_item_count(raw_, count), "ListView::setItemCount"); }
    void refresh() { call(kui_list_view_refresh(raw_), "ListView::refresh"); }
    void scrollToIndex(u32 index) { call(kui_list_view_scroll_to_index(raw_, index), "ListView::scrollToIndex"); }
    void scrollToTop() { call(kui_list_view_scroll_to_top(raw_), "ListView::scrollToTop"); }
    void scrollToBottom() { call(kui_list_view_scroll_to_bottom(raw_), "ListView::scrollToBottom"); }
    void setScrollUnit(ScrollUnit unit) {
        call(kui_list_view_set_scroll_unit(raw_, detail::scrollUnitToC(unit)), "ListView::setScrollUnit");
    }
    ScrollUnit scrollUnit() const {
        KuiCVirtualListState s{};
        call(kui_list_view_get_state(raw_, &s), "ListView::scrollUnit");
        return detail::scrollUnitFromC(s.scroll_unit);
    }
    // The rows rendered now: [visibleStart, visibleEnd) of totalItems rows of itemHeight each.
    u32 visibleStart() const { return state().visible_start; }
    u32 visibleEnd() const { return state().visible_end; }
    u32 totalItems() const { return state().total_items; }
    f32 totalHeight() const { return state().total_height; }
    f32 itemHeight() const { return state().item_height; }
    KuiListView* cHandle() const noexcept { return raw_; }

private:
    using State = detail::ComponentState<ListViewConfig>;
    KuiCVirtualListState state() const {
        KuiCVirtualListState value{};
        call(kui_list_view_get_state(raw_, &value), "ListView::state");
        return value;
    }

    void reset() {
        if (raw_) kui_list_view_release(raw_);
        raw_ = nullptr;
    }
    void call(KuiStatus status, const char* context) const {
        if (raw_) detail::check(status, context);
    }
    // The item strings must outlive the return (KUI copies them right after).
    struct ItemBuffer {
        ListViewItem item;
    };
    static void provide(uint32_t index, KuiCListViewItem* out, void* userData) noexcept {
        auto* state = static_cast<State*>(userData);
        static thread_local ItemBuffer buffer;
        try {
            buffer.item = state->config.itemProvider(index);
            out->text = buffer.item.text.c_str();
            out->text_len = buffer.item.text.size();
            out->subtitle = buffer.item.subtitle.c_str();
            out->subtitle_len = buffer.item.subtitle.size();
            out->icon = buffer.item.icon.c_str();
            out->icon_len = buffer.item.icon.size();
            out->disabled = buffer.item.disabled ? 1 : 0;
        } catch (...) {}
    }
    static void selected(uint32_t index, void* userData) noexcept {
        try { static_cast<State*>(userData)->config.onSelect(index); }
        catch (...) {}
    }
    static void activated(uint32_t index, void* userData) noexcept {
        try { static_cast<State*>(userData)->config.onActivate(index); }
        catch (...) {}
    }

    KuiListView* raw_ = nullptr;
};

// --- TreeView --------------------------------------------------------------------------
struct TreeNodeData {
    String text;
    String icon;
    bool expanded = false;
    bool disabled = false;
    i32 parentIndex = -1;
    std::vector<u32> children;
    void* userData = nullptr;
};

struct TreeViewConfig {
    f32 itemHeight = 28.0f;
    f32 indentWidth = 20.0f;
    u32 overscan = 5;
    ScrollUnit scrollUnit = ScrollUnit::Pixel;
    String itemBg = "transparent";
    String itemHoverBg = "rgba(255,255,255,0.06)";
    String itemSelectedBg = "#1d4ed8";
    String itemTextColor = "#e2e8f0";
    bool multiSelect = false;
    using SelectCallback = std::function<void(u32 nodeIndex)>;
    using ToggleCallback = std::function<void(u32 nodeIndex, bool expanded)>;
    using MultiSelectCallback = std::function<void(const std::set<u32>& selected)>;
    SelectCallback onSelect;
    ToggleCallback onToggle;
    MultiSelectCallback onSelectionChanged;
};

class TreeView {
public:
    TreeView() { detail::check(kui_tree_view_create(&raw_), "TreeView"); }
    ~TreeView() { if (raw_) kui_tree_view_release(raw_); }
    TreeView(const TreeView&) = delete;
    TreeView& operator=(const TreeView&) = delete;

    u32 addNode(const String& text, const String& icon = "") {
        uint32_t index = 0;
        detail::check(kui_tree_view_add_node(raw_, text.data(), text.size(), icon.data(), icon.size(), &index),
                      "TreeView::addNode");
        userData_.resize(index + 1, nullptr);
        return index;
    }
    u32 addChild(u32 parentIndex, const String& text, const String& icon = "") {
        uint32_t index = 0;
        detail::check(kui_tree_view_add_child(raw_, parentIndex, text.data(), text.size(), icon.data(),
                                              icon.size(), &index),
                      "TreeView::addChild");
        userData_.resize(index + 1, nullptr);
        return index;
    }
    // A copy (const, so assigning to its members does not compile); change a node with setNode.
    const TreeNodeData node(u32 index) const {
        KuiCTreeNode raw{};
        detail::check(kui_tree_view_get_node(raw_, index, &raw), "TreeView::node");
        TreeNodeData data;
        data.text.assign(raw.text, raw.text_len);
        data.icon.assign(raw.icon, raw.icon_len);
        data.expanded = raw.expanded != 0;
        data.disabled = raw.disabled != 0;
        data.parentIndex = raw.parent;
        for (std::size_t position = 0; position < raw.child_count; ++position) {
            uint32_t child = 0;
            detail::check(kui_tree_view_get_node_child(raw_, index, position, &child), "TreeView::node");
            data.children.push_back(child);
        }
        data.userData = index < userData_.size() ? userData_[index] : nullptr;
        return data;
    }
    // Change only some fields of a node: an unset field keeps its current value.
    struct TreeNodeUpdate {
        Optional<String> text;
        Optional<String> icon;
        Optional<bool> disabled;
    };
    void updateNode(u32 index, const TreeNodeUpdate& update) {
        int disabled = 0;
        if (update.disabled) {
            disabled = *update.disabled ? 1 : 0;
        } else {
            KuiCTreeNode current{};
            detail::check(kui_tree_view_get_node(raw_, index, &current), "TreeView::updateNode");
            disabled = current.disabled;
        }
        detail::check(kui_tree_view_set_node(raw_, index, update.text ? update.text->data() : nullptr,
                                             update.text ? update.text->size() : 0,
                                             update.icon ? update.icon->data() : nullptr,
                                             update.icon ? update.icon->size() : 0, disabled),
                      "TreeView::updateNode");
    }
    // Text, icon, disabled and userData of a node (expand / collapse change `expanded`); call
    // refresh() to show the change. updateNode() changes only the fields you set.
    void setNode(u32 index, const TreeNodeData& data) {
        detail::check(kui_tree_view_set_node(raw_, index, data.text.data(), data.text.size(), data.icon.data(),
                                             data.icon.size(), data.disabled ? 1 : 0),
                      "TreeView::setNode");
        if (index < userData_.size()) userData_[index] = data.userData;
    }
    u32 nodeCount() const {
        uint32_t count = 0;
        detail::check(kui_tree_view_get_node_count(raw_, &count), "TreeView::nodeCount");
        return count;
    }
    void setup(Element* container, const TreeViewConfig& config) {
        if (!container) throw Error(KUI_ERR_INVALID_ARG, "TreeView::setup");
        auto* state = new State{config, nullptr};
        KuiCTreeViewConfig c{};
        c.item_height = config.itemHeight;
        c.indent_width = config.indentWidth;
        c.overscan = config.overscan;
        c.scroll_unit = detail::scrollUnitToC(config.scrollUnit);
        c.item_bg = state->config.itemBg.c_str();
        c.item_hover_bg = state->config.itemHoverBg.c_str();
        c.item_selected_bg = state->config.itemSelectedBg.c_str();
        c.item_text_color = state->config.itemTextColor.c_str();
        c.multi_select = config.multiSelect ? 1 : 0;
        if (config.onSelect) c.on_select = &TreeView::selected;
        if (config.onToggle) c.on_toggle = &TreeView::toggled;
        if (config.onSelectionChanged) c.on_selection_changed = &TreeView::selectionChanged;
        c.user_data = state;
        c.release_fn = &State::release;
        const KuiStatus status = kui_tree_view_setup(raw_, container->cHandle(), &c);
        if (status != KUI_OK) delete state;
        detail::check(status, "TreeView::setup");
    }
    void expand(u32 index) { detail::check(kui_tree_view_expand(raw_, index), "TreeView::expand"); }
    void collapse(u32 index) { detail::check(kui_tree_view_collapse(raw_, index), "TreeView::collapse"); }
    void toggle(u32 index) { detail::check(kui_tree_view_toggle(raw_, index), "TreeView::toggle"); }
    void expandAll() { detail::check(kui_tree_view_expand_all(raw_), "TreeView::expandAll"); }
    void collapseAll() { detail::check(kui_tree_view_collapse_all(raw_), "TreeView::collapseAll"); }
    void select(u32 index) { detail::check(kui_tree_view_select(raw_, index), "TreeView::select"); }
    void clearSelection() { detail::check(kui_tree_view_clear_selection(raw_), "TreeView::clearSelection"); }
    bool isSelected(u32 index) const {
        int value = 0;
        detail::check(kui_tree_view_is_selected(raw_, index, &value), "TreeView::isSelected");
        return value != 0;
    }
    i32 selectedIndex() const {
        int32_t value = -1;
        detail::check(kui_tree_view_get_selected_index(raw_, &value), "TreeView::selectedIndex");
        return value;
    }
    void toggleSelect(u32 index) { detail::check(kui_tree_view_toggle_select(raw_, index), "TreeView::toggleSelect"); }
    void selectRange(u32 index) { detail::check(kui_tree_view_select_range(raw_, index), "TreeView::selectRange"); }
    // A copy (native returns a reference to its set).
    std::set<u32> selectedIndices() const {
        std::size_t count = 0;
        detail::check(kui_tree_view_get_selected(raw_, nullptr, 0, &count), "TreeView::selectedIndices");
        std::vector<u32> values(count);
        if (count)
            detail::check(kui_tree_view_get_selected(raw_, values.data(), values.size(), &count),
                          "TreeView::selectedIndices");
        return std::set<u32>(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(count));
    }
    void selectAll() { detail::check(kui_tree_view_select_all(raw_), "TreeView::selectAll"); }
    u32 selectionCount() const { return static_cast<u32>(selectedIndices().size()); }
    void refresh() { detail::check(kui_tree_view_refresh(raw_), "TreeView::refresh"); }
    void scrollToNode(u32 index) { detail::check(kui_tree_view_scroll_to_node(raw_, index), "TreeView::scrollToNode"); }
    void setScrollUnit(ScrollUnit unit) {
        detail::check(kui_tree_view_set_scroll_unit(raw_, detail::scrollUnitToC(unit)), "TreeView::setScrollUnit");
    }
    u32 visibleRowCount() const {
        uint32_t count = 0;
        detail::check(kui_tree_view_get_visible_row_count(raw_, &count), "TreeView::visibleRowCount");
        return count;
    }
    KuiTreeView* cHandle() const noexcept { return raw_; }

private:
    using State = detail::ComponentState<TreeViewConfig>;

    static void selected(uint32_t index, void* userData) noexcept {
        try { static_cast<State*>(userData)->config.onSelect(index); }
        catch (...) {}
    }
    static void toggled(uint32_t index, int expanded, void* userData) noexcept {
        try { static_cast<State*>(userData)->config.onToggle(index, expanded != 0); }
        catch (...) {}
    }
    static void selectionChanged(const uint32_t* selected, std::size_t count, void* userData) noexcept {
        try {
            const std::set<u32> values(selected, selected + count);
            static_cast<State*>(userData)->config.onSelectionChanged(values);
        } catch (...) {}
    }

    KuiTreeView* raw_ = nullptr;
    std::vector<void*> userData_;
};

}  // namespace kui
KUI_CPP_HIDDEN_END
