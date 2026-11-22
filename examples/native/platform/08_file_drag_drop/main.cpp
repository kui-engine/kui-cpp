// ===========================================================================
// 08_file_drag_drop — External file drag & drop from the OS.
//
//   KUI-026  external file drag-drop integration
//   SDK 3.13.0
//
// Drag files from Explorer onto the window.  The OLE IDropTarget is registered
// automatically inside Window::create() (non-elevated processes), so no manual
// setup is needed — the window receives standard DOM drag events:
//
//   dragenter / dragover / dragleave / drop
//
// On "drop", the event is a DragEvent; cast it (typeTag() == Drag) to read
//   DragEvent::dataTransfer()  -> DataTransfer*
//   DataTransfer::files()      -> const std::vector<String>&  (UTF-8 paths)
//
// Source references:
//   include/kui/event/drag_event.h:62,95-110  (DataTransfer::files / DragEvent)
//   include/kui/event/event.h:32              (EventTypeTag::Drag)
//   src/event/drag_drop_manager.cpp:336-352   (synthesizes enter/over/drop/leave)
//   src/platform/ole_drop_target_win32.cpp:63-77 (DragQueryFileW -> UTF-8)
//
// The drop hit-tests the element under the cursor and falls back to the
// document root, so any element listening on "drop" works.  We listen on the
// document root via documentElement() so the whole window is a drop target.
// ===========================================================================

#include <kui/kui.h>
#include <kui/event/event.h>        // EventTypeTag
#include <kui/event/drag_event.h>   // DragEvent / DataTransfer
#include <string>

int main() {
    kui::Platform::init();
    auto window = kui::Window::create({
        .title  = "KUI — External File Drag & Drop (KUI-026)",
        .width  = 760,
        .height = 560,
    });

    auto* doc = window->document();
    doc->loadHTML(R"HTML(
<!DOCTYPE html><html><head><style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:"Segoe UI","Microsoft YaHei",sans-serif;background:#0f172a;color:#e2e8f0;padding:20px;height:100vh;display:flex;flex-direction:column}
h2{font-size:16px;margin-bottom:6px}
.hint{font-size:12px;color:#94a3b8;margin-bottom:14px}
#zone{flex:1;min-height:160px;border:2px dashed #475569;border-radius:12px;display:flex;align-items:center;justify-content:center;color:#64748b;font-size:15px;text-align:center;transition:background .15s,border-color .15s;margin-bottom:14px}
#zone.over{border-color:#3b82f6;background:#1e3a8a33;color:#93c5fd}
#list{background:#020617;border:1px solid #334155;border-radius:6px;padding:10px 12px;font:12px "Cascadia Code","Consolas",monospace;color:#cbd5e1;height:200px;overflow:auto}
.row{padding:3px 0;border-bottom:1px solid #1e293b;word-break:break-all}
.empty{color:#475569}
.count{color:#38bdf8}
</style></head><body>
<h2>外部文件拖放 (KUI-026)</h2>
<p class="hint">从资源管理器拖拽一个或多个文件/文件夹到下方区域,松手后显示完整路径(UTF-8)。</p>
<div id="zone">把文件拖到这里 ↓</div>
<div id="list"><span class="empty">尚未拖入任何文件</span></div>
</body></html>
    )HTML");

    auto* zone = doc->getElementById("zone");
    auto* list = doc->getElementById("list");
    auto* root = doc->documentElement();   // 整窗 drop 目标

    // dragover 必须 preventDefault 才允许 drop;同时高亮投放区。
    root->addEventListener("dragover", [zone](kui::Event& e) {
        e.preventDefault();
        zone->removeClass("over");
        zone->addClass("over");
    });
    root->addEventListener("dragenter", [zone](kui::Event& e) {
        e.preventDefault();
        zone->removeClass("over");
        zone->addClass("over");
    });
    root->addEventListener("dragleave", [zone](kui::Event&) {
        zone->removeClass("over");
    });

    // drop:取 DataTransfer::files() 完整路径列表。
    root->addEventListener("drop", [zone, list](kui::Event& e) {
        zone->removeClass("over");
        if (e.typeTag() != kui::EventTypeTag::Drag) return;
        auto* dt = static_cast<kui::DragEvent&>(e).dataTransfer();
        if (!dt) return;

        const auto& files = dt->files();
        if (files.empty()) return;
        e.preventDefault();

        std::string html =
            "<div class=\"row count\">收到 " + std::to_string(files.size()) +
            " 个项目:</div>";
        for (const auto& path : files) {
            // path 已是完整 UTF-8 路径字符串。
            html += "<div class=\"row\">" + std::string(path) + "</div>";
        }
        list->setInnerHTML(html);
    });

    return kui::Platform::run();
}
