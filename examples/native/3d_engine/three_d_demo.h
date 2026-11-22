#pragma once

#include "../common/demo_runtime.h"
#include "../extension/_find_extensions_dir.h"

#include <kui/extension/extension_manager.h>

#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace kui::examples {

struct ThreeDDemoInfo {
    const char* title;
    const char* description;
};

inline const ThreeDDemoInfo& threeDDemoInfo(int id) {
    static const ThreeDDemoInfo demos[] = {
        {"Minimal 3D Scene", "A live Filament scene with camera, light and procedural geometry."},
        {"Rotating Cube", "A box whose quaternion transform is updated every frame."},
        {"Lighting", "Directional and point lights illuminate multiple materials."},
        {"Texture Mapping / glTF", "Load a textured glTF/GLB asset with the file picker."},
        {"Camera Orbit", "The camera orbits a procedural scene in real time."},
        {"Multiple Objects", "A live scene containing boxes, spheres and cylinders."},
        {"Skybox", "Cycle real Filament solid-color skyboxes."},
        {"Shadows", "Directional-light shadow casters over a ground plane."},
        {"Particle System", "A CPU emitter animates a pool of small 3D particles."},
        {"Physics Basic", "Bullet gravity drives a visible sphere."},
        {"Rigid Body", "Impulse and velocity controls drive a linked renderable."},
        {"Collision Scene", "Dynamic bodies fall onto a static Bullet plane."},
        {"Entity Lifecycle", "Step through create, remove, re-add and destroy operations on a live entity."},
        {"Update System", "A timer updates a set of scene transforms as one system."},
        {"Render + Physics Components", "Renderable and rigid-body state are linked."},
        {"Scene Graph", "Parent-child transforms propagate through a three-level hierarchy."},
        {"Model Loading", "Load, transform and release a real glTF/GLB asset."},
        {"Skeletal Animation", "A four-joint procedural skeleton demonstrates hierarchical bone animation and key poses."},
        {"Post Processing", "Bloom, tone mapping and fog are applied by Filament."},
        {"Material Gallery", "Procedural materials under shared PBR lighting."},
        {"Terrain", "A generated height field made from scene primitives."},
        {"GPU Instancing", "A single Filament renderable draws an 8x8 box grid through InstanceBuffer."},
        {"HTML + 3D Overlay", "Interactive DOM controls are composited over live 3D."},
        {"Physics Raycast", "Bullet raycasts report the hit body, point, normal and ray fraction."},
        {"Constraint Playground", "Two visible rigid bodies are joined by a real Bullet hinge constraint."},
        {"Physics Debug Draw", "Bullet collision wireframes are collected through the debug-draw callback."},
    };
    const int index = id < 1 ? 0 : (id > 26 ? 25 : id - 1);
    return demos[index];
}

inline ExtensionValue extNumber(double value) { return ExtensionValue::number(value); }
inline ExtensionValue extInteger(std::int64_t value) { return ExtensionValue::integer(value); }
inline ExtensionValue extBoolean(bool value) { return ExtensionValue::boolean(value); }
inline ExtensionValue extString(std::string value) { return ExtensionValue::string(std::move(value)); }

inline ExtensionInvokeResult invoke3D(Document* doc, Element* view,
                                      const std::string& method,
                                      std::vector<ExtensionValue> args = {},
                                      std::size_t maxResults = 8) {
    return doc->extensionManager()->invoke(view, method, args, maxResults);
}

inline std::int64_t invoke3DInteger(Document* doc, Element* view,
                                    const std::string& method,
                                    std::vector<ExtensionValue> args) {
    auto result = invoke3D(doc, view, method, std::move(args), 1);
    if (!result || result.values.empty()) return 0;
    return result.values[0].intValue;
}

inline std::int64_t create3DPrimitive(Document* doc, Element* view,
                                      const char* method, float r, float g, float b) {
    const auto entity = invoke3DInteger(doc, view, method,
        {extNumber(r), extNumber(g), extNumber(b)});
    if (entity) invoke3D(doc, view, "addToScene", {extInteger(entity)});
    return entity;
}

inline void set3DTransform(Document* doc, Element* view, std::int64_t entity,
                           float x, float y, float z, float angle,
                           float sx = 1.0f, float sy = 1.0f, float sz = 1.0f) {
    const float half = angle * 0.5f;
    invoke3D(doc, view, "setEntityTransform", {
        extInteger(entity), extNumber(x), extNumber(y), extNumber(z),
        extNumber(0.0), extNumber(std::sin(half)), extNumber(0.0), extNumber(std::cos(half)),
        extNumber(sx), extNumber(sy), extNumber(sz),
    });
}

struct ThreeDDemoState {
    Document* document = nullptr;
    Element* view = nullptr;
    Element* status = nullptr;
    int demoId = 1;
    double angle = 0.0;
    bool bloom = false;
    int sky = 0;
    std::vector<std::int64_t> entities;
    std::vector<std::int64_t> bodies;
    std::int64_t constraint = 0;
    std::int64_t loadedAsset = 0;
    std::size_t lifecycleStep = 0;
    std::size_t particleStart = 0;
    std::size_t particleCount = 0;
    double particlePulse = 0.0;
    bool physicsDebug = false;
    bool physicsPaused = false;
    bool updateSystemPaused = false;
    std::uint64_t updateSystemTicks = 0;
    bool renderPhysicsLinked = true;
    bool sceneGrandchildOnChild = true;
    double modelYaw = 0.0;
    bool skeletonPaused = false;
    bool skeletonPoseDirty = true;
    int skeletonPose = 0;
    int skeletonSpeedIndex = 1;
    std::uint64_t skeletonTicks = 0;
    int postToneMapping = 2;
    bool postFog = true;
    std::string featureStatus;
    std::string lastStatusText;
    int statusUpdateCountdown = 0;
};

inline void setDemo17AssetTransform(const std::shared_ptr<ThreeDDemoState>& state) {
    if (!state->loadedAsset) return;
    const double halfYaw = state->modelYaw * .5;
    invoke3D(state->document, state->view, "setAssetTransform", {
        extInteger(state->loadedAsset),
        extNumber(.65), extNumber(-.55), extNumber(0.0),
        extNumber(0.0), extNumber(std::sin(halfYaw)),
        extNumber(0.0), extNumber(std::cos(halfYaw)),
        extNumber(.88), extNumber(.88), extNumber(.88),
    });
}

inline bool loadDemo17BundledAsset(const std::shared_ptr<ThreeDDemoState>& state) {
    const auto path = findRuntimeAsset("native_3d/stylized_tank.glb");
    if (path.empty()) {
        state->loadedAsset = 0;
        state->featureStatus = "Model loading failed: bundled stylized_tank.glb missing";
        state->statusUpdateCountdown = 0;
        return false;
    }
    if (state->loadedAsset) {
        invoke3D(state->document, state->view, "destroyAsset",
                 {extInteger(state->loadedAsset)});
        state->loadedAsset = 0;
    }
    state->loadedAsset = invoke3DInteger(state->document, state->view, "loadGltf",
                                         {extString(path.string())});
    state->modelYaw = 0.0;
    setDemo17AssetTransform(state);
    state->featureStatus = state->loadedAsset
        ? "Model loading: bundled textured tank loaded as asset " +
              std::to_string(state->loadedAsset)
        : "Model loading failed: glTF loader returned no asset";
    state->statusUpdateCountdown = 0;
    return state->loadedAsset != 0;
}

inline void resetCollisionBodies(const std::shared_ptr<ThreeDDemoState>& state,
                                 bool launchTowardCenter) {
    if (state->bodies.size() < 6) return;
    static constexpr float positions[5][3] = {
        {-2.35f, 3.15f,  0.00f}, { 2.35f, 3.15f,  0.00f},
        {-0.85f, 6.00f,  0.15f}, { 0.85f, 7.65f, -0.15f},
        { 0.00f, 9.45f,  0.00f},
    };
    static constexpr float launchVelocities[5][3] = {
        { 3.2f, 0.0f,  0.0f}, {-3.2f, 0.0f,  0.0f},
        { 0.9f, 0.0f, -0.2f}, {-0.9f, 0.0f,  0.2f},
        { 0.0f, 0.0f,  0.0f},
    };
    for (std::size_t i = 1; i < state->bodies.size() && i <= 5; ++i) {
        const auto body = state->bodies[i];
        const auto& p = positions[i - 1];
        invoke3D(state->document, state->view, "setLinearVelocity", {
            extInteger(body), extNumber(0.0), extNumber(0.0), extNumber(0.0),
        });
        invoke3D(state->document, state->view, "setAngularVelocity", {
            extInteger(body), extNumber(0.0), extNumber(0.0), extNumber(0.0),
        });
        invoke3D(state->document, state->view, "setRigidbodyTransform", {
            extInteger(body), extNumber(p[0]), extNumber(p[1]), extNumber(p[2]),
            extNumber(0.0), extNumber(0.0), extNumber(0.0), extNumber(1.0),
            extNumber(1.0), extNumber(1.0), extNumber(1.0),
        });
        if (launchTowardCenter) {
            const auto& v = launchVelocities[i - 1];
            invoke3D(state->document, state->view, "setLinearVelocity", {
                extInteger(body), extNumber(v[0]), extNumber(v[1]), extNumber(v[2]),
            });
        }
    }
    state->featureStatus = launchTowardCenter
        ? "Collision run restarted: opposing bodies launched"
        : "Collision stack reset: five bodies ready";
    state->statusUpdateCountdown = 0;
}

inline void prepareThreeDEnvironment(const std::shared_ptr<ThreeDDemoState>& state,
                                     float eyeX = 7.0f, float eyeY = 5.0f,
                                     float eyeZ = 9.0f) {
    invoke3D(state->document, state->view, "setCameraProjection",
             {extNumber(55.0), extNumber(0.1), extNumber(300.0)});
    invoke3D(state->document, state->view, "setCameraLookAt", {
        extNumber(eyeX), extNumber(eyeY), extNumber(eyeZ),
        extNumber(0.0), extNumber(0.5), extNumber(0.0),
    });
    invoke3D(state->document, state->view, "setSkyboxColor",
             {extNumber(0.02), extNumber(0.035), extNumber(0.08)});
    invoke3D(state->document, state->view, "createDirectionalLight", {
        extNumber(1.0), extNumber(0.95), extNumber(0.88), extNumber(110000.0),
        extNumber(-0.8), extNumber(-1.0), extNumber(-0.5),
    });
}

inline std::int64_t addThreeDEntity(const std::shared_ptr<ThreeDDemoState>& state,
                                    const char* method, float r, float g, float b,
                                    float x, float y, float z, float scale = 1.0f) {
    const auto entity = create3DPrimitive(state->document, state->view,
                                          method, r, g, b);
    if (!entity) return 0;
    state->entities.push_back(entity);
    set3DTransform(state->document, state->view, entity, x, y, z, 0.0f,
                   scale, scale, scale);
    return entity;
}

inline void setupThreeDDemo01(const std::shared_ptr<ThreeDDemoState>& state) {
    prepareThreeDEnvironment(state, 0.0f, 0.2f, 4.2f);
    const auto triangle = addThreeDEntity(state, "createTriangle",
                                          0.15f, 0.65f, 1.0f,
                                          0.0f, 0.0f, 0.0f, 1.35f);
    state->featureStatus = triangle
        ? "Renderable: one procedural indexed triangle"
        : "Triangle creation failed";
}

inline void setupThreeDDemo02(const std::shared_ptr<ThreeDDemoState>& state) {
    prepareThreeDEnvironment(state, 4.0f, 3.0f, 6.0f);
    addThreeDEntity(state, "createPlane", .18f, .22f, .3f,
                    0, -.75f, 0, 8.0f);
    addThreeDEntity(state, "createBox", .18f, .55f, .95f,
                    0, -.125f, 0, 1.25f);
    state->featureStatus = "Quaternion rotation: active on the cube";
}

inline void setupThreeDDemo03(const std::shared_ptr<ThreeDDemoState>& state) {
    prepareThreeDEnvironment(state, 5.5f, 2.8f, 7.2f);
    addThreeDEntity(state, "createPlane", .18f, .20f, .25f,
                    0, -.7f, 0, 9.0f);
    const auto matte = addThreeDEntity(state, "createSphere", .85f, .18f, .12f,
                                       -2.0f, -.05f, 0, 1.3f);
    const auto metal = addThreeDEntity(state, "createSphere", .18f, .55f, .95f,
                                       0.0f, -.05f, 0, 1.3f);
    const auto rough = addThreeDEntity(state, "createSphere", .25f, .9f, .4f,
                                       2.0f, -.05f, 0, 1.3f);
    invoke3D(state->document, state->view, "setMaterialProperties",
             {extInteger(matte), extNumber(.85), extNumber(.18), extNumber(.12),
              extNumber(.0), extNumber(.85)});
    invoke3D(state->document, state->view, "setMaterialProperties",
             {extInteger(metal), extNumber(.18), extNumber(.55), extNumber(.95),
              extNumber(.95), extNumber(.18)});
    invoke3D(state->document, state->view, "setMaterialProperties",
             {extInteger(rough), extNumber(.25), extNumber(.9), extNumber(.4),
              extNumber(.15), extNumber(.55)});
    invoke3D(state->document, state->view, "createPointLight", {
        extNumber(.25), extNumber(.5), extNumber(1.0), extNumber(65000.0),
        extNumber(-2.5), extNumber(3.5), extNumber(2.0), extNumber(12.0),
    });
    invoke3D(state->document, state->view, "createPointLight", {
        extNumber(1.0), extNumber(.3), extNumber(.15), extNumber(50000.0),
        extNumber(2.5), extNumber(2.5), extNumber(-1.0), extNumber(10.0),
    });
    state->featureStatus = "Lights: directional + 2 point / materials: 3";
}

inline void setupThreeDDemo04(const std::shared_ptr<ThreeDDemoState>& state) {
    prepareThreeDEnvironment(state, 4.5f, 3.2f, 6.5f);
    const auto path = findRuntimeAsset("native_3d/stylized_tank.glb");
    if (path.empty()) {
        state->featureStatus = "Bundled textured GLB is missing";
        return;
    }
    state->loadedAsset = invoke3DInteger(state->document, state->view, "loadGltf",
                                         {extString(path.string())});
    if (state->loadedAsset) {
        invoke3D(state->document, state->view, "setAssetTransform", {
            extInteger(state->loadedAsset), extNumber(0.0), extNumber(-.5), extNumber(0.0),
            extNumber(0.0), extNumber(0.0), extNumber(0.0), extNumber(1.0),
            extNumber(1.15), extNumber(1.15), extNumber(1.15),
        });
    }
    state->featureStatus = state->loadedAsset
        ? "Embedded-texture GLB loaded automatically"
        : "Bundled textured GLB failed to load";
}

inline void setupThreeDDemo05(const std::shared_ptr<ThreeDDemoState>& state) {
    prepareThreeDEnvironment(state, 7.2f, 3.6f, 0.0f);
    addThreeDEntity(state, "createPlane", .18f, .22f, .3f,
                    0, -.7f, 0, 8.0f);
    addThreeDEntity(state, "createSphere", .9f, .35f, .18f,
                    1.4f, .05f, 0, 1.5f);
    addThreeDEntity(state, "createBox", .2f, .55f, .95f,
                    -1.4f, -.075f, 0, 1.25f);
    state->featureStatus = "Camera orbit: continuous look-at update";
}

inline void setupThreeDDemo06(const std::shared_ptr<ThreeDDemoState>& state) {
    prepareThreeDEnvironment(state, 6.5f, 4.2f, 8.0f);
    addThreeDEntity(state, "createPlane", .15f, .18f, .24f,
                    0, -.7f, 0, 9.0f);
    static constexpr float colors[9][3] = {
        {.92f, .24f, .20f}, {.98f, .58f, .16f}, {.95f, .82f, .22f},
        {.22f, .78f, .42f}, {.12f, .72f, .78f}, {.18f, .48f, .95f},
        {.40f, .30f, .92f}, {.72f, .28f, .88f}, {.92f, .30f, .62f},
    };
    for (int i = 0; i < 9; ++i) {
        const char* shape = i % 3 == 0 ? "createBox" :
                            (i % 3 == 1 ? "createSphere" : "createCylinder");
        const float scale = .9f + .1f * static_cast<float>(i / 3);
        addThreeDEntity(state, shape,
                        colors[i][0], colors[i][1], colors[i][2],
                        -2.2f + static_cast<float>(i % 3) * 2.2f,
                        -.7f + scale * .5f,
                        -2.0f + static_cast<float>(i / 3) * 2.0f,
                        scale);
    }
    state->featureStatus = "Scene entities: 9 mixed primitives + ground";
}

inline void setupThreeDDemo07(const std::shared_ptr<ThreeDDemoState>& state) {
    prepareThreeDEnvironment(state, 5.0f, 2.6f, 7.0f);
    addThreeDEntity(state, "createPlane", .10f, .12f, .17f,
                    0, -.7f, 0, 6.5f);
    const auto mirror = addThreeDEntity(state, "createSphere", .95f, .45f, .18f,
                                         -1.5f, 0.0f, 0, 1.4f);
    const auto diffuse = addThreeDEntity(state, "createSphere", .18f, .62f, .98f,
                                          1.5f, 0.0f, 0, 1.4f);
    invoke3D(state->document, state->view, "setMaterialProperties",
             {extInteger(mirror), extNumber(.95), extNumber(.45), extNumber(.18),
              extNumber(1.0), extNumber(.12)});
    invoke3D(state->document, state->view, "setMaterialProperties",
             {extInteger(diffuse), extNumber(.18), extNumber(.62), extNumber(.98),
              extNumber(.0), extNumber(.72)});
    state->featureStatus = "Filament solid-color skybox: use Cycle sky";
}

inline void setupThreeDDemo08(const std::shared_ptr<ThreeDDemoState>& state) {
    prepareThreeDEnvironment(state, 5.5f, 3.3f, 7.5f);
    const auto ground = addThreeDEntity(state, "createBox", .16f, .18f, .22f,
                                        0, -.8f, 0, 1.0f);
    set3DTransform(state->document, state->view, ground,
                   0, -.8f, 0, 0, 8.5f, .2f, 8.5f);
    addThreeDEntity(state, "createBox", .18f, .58f, .98f,
                    -1.35f, -.05f, 0, 1.3f);
    addThreeDEntity(state, "createSphere", .98f, .32f, .16f,
                    1.35f, -.05f, 0, 1.3f);
    state->featureStatus =
        "Directional shadows: box + sphere / receiver: solid platform";
}

inline void setupThreeDDemo09(const std::shared_ptr<ThreeDDemoState>& state) {
    prepareThreeDEnvironment(state, 6.4f, 3.5f, 7.8f);
    const auto platform = addThreeDEntity(state, "createBox", .07f, .10f, .18f,
                                           0, -.82f, 0, 1.0f);
    set3DTransform(state->document, state->view, platform,
                   0, -.82f, 0, 0, 7.4f, .18f, 7.4f);
    const auto emitter = addThreeDEntity(state, "createCylinder", .10f, .72f, .96f,
                                          0, -.58f, 0, 1.0f);
    set3DTransform(state->document, state->view, emitter,
                   0, -.58f, 0, 0, .72f, .24f, .72f);
    state->particleStart = state->entities.size();
    static constexpr float colors[6][3] = {
        {.18f, .82f, 1.00f}, {.28f, .58f, 1.00f}, {.58f, .38f, 1.00f},
        {.94f, .34f, .82f}, {1.00f, .54f, .24f}, {.98f, .82f, .28f},
    };
    for (int i = 0; i < 48; ++i) {
        const auto& color = colors[i % 6];
        addThreeDEntity(state, "createSphere", color[0], color[1], color[2],
                        0, -.45f, 0, .12f);
    }
    state->particleCount = state->entities.size() - state->particleStart;
    state->featureStatus = "CPU emitter: 48 pooled particles / continuous recycle";
}

inline void addDefault3DScene(const std::shared_ptr<ThreeDDemoState>& state) {
    auto* doc = state->document;
    auto* view = state->view;
    const int id = state->demoId;

    switch (id) {
        case 1: setupThreeDDemo01(state); return;
        case 2: setupThreeDDemo02(state); return;
        case 3: setupThreeDDemo03(state); return;
        case 4: setupThreeDDemo04(state); return;
        case 5: setupThreeDDemo05(state); return;
        case 6: setupThreeDDemo06(state); return;
        case 7: setupThreeDDemo07(state); return;
        case 8: setupThreeDDemo08(state); return;
        case 9: setupThreeDDemo09(state); return;
        default: break;
    }

    invoke3D(doc, view, "setCameraProjection",
             {extNumber(55.0), extNumber(0.1), extNumber(300.0)});
    invoke3D(doc, view, "setCameraLookAt", {
        extNumber(7.0), extNumber(5.0), extNumber(9.0),
        extNumber(0.0), extNumber(0.5), extNumber(0.0),
    });
    if (id == 12) {
        invoke3D(doc, view, "setCameraLookAt", {
            extNumber(10.0), extNumber(7.0), extNumber(13.0),
            extNumber(0.0), extNumber(3.3), extNumber(0.0),
        });
    }
    if (id == 17) {
        invoke3D(doc, view, "setCameraLookAt", {
            extNumber(5.8), extNumber(3.7), extNumber(8.5),
            extNumber(.45), extNumber(0.25), extNumber(0.0),
        });
    }
    if (id == 18) {
        invoke3D(doc, view, "setCameraLookAt", {
            extNumber(5.8), extNumber(4.8), extNumber(8.2),
            extNumber(.2), extNumber(.35), extNumber(0.0),
        });
    }
    if (id == 19) {
        invoke3D(doc, view, "setCameraLookAt", {
            extNumber(7.2), extNumber(5.2), extNumber(10.8),
            extNumber(0.0), extNumber(.2), extNumber(-.8),
        });
    }
    invoke3D(doc, view, "setSkyboxColor",
             {extNumber(0.02), extNumber(0.035), extNumber(0.08)});
    invoke3D(doc, view, "createDirectionalLight", {
        extNumber(1.0), extNumber(0.95), extNumber(0.88), extNumber(110000.0),
        extNumber(-0.8), extNumber(-1.0), extNumber(-0.5),
    });

    auto add = [&](const char* method, float r, float g, float b,
                   float x, float y, float z, float scale = 1.0f) {
        const auto entity = create3DPrimitive(doc, view, method, r, g, b);
        if (!entity) return std::int64_t{0};
        state->entities.push_back(entity);
        set3DTransform(doc, view, entity, x, y, z, 0.0f, scale, scale, scale);
        return entity;
    };

    const bool useSolidPlatform =
        id == 10 || id == 11 || id == 12 || id == 13 || id == 14 || id == 15 ||
        id == 16 || id == 17 || id == 18 || id == 19;
    const auto ground = add(useSolidPlatform ? "createBox" : "createPlane",
                            0.16f, 0.20f, 0.30f,
                            0, useSolidPlatform ? -0.72f : -0.65f, 0,
                            useSolidPlatform ? 1.0f : 12.0f);
    if (useSolidPlatform && ground) {
        set3DTransform(doc, view, ground, 0, -0.72f, 0, 0,
                       8.5f, .14f, 8.5f);
    }
    (void)ground;

    if (id == 21) {
        for (int z = -4; z <= 4; ++z) {
            for (int x = -4; x <= 4; ++x) {
                const float h = 0.18f + 0.65f * (std::sin(x * 0.8f) * std::cos(z * 0.65f) + 1.0f);
                add("createBox", 0.16f, 0.42f + h * 0.15f, 0.22f,
                    x * 0.58f, -0.65f + h * 0.5f, z * 0.58f, 0.26f + h * 0.16f);
            }
        }
    } else if (id == 22) {
        const auto batch = invoke3DInteger(doc, view, "createInstancedBoxes", {
            extInteger(8), extInteger(8), extNumber(0.72),
            extNumber(0.25), extNumber(0.55), extNumber(0.95), extNumber(1.0),
        });
        if (batch) {
            state->entities.push_back(batch);
            state->featureStatus = "InstanceBuffer: 64 boxes / one renderable";
        }
    } else if (id == 12) {
        add("createBox",     0.18f, 0.62f, 1.00f, -2.35f, 3.15f,  0.00f);
        add("createSphere",  1.00f, 0.34f, 0.20f,  2.35f, 3.15f,  0.00f);
        add("createCylinder",0.22f, 0.88f, 0.48f, -0.85f, 6.00f,  0.15f);
        add("createSphere",  0.72f, 0.36f, 1.00f,  0.85f, 7.65f, -0.15f);
        add("createBox",     1.00f, 0.76f, 0.18f,  0.00f, 9.45f,  0.00f);
    } else if (id == 14) {
        static constexpr float colors[12][3] = {
            {.15f,.62f,1.00f}, {.22f,.82f,.88f}, {.28f,.88f,.48f},
            {.74f,.90f,.28f}, {1.00f,.72f,.20f}, {1.00f,.38f,.20f},
            {.96f,.28f,.52f}, {.78f,.30f,.92f}, {.48f,.36f,1.00f},
            {.24f,.48f,1.00f}, {.18f,.76f,.72f}, {.58f,.88f,.30f},
        };
        for (int i = 0; i < 12; ++i) {
            const char* shape = i % 3 == 0 ? "createBox" :
                                (i % 3 == 1 ? "createSphere" : "createCylinder");
            add(shape, colors[i][0], colors[i][1], colors[i][2],
                0.0f, 0.0f, 0.0f, .55f);
        }
        state->featureStatus = "Update system: one tick updates 12 transforms";
    } else if (id == 16) {
        // Three deliberately different shapes make each hierarchy level easy
        // to identify: blue root joint, orange child arm and green endpoint.
        add("createSphere", .12f, .48f, 1.00f, 0.0f, .10f, 0.0f, .85f);
        add("createBox", 1.00f, .40f, .12f, 0.0f, 0.0f, 0.0f, 1.0f);
        add("createSphere", .18f, .92f, .46f, 0.0f, 0.0f, 0.0f, 1.0f);
    } else if (id == 17) {
        loadDemo17BundledAsset(state);
    } else if (id == 18) {
        // Four spherical joints carry the hierarchy. Three box renderables
        // visualize the bone segments without becoming transform parents,
        // so non-uniform bone thickness never collapses downstream joints.
        add("createSphere", .16f, .52f, 1.00f, 0, 0, 0, 1.0f); // root [1]
        add("createBox",   .18f, .62f, 1.00f, 0, 0, 0, 1.0f); // upper [2]
        add("createSphere",1.00f, .42f, .14f, 0, 0, 0, 1.0f); // elbow [3]
        add("createBox",  1.00f, .48f, .14f, 0, 0, 0, 1.0f); // forearm [4]
        add("createSphere",.22f, .92f, .48f, 0, 0, 0, 1.0f); // wrist [5]
        add("createBox",   .28f, .88f, .52f, 0, 0, 0, 1.0f); // hand [6]
        add("createSphere",.78f, .36f, 1.00f, 0, 0, 0, 1.0f); // tip [7]
    } else if (id == 19) {
        static constexpr float colors[9][3] = {
            {1.00f,.18f,.08f}, {1.00f,.72f,.08f}, {.28f,1.00f,.30f},
            {.08f,.88f,1.00f}, {.20f,.42f,1.00f}, {.74f,.20f,1.00f},
            {1.00f,.16f,.58f}, {.92f,.92f,1.00f}, {.22f,1.00f,.74f},
        };
        for (int i = 0; i < 9; ++i) {
            const char* shape = i % 2 ? "createSphere" : "createBox";
            const float x = -2.2f + static_cast<float>(i % 3) * 2.2f;
            const float z = -3.0f + static_cast<float>(i / 3) * 2.25f;
            add(shape, colors[i][0], colors[i][1], colors[i][2],
                x, -.08f + static_cast<float>(i / 3) * .18f, z,
                .72f + static_cast<float>(i / 3) * .12f);
        }
        state->featureStatus =
            "Post stack: bloom + Filmic tone mapping + depth fog";
    } else {
        add("createBox", 0.22f, 0.52f, 0.95f, -2.1f, 0.15f, 0);
        add("createSphere", 0.95f, 0.32f, 0.20f, 0, 0.15f, 0);
        add("createCylinder", 0.20f, 0.82f, 0.42f, 2.1f, 0.15f, 0);
    }

    if (id == 13) {
        state->featureStatus = "Lifecycle: target attached / next action removes it";
    }


    if (id == 16 && state->entities.size() >= 4) {
        invoke3D(doc, view, "setEntityParent",
                 {extInteger(state->entities[2]), extInteger(state->entities[1])});
        invoke3D(doc, view, "setEntityParent",
                 {extInteger(state->entities[3]), extInteger(state->entities[2])});
        set3DTransform(doc, view, state->entities[1], 0.0f, .10f, 0.0f, 0.0f,
                       .85f, .85f, .85f);
        set3DTransform(doc, view, state->entities[2], 2.25f, .65f, 0.0f, .25f,
                       1.55f, .30f, .42f);
        // Counter the arm's non-uniform scale so the endpoint remains round.
        set3DTransform(doc, view, state->entities[3], .82f, 0.0f, 0.0f, 0.0f,
                       .25f, 1.28f, .92f);
        state->featureStatus =
            "Scene graph: blue root -> orange child -> green endpoint";
    } else if (id == 18 && state->entities.size() >= 8) {
        // Joint chain: root [1] -> elbow [3] -> wrist [5] -> tip [7].
        // Each bone box is a visual child of the joint that drives it.
        invoke3D(doc, view, "setEntityParent",
                 {extInteger(state->entities[2]), extInteger(state->entities[1])});
        invoke3D(doc, view, "setEntityParent",
                 {extInteger(state->entities[3]), extInteger(state->entities[1])});
        invoke3D(doc, view, "setEntityParent",
                 {extInteger(state->entities[4]), extInteger(state->entities[3])});
        invoke3D(doc, view, "setEntityParent",
                 {extInteger(state->entities[5]), extInteger(state->entities[3])});
        invoke3D(doc, view, "setEntityParent",
                 {extInteger(state->entities[6]), extInteger(state->entities[5])});
        invoke3D(doc, view, "setEntityParent",
                 {extInteger(state->entities[7]), extInteger(state->entities[5])});
        state->featureStatus =
            "Skeleton animation: 4 joints / 3 visible bones / live hierarchy";
    }

    if (id == 3 || id == 8 || id == 20) {
        invoke3D(doc, view, "createPointLight", {
            extNumber(0.3), extNumber(0.55), extNumber(1.0), extNumber(65000.0),
            extNumber(-2.5), extNumber(3.5), extNumber(2.0), extNumber(12.0),
        });
        invoke3D(doc, view, "createPointLight", {
            extNumber(1.0), extNumber(0.35), extNumber(0.2), extNumber(50000.0),
            extNumber(2.5), extNumber(2.5), extNumber(-1.0), extNumber(10.0),
        });
    }

    if (id == 10 || id == 11 || id == 12 || id == 15 || id == 24 || id == 25 || id == 26) {
        invoke3D(doc, view, "enablePhysics",
                 {extNumber(0.0), extNumber(-9.81), extNumber(0.0)});
        const auto floorBody = invoke3DInteger(doc, view, "createRigidbody", {
            extInteger(3), extNumber(0.0), extNumber(0.0), extNumber(-0.65), extNumber(0.0),
            extNumber(0.0), extNumber(1.0), extNumber(0.0),
        });
        if (floorBody) state->bodies.push_back(floorBody);
        if (id == 12 && state->entities.size() >= 6) {
            static constexpr int shapeTypes[5] = {0, 1, 2, 1, 0};
            static constexpr float positions[5][3] = {
                {-2.35f, 3.15f,  0.00f}, { 2.35f, 3.15f,  0.00f},
                {-0.85f, 6.00f,  0.15f}, { 0.85f, 7.65f, -0.15f},
                { 0.00f, 9.45f,  0.00f},
            };
            for (std::size_t i = 0; i < 5; ++i) {
                const float sx = shapeTypes[i] == 1 ? .55f : .50f;
                const float sy = shapeTypes[i] == 2 ? .82f : .50f;
                const auto body = invoke3DInteger(doc, view, "createRigidbody", {
                    extInteger(shapeTypes[i]), extNumber(1.0),
                    extNumber(positions[i][0]), extNumber(positions[i][1]),
                    extNumber(positions[i][2]), extNumber(sx), extNumber(sy),
                    extNumber(sx), extNumber(0.55), extNumber(0.35),
                });
                if (!body) continue;
                state->bodies.push_back(body);
                invoke3D(doc, view, "linkRigidbody",
                         {extInteger(body), extInteger(state->entities[i + 1])});
            }
            resetCollisionBodies(state, true);
        } else if (state->entities.size() > 2) {
            const auto body = invoke3DInteger(doc, view, "createRigidbody", {
                extInteger(1), extNumber(1.0), extNumber(0.0), extNumber(5.0), extNumber(0.0),
                extNumber(0.7), extNumber(0.7), extNumber(0.7),
                extNumber(0.5), extNumber(0.45),
            });
            if (body) {
                state->bodies.push_back(body);
                invoke3D(doc, view, "linkRigidbody", {extInteger(body), extInteger(state->entities[2])});
                if (id == 10) {
                    state->featureStatus =
                        "Bullet gravity: sphere settles on the static plane";
                }
                if (id == 11) {
                    invoke3D(doc, view, "setRigidbodyTransform", {
                        extInteger(body), extNumber(0.0), extNumber(0.05), extNumber(0.0),
                        extNumber(0.0), extNumber(0.0), extNumber(0.0), extNumber(1.0),
                        extNumber(1.0), extNumber(1.0), extNumber(1.0),
                    });
                    state->featureStatus =
                        "Rigid body ready: use impulse, velocity and brake controls";
                }
                if (id == 15) {
                    state->renderPhysicsLinked = true;
                    state->featureStatus =
                        "Components linked: render transform follows Bullet body";
                }
                if (id == 25) {
                    invoke3D(doc, view, "applyImpulse",
                             {extInteger(body), extNumber(2.5), extNumber(1.0), extNumber(0.0)});
                }
            }
        }


        if (id == 25 && state->entities.size() > 3) {
            const auto body = invoke3DInteger(doc, view, "createRigidbody", {
                extInteger(0), extNumber(1.0), extNumber(1.4), extNumber(3.4), extNumber(0.0),
                extNumber(0.55), extNumber(0.55), extNumber(0.55),
                extNumber(0.5), extNumber(0.25),
            });
            if (body) {
                state->bodies.push_back(body);
                invoke3D(doc, view, "linkRigidbody",
                         {extInteger(body), extInteger(state->entities[3])});
            }
            if (state->bodies.size() >= 3) {
                state->constraint = invoke3DInteger(doc, view, "createConstraint", {
                    extInteger(state->bodies[state->bodies.size() - 2]),
                    extInteger(state->bodies.back()), extInteger(0),
                    extNumber(0.65), extNumber(0.0), extNumber(0.0),
                    extNumber(-0.65), extNumber(0.0), extNumber(0.0),
                    extNumber(-0.75), extNumber(0.75),
                });
                state->featureStatus = state->constraint
                    ? "Bullet hinge constraint: active"
                    : "Bullet hinge constraint: creation failed";
            }
        }

        if (id == 26) {
            state->physicsDebug = true;
            invoke3D(doc, view, "setPhysicsDebugDraw", {extBoolean(true)});
            state->featureStatus = "Bullet debug draw: collecting collision wireframe lines";
        }
    }

    if (id == 19) {
        state->bloom = true;
        invoke3D(doc, view, "setBloomEnabled", {extBoolean(true)});
        invoke3D(doc, view, "configureBloom",
                 {extNumber(0.16), extInteger(6), extNumber(1.0)});
        invoke3D(doc, view, "setToneMapping", {extInteger(2)});
        invoke3D(doc, view, "configureFog", {
            extNumber(0.045), extNumber(1.0), extNumber(1.0),
            extNumber(0.12), extNumber(0.18), extNumber(0.32),
        });
        invoke3D(doc, view, "setFogEnabled", {extBoolean(true)});
    }
}

inline int runThreeDDemo(int demoId) {
    if (!Platform::init()) return 2;
    const auto& info = threeDDemoInfo(demoId);
    auto window = Window::create({.title = std::string("KUI — ") + info.title,
                                  .width = 1100, .height = 760});
    if (!window) return 3;
    if (std::getenv("KUI_EXAMPLES_DEVTOOLS")) {
        window->startDevTools(0);
    }
    auto* doc = window->document();

    const auto extensionDir = kui_examples_internal::findExtensionsDir("kui_ext_3d");
    doc->initExtensions(extensionDir);

    const std::string html = std::string(R"HTML(
<!DOCTYPE html><html><head><style>
*{box-sizing:border-box}body{margin:0;background:#07101f;color:#e2e8f0;font-family:"Segoe UI",sans-serif;overflow:hidden}
.stage{position:relative;width:100vw;height:100vh}kui-3d-view{display:block;width:100%;height:100%}
.panel{position:absolute;left:18px;top:18px;width:310px;padding:16px;background:rgba(2,6,23,.86);border:1px solid #1e3a5f;border-radius:12px;z-index:5}
h1{font-size:18px;margin:0 0 6px;color:#60a5fa}.desc{font-size:12px;color:#94a3b8;line-height:1.55}
#status{margin-top:12px;padding:9px;background:#0f172a;border-radius:7px;font:11px Consolas,monospace;color:#a7f3d0;white-space:pre-line}
.tools{display:flex;gap:7px;flex-wrap:wrap;margin-top:12px}.btn{padding:7px 10px;border-radius:6px;background:#2563eb;color:white;font-size:11px;cursor:pointer}.btn:hover{background:#1d4ed8}
.missing{color:#fca5a5}.axis{position:absolute;left:18px;bottom:18px;display:flex;gap:5px;z-index:5}.axis span{padding:4px 8px;border-radius:4px;font-size:10px;font-weight:700}.x{background:#7f1d1d}.y{background:#14532d}.z{background:#1e3a8a}
</style></head><body><div class="stage"><kui-3d-view id="view3d" width="1100" height="760"></kui-3d-view>
<div class="panel"><h1>)HTML") + info.title + R"HTML(</h1><div class="desc">)HTML" + info.description +
        R"HTML(</div><div id="status">Initializing extension...</div><div class="tools">
<div class="btn" id="add">Add object</div><div class="btn" id="bloom">Toggle bloom</div><div class="btn" id="sky">Cycle sky</div><div class="btn" id="camera">Reset camera</div><div class="btn" id="load">Load GLB...</div>
</div></div><div class="axis"><span class="x">X</span><span class="y">Y</span><span class="z">Z</span></div></div></body></html>)HTML";
    doc->loadHTML(html);

    auto* status = doc->getElementById("status");
    auto* view = doc->getElementById("view3d");
    if (!doc->extensionManager()->isExtensionLoaded("kui-3d-view")) {
        status->setClassName("missing");
        status->setTextContent("kui_ext_3d was not loaded.\nSearched: " + extensionDir +
                               "\nInstall the 3D extension and its Filament/Bullet runtime dependencies.");
        return Platform::run();
    }

    auto state = std::make_shared<ThreeDDemoState>();
    state->document = doc;
    state->view = view;
    state->status = status;
    state->demoId = demoId;
    addDefault3DScene(state);

    if (demoId == 13) doc->getElementById("add")->setTextContent("Remove target");
    if (demoId == 14) doc->getElementById("add")->setTextContent("Pause system");
    if (demoId == 15) {
        doc->getElementById("add")->setTextContent("Reset + link");
        doc->getElementById("bloom")->setTextContent("Detach render");
        doc->getElementById("sky")->setTextContent("Relink render");
        doc->getElementById("load")->setTextContent("Impulse body");
    }
    if (demoId == 9) doc->getElementById("add")->setTextContent("Pulse emitter");
    if (demoId == 10) doc->getElementById("add")->setTextContent("Drop again");
    if (demoId == 11) {
        doc->getElementById("add")->setTextContent("Impulse +X");
        doc->getElementById("bloom")->setTextContent("Velocity +Z");
        doc->getElementById("sky")->setTextContent("Brake");
        doc->getElementById("load")->setTextContent("Reset body");
    }
    if (demoId == 12) {
        doc->getElementById("add")->setTextContent("Restart collision");
        doc->getElementById("bloom")->setTextContent("Cross impulse");
        doc->getElementById("sky")->setTextContent("Pause physics");
        doc->getElementById("load")->setTextContent("Reset stack");
    }
    if (demoId == 16) {
        doc->getElementById("add")->setTextContent("Attach endpoint to root");
    }
    if (demoId == 17) {
        doc->getElementById("add")->setTextContent("Reload bundled GLB");
        doc->getElementById("bloom")->setTextContent("Rotate +45 deg");
        doc->getElementById("sky")->setTextContent("Unload model");
        doc->getElementById("camera")->setTextContent("Frame model");
        doc->getElementById("load")->setTextContent("Reset transform");
    }
    if (demoId == 18) {
        doc->getElementById("add")->setTextContent("Pause animation");
        doc->getElementById("bloom")->setTextContent("Next key pose");
        doc->getElementById("sky")->setTextContent("Speed 1.0x");
        doc->getElementById("camera")->setTextContent("Frame skeleton");
        doc->getElementById("load")->setTextContent("Reset skeleton");
    }
    if (demoId == 19) {
        doc->getElementById("add")->setTextContent("Bloom off");
        doc->getElementById("bloom")->setTextContent("Cycle tone map");
        doc->getElementById("sky")->setTextContent("Fog off");
        doc->getElementById("camera")->setTextContent("Frame post scene");
        doc->getElementById("load")->setTextContent("Reset effects");
    }
    if (demoId == 22) doc->getElementById("add")->setTextContent("Add instance batch");
    if (demoId == 24) doc->getElementById("add")->setTextContent("Cast ray");
    if (demoId == 25) doc->getElementById("add")->setTextContent("Impulse hinge");
    if (demoId == 26) doc->getElementById("add")->setTextContent("Toggle debug draw");

    doc->getElementById("add")->addEventListener("click", [state](Event&) {
        if (state->demoId == 9) {
            state->particlePulse = 1.0;
            state->featureStatus =
                "CPU emitter: radial pulse active / particles remain pooled";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 10 && state->bodies.size() >= 2) {
            const auto body = state->bodies.back();
            invoke3D(state->document, state->view, "setLinearVelocity", {
                extInteger(body), extNumber(0.0), extNumber(0.0), extNumber(0.0),
            });
            invoke3D(state->document, state->view, "setAngularVelocity", {
                extInteger(body), extNumber(0.0), extNumber(0.0), extNumber(0.0),
            });
            invoke3D(state->document, state->view, "setRigidbodyTransform", {
                extInteger(body), extNumber(0.0), extNumber(5.0), extNumber(0.0),
                extNumber(0.0), extNumber(0.0), extNumber(0.0), extNumber(1.0),
                extNumber(1.0), extNumber(1.0), extNumber(1.0),
            });
            state->featureStatus = "Bullet gravity: drop reset at y=5";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 11 && state->bodies.size() >= 2) {
            invoke3D(state->document, state->view, "applyImpulse", {
                extInteger(state->bodies.back()),
                extNumber(4.0), extNumber(2.4), extNumber(0.0),
            });
            state->featureStatus = "Rigid body: +X/+Y impulse applied";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 12) {
            state->physicsPaused = false;
            invoke3D(state->document, state->view, "setPhysicsPaused",
                     {extBoolean(false)});
            state->document->getElementById("sky")->setTextContent("Pause physics");
            resetCollisionBodies(state, true);
            return;
        }
        if (state->demoId == 13) {
            if (state->entities.empty()) return;
            const auto entity = state->entities.back();
            switch (state->lifecycleStep % 4) {
                case 0:
                    invoke3D(state->document, state->view, "removeFromScene",
                             {extInteger(entity)});
                    state->featureStatus = "Lifecycle: removed from scene (entity retained)";
                    state->document->getElementById("add")->setTextContent("Re-add target");
                    break;
                case 1:
                    invoke3D(state->document, state->view, "addToScene",
                             {extInteger(entity)});
                    state->featureStatus = "Lifecycle: re-added to scene";
                    state->document->getElementById("add")->setTextContent("Destroy target");
                    break;
                case 2:
                    invoke3D(state->document, state->view, "destroyEntity",
                             {extInteger(entity)});
                    state->entities.pop_back();
                    state->featureStatus = "Lifecycle: entity destroyed";
                    state->document->getElementById("add")->setTextContent("Create replacement");
                    break;
                default: {
                    const auto replacement = create3DPrimitive(
                        state->document, state->view, "createBox", .85f, .35f, .25f);
                    if (replacement) {
                        state->entities.push_back(replacement);
                        set3DTransform(state->document, state->view, replacement,
                                       2.1f, .25f, 0, 0, .55f, .55f, .55f);
                    }
                    state->featureStatus = "Lifecycle: replacement entity created";
                    state->document->getElementById("add")->setTextContent("Remove target");
                    break;
                }
            }
            ++state->lifecycleStep;
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 14) {
            state->updateSystemPaused = !state->updateSystemPaused;
            state->document->getElementById("add")->setTextContent(
                state->updateSystemPaused ? "Resume system" : "Pause system");
            state->featureStatus = state->updateSystemPaused
                ? "Update system: paused with transforms preserved"
                : "Update system: resumed / updating 12 transforms per tick";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 15 && state->bodies.size() >= 2 &&
            state->entities.size() >= 3) {
            const auto body = state->bodies.back();
            invoke3D(state->document, state->view, "setLinearVelocity", {
                extInteger(body), extNumber(0.0), extNumber(0.0), extNumber(0.0),
            });
            invoke3D(state->document, state->view, "setAngularVelocity", {
                extInteger(body), extNumber(0.0), extNumber(0.0), extNumber(0.0),
            });
            invoke3D(state->document, state->view, "setRigidbodyTransform", {
                extInteger(body), extNumber(0.0), extNumber(5.0), extNumber(0.0),
                extNumber(0.0), extNumber(0.0), extNumber(0.0), extNumber(1.0),
                extNumber(1.0), extNumber(1.0), extNumber(1.0),
            });
            invoke3D(state->document, state->view, "linkRigidbody",
                     {extInteger(body), extInteger(state->entities[2])});
            state->renderPhysicsLinked = true;
            state->featureStatus = "Components reset + linked at body y=5";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 16 && state->entities.size() >= 4) {
            state->sceneGrandchildOnChild = !state->sceneGrandchildOnChild;
            const auto parent = state->sceneGrandchildOnChild
                ? state->entities[2] : state->entities[1];
            invoke3D(state->document, state->view, "setEntityParent",
                     {extInteger(state->entities[3]), extInteger(parent)});
            state->document->getElementById("add")->setTextContent(
                state->sceneGrandchildOnChild
                    ? "Attach endpoint to root" : "Attach endpoint to child");
            state->featureStatus = state->sceneGrandchildOnChild
                ? "Scene graph: endpoint restored under orange child"
                : "Scene graph: endpoint reparented directly under blue root";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 17) {
            if (loadDemo17BundledAsset(state)) {
                state->document->getElementById("add")->setTextContent(
                    "Reload bundled GLB");
            }
            return;
        }
        if (state->demoId == 18) {
            state->skeletonPaused = !state->skeletonPaused;
            state->skeletonPoseDirty = true;
            state->document->getElementById("add")->setTextContent(
                state->skeletonPaused ? "Resume animation" : "Pause animation");
            state->featureStatus = state->skeletonPaused
                ? "Skeleton animation paused: current joint pose preserved"
                : "Skeleton animation resumed: hierarchical motion active";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 19) {
            state->bloom = !state->bloom;
            invoke3D(state->document, state->view, "setBloomEnabled",
                     {extBoolean(state->bloom)});
            state->document->getElementById("add")->setTextContent(
                state->bloom ? "Bloom off" : "Bloom on");
            state->featureStatus = state->bloom
                ? "Post stack: bloom enabled" : "Post stack: bloom disabled";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 24) {
            auto hit = invoke3D(state->document, state->view, "raycast", {
                extNumber(0.0), extNumber(8.0), extNumber(0.0),
                extNumber(0.0), extNumber(-2.0), extNumber(0.0),
            }, 9);
            state->featureStatus = hit && hit.values.size() == 9 && hit.values[0].boolValue
                ? "Raycast: HIT body " + std::to_string(hit.values[1].intValue)
                : "Raycast: MISS";
            return;
        }
        if (state->demoId == 25 && state->bodies.size() >= 3) {
            invoke3D(state->document, state->view, "applyImpulse", {
                extInteger(state->bodies.back()), extNumber(0.0),
                extNumber(0.0), extNumber(2.5),
            });
            state->featureStatus = "Constraint impulse applied; hinge remains active";
            return;
        }
        if (state->demoId == 26) {
            state->physicsDebug = !state->physicsDebug;
            invoke3D(state->document, state->view, "setPhysicsDebugDraw",
                     {extBoolean(state->physicsDebug)});
            state->featureStatus = state->physicsDebug
                ? "Bullet debug draw: enabled" : "Bullet debug draw: disabled";
            return;
        }
        const auto index = state->entities.size();
        if (state->demoId == 22) {
            const auto entity = invoke3DInteger(state->document, state->view,
                                                "createInstancedBoxes", {
                extInteger(6), extInteger(6), extNumber(.55),
                extNumber(.85), extNumber(.35), extNumber(.25), extNumber(1.0),
            });
            if (entity) state->entities.push_back(entity);
            state->featureStatus = "Second InstanceBuffer batch created (36 instances)";
            return;
        }
        const auto entity = create3DPrimitive(state->document, state->view,
            index % 2 ? "createSphere" : "createBox",
            0.25f + static_cast<float>(index % 5) * 0.13f, 0.55f, 0.9f);
        if (entity) {
            state->entities.push_back(entity);
            if (state->demoId == 6) {
                const auto slot = index >= 10 ? index - 10 : 0;
                constexpr float scale = .8f;
                set3DTransform(state->document, state->view, entity,
                               -2.2f + static_cast<float>(slot % 3) * 2.2f,
                               -.7f + scale * .5f,
                               3.4f + static_cast<float>(slot / 3) * 1.8f,
                               0.0f, scale, scale, scale);
                state->featureStatus = "Scene extended on the ground: " +
                    std::to_string(state->entities.size() - 1) + " mixed primitives";
            } else {
                set3DTransform(state->document, state->view, entity,
                               static_cast<float>(index % 7) - 3.0f, 1.2f,
                               static_cast<float>((index / 7) % 5) - 2.0f,
                               0.0f, 0.45f, 0.45f, 0.45f);
            }
        }
    });
    doc->getElementById("bloom")->addEventListener("click", [state](Event&) {
        if (state->demoId == 11 && state->bodies.size() >= 2) {
            invoke3D(state->document, state->view, "setLinearVelocity", {
                extInteger(state->bodies.back()),
                extNumber(0.0), extNumber(3.2), extNumber(3.5),
            });
            state->featureStatus = "Rigid body: velocity set to (0, 3.2, 3.5)";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 12 && state->bodies.size() >= 6) {
            state->physicsPaused = false;
            invoke3D(state->document, state->view, "setPhysicsPaused",
                     {extBoolean(false)});
            state->document->getElementById("sky")->setTextContent("Pause physics");
            for (std::size_t i = 1; i < state->bodies.size(); ++i) {
                const float x = i % 2 ? 3.4f : -3.4f;
                const float z = i % 3 ? 1.05f : -1.05f;
                invoke3D(state->document, state->view, "applyImpulse", {
                    extInteger(state->bodies[i]), extNumber(x),
                    extNumber(5.5 + .35 * static_cast<double>(i)), extNumber(z),
                });
            }
            state->featureStatus = "Cross impulse: five bodies launched into new contacts";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 15 && state->bodies.size() >= 2) {
            invoke3D(state->document, state->view, "linkRigidbody",
                     {extInteger(state->bodies.back()), extInteger(0)});
            state->renderPhysicsLinked = false;
            state->featureStatus =
                "Render component detached; Bullet body continues invisibly";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 17) {
            if (state->loadedAsset) {
                state->modelYaw += .7853981633974483;
                setDemo17AssetTransform(state);
                state->featureStatus =
                    "Model transform: rotated +45 degrees around Y";
            } else {
                state->featureStatus =
                    "Model transform unavailable: load the bundled GLB first";
            }
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 18) {
            state->skeletonPaused = true;
            state->skeletonPose = (state->skeletonPose + 1) % 3;
            state->skeletonPoseDirty = true;
            state->document->getElementById("add")->setTextContent("Resume animation");
            state->featureStatus = "Skeleton key pose " +
                std::to_string(state->skeletonPose + 1) +
                ": joint-local rotations applied";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 19) {
            state->postToneMapping = (state->postToneMapping + 1) % 4;
            invoke3D(state->document, state->view, "setToneMapping",
                     {extInteger(state->postToneMapping)});
            static const char* names[] = {"Linear", "ACES", "Filmic", "AgX"};
            state->featureStatus = "Tone mapping: " +
                std::string(names[state->postToneMapping]);
            state->statusUpdateCountdown = 0;
            return;
        }
        state->bloom = !state->bloom;
        invoke3D(state->document, state->view, "setBloomEnabled", {extBoolean(state->bloom)});
    });
    doc->getElementById("sky")->addEventListener("click", [state](Event&) {
        if (state->demoId == 11 && state->bodies.size() >= 2) {
            invoke3D(state->document, state->view, "setLinearVelocity", {
                extInteger(state->bodies.back()),
                extNumber(0.0), extNumber(0.0), extNumber(0.0),
            });
            invoke3D(state->document, state->view, "setAngularVelocity", {
                extInteger(state->bodies.back()),
                extNumber(0.0), extNumber(0.0), extNumber(0.0),
            });
            state->featureStatus = "Rigid body: linear and angular velocity cleared";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 12 && state->bodies.size() >= 6) {
            state->physicsPaused = !state->physicsPaused;
            invoke3D(state->document, state->view, "setPhysicsPaused",
                     {extBoolean(state->physicsPaused)});
            state->document->getElementById("sky")->setTextContent(
                state->physicsPaused ? "Resume physics" : "Pause physics");
            state->featureStatus = state->physicsPaused
                ? "Collision simulation paused: transforms and velocities preserved"
                : "Collision simulation resumed";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 15 && state->bodies.size() >= 2 &&
            state->entities.size() >= 3) {
            invoke3D(state->document, state->view, "linkRigidbody", {
                extInteger(state->bodies.back()), extInteger(state->entities[2]),
            });
            state->renderPhysicsLinked = true;
            state->featureStatus =
                "Render component relinked; transform follows Bullet again";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 17) {
            if (state->loadedAsset) {
                invoke3D(state->document, state->view, "destroyAsset",
                         {extInteger(state->loadedAsset)});
                state->loadedAsset = 0;
            }
            state->modelYaw = 0.0;
            state->document->getElementById("add")->setTextContent("Load bundled GLB");
            state->featureStatus = "Model loading: asset released from Filament scene";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 18) {
            state->skeletonSpeedIndex = (state->skeletonSpeedIndex + 1) % 3;
            static const char* labels[] = {"Speed 0.5x", "Speed 1.0x", "Speed 1.75x"};
            state->document->getElementById("sky")->setTextContent(
                labels[state->skeletonSpeedIndex]);
            state->featureStatus = "Skeleton playback speed: " +
                std::string(state->skeletonSpeedIndex == 0 ? "0.5x" :
                            (state->skeletonSpeedIndex == 1 ? "1.0x" : "1.75x"));
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 19) {
            state->postFog = !state->postFog;
            invoke3D(state->document, state->view, "setFogEnabled",
                     {extBoolean(state->postFog)});
            state->document->getElementById("sky")->setTextContent(
                state->postFog ? "Fog off" : "Fog on");
            state->featureStatus = state->postFog
                ? "Depth fog: enabled" : "Depth fog: disabled";
            state->statusUpdateCountdown = 0;
            return;
        }
        static const float colors[][3] = {{.02f,.035f,.08f},{.18f,.035f,.025f},{.02f,.13f,.08f},{.18f,.12f,.03f}};
        state->sky = (state->sky + 1) % 4;
        const auto& c = colors[state->sky];
        invoke3D(state->document, state->view, "setSkyboxColor",
                 {extNumber(c[0]), extNumber(c[1]), extNumber(c[2])});
    });
    doc->getElementById("camera")->addEventListener("click", [state](Event&) {
        if (state->demoId == 17) {
            invoke3D(state->document, state->view, "setCameraLookAt", {
                extNumber(5.8), extNumber(3.7), extNumber(8.5),
                extNumber(.45), extNumber(0.25), extNumber(0.0),
            });
            state->featureStatus = "Model loading: camera framed on bundled asset";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 18) {
            invoke3D(state->document, state->view, "setCameraLookAt", {
                extNumber(5.8), extNumber(4.8), extNumber(8.2),
                extNumber(.2), extNumber(.35), extNumber(0.0),
            });
            state->featureStatus = "Skeleton camera: full joint chain framed";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 19) {
            invoke3D(state->document, state->view, "setCameraLookAt", {
                extNumber(7.2), extNumber(5.2), extNumber(10.8),
                extNumber(0.0), extNumber(.2), extNumber(-.8),
            });
            state->featureStatus = "Post scene camera: near/mid/far rows framed";
            state->statusUpdateCountdown = 0;
            return;
        }
        invoke3D(state->document, state->view, "setCameraLookAt", {
            extNumber(7.0), extNumber(5.0), extNumber(9.0),
            extNumber(0.0), extNumber(0.5), extNumber(0.0),
        });
    });
    doc->getElementById("load")->addEventListener("click", [state](Event&) {
        if (state->demoId == 11 && state->bodies.size() >= 2) {
            const auto body = state->bodies.back();
            invoke3D(state->document, state->view, "setLinearVelocity", {
                extInteger(body), extNumber(0.0), extNumber(0.0), extNumber(0.0),
            });
            invoke3D(state->document, state->view, "setAngularVelocity", {
                extInteger(body), extNumber(0.0), extNumber(0.0), extNumber(0.0),
            });
            invoke3D(state->document, state->view, "setRigidbodyTransform", {
                extInteger(body), extNumber(0.0), extNumber(0.05), extNumber(0.0),
                extNumber(0.0), extNumber(0.0), extNumber(0.0), extNumber(1.0),
                extNumber(1.0), extNumber(1.0), extNumber(1.0),
            });
            state->featureStatus = "Rigid body: reset on the platform";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 12) {
            state->physicsPaused = true;
            invoke3D(state->document, state->view, "setPhysicsPaused",
                     {extBoolean(true)});
            state->document->getElementById("sky")->setTextContent("Resume physics");
            resetCollisionBodies(state, false);
            return;
        }
        if (state->demoId == 15 && state->bodies.size() >= 2) {
            invoke3D(state->document, state->view, "applyImpulse", {
                extInteger(state->bodies.back()),
                extNumber(2.2), extNumber(4.5), extNumber(0.8),
            });
            state->featureStatus = "Physics component: impulse applied to body";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 17) {
            if (!state->loadedAsset) {
                loadDemo17BundledAsset(state);
                state->document->getElementById("add")->setTextContent(
                    "Reload bundled GLB");
            } else {
                state->modelYaw = 0.0;
                setDemo17AssetTransform(state);
                state->featureStatus =
                    "Model transform: reset to showcase position, scale 0.88, yaw 0";
                state->statusUpdateCountdown = 0;
            }
            return;
        }
        if (state->demoId == 18) {
            state->angle = 0.0;
            state->skeletonPaused = false;
            state->skeletonPose = 0;
            state->skeletonSpeedIndex = 1;
            state->skeletonTicks = 0;
            state->skeletonPoseDirty = true;
            state->document->getElementById("add")->setTextContent("Pause animation");
            state->document->getElementById("sky")->setTextContent("Speed 1.0x");
            state->featureStatus =
                "Skeleton reset: bind pose origin / playback 1.0x";
            state->statusUpdateCountdown = 0;
            return;
        }
        if (state->demoId == 19) {
            state->bloom = true;
            state->postToneMapping = 2;
            state->postFog = true;
            invoke3D(state->document, state->view, "setBloomEnabled",
                     {extBoolean(true)});
            invoke3D(state->document, state->view, "configureBloom",
                     {extNumber(.16), extInteger(6), extNumber(1.0)});
            invoke3D(state->document, state->view, "setToneMapping",
                     {extInteger(2)});
            invoke3D(state->document, state->view, "configureFog", {
                extNumber(.045), extNumber(1.0), extNumber(1.0),
                extNumber(.12), extNumber(.18), extNumber(.32),
            });
            invoke3D(state->document, state->view, "setFogEnabled",
                     {extBoolean(true)});
            state->document->getElementById("add")->setTextContent("Bloom off");
            state->document->getElementById("sky")->setTextContent("Fog off");
            state->featureStatus =
                "Post stack reset: bloom + Filmic + fog enabled";
            state->statusUpdateCountdown = 0;
            return;
        }
        Platform::FileDialogOptions options("Open glTF/GLB model", "", {"*.glb", "*.gltf"}, false);
        const auto path = Platform::openFileDialog(options);
        if (!path) return;
        const auto handle = invoke3DInteger(state->document, state->view, "loadGltf",
                                             {extString(*path)});
        if (!handle) {
            state->featureStatus = "Model load failed: " + *path;
            return;
        }
        state->loadedAsset = handle;
        auto count = invoke3D(state->document, state->view, "getAssetAnimationCount",
                              {extInteger(handle)}, 1);
        const auto animationCount = count && !count.values.empty()
            ? count.values[0].intValue : 0;
        if (animationCount > 0) {
            invoke3D(state->document, state->view, "playAssetAnimation",
                     {extInteger(handle), extInteger(0)});
        }
        state->featureStatus = "Loaded asset " + std::to_string(handle) +
            " / animation clips: " + std::to_string(animationCount) +
            (animationCount > 0 ? " / clip 0 playing" : "");
    });

    Platform::setInterval([state]() {
        if ((state->demoId != 14 || !state->updateSystemPaused) &&
            (state->demoId != 18 || !state->skeletonPaused)) {
            static constexpr double skeletonSpeeds[] = {.5, 1.0, 1.75};
            state->angle += 0.018 * (state->demoId == 18
                ? skeletonSpeeds[state->skeletonSpeedIndex] : 1.0);
        }
        if (state->demoId == 5) {
            const double a = state->angle * 0.65;
            invoke3D(state->document, state->view, "setCameraLookAt", {
                extNumber(std::cos(a) * 7.2), extNumber(3.6), extNumber(std::sin(a) * 7.2),
                extNumber(0.0), extNumber(0.5), extNumber(0.0),
            });
        } else if (state->demoId == 9 && state->particleCount > 0) {
            const bool pulseWasActive = state->particlePulse > 0.0;
            state->particlePulse = std::max(0.0, state->particlePulse - .018);
            if (pulseWasActive && state->particlePulse == 0.0) {
                state->featureStatus =
                    "CPU emitter: 48 pooled particles / continuous recycle";
                state->statusUpdateCountdown = 0;
            }
            constexpr float goldenAngle = 2.39996323f;
            constexpr float twoPi = 6.28318531f;
            const float pulse = static_cast<float>(state->particlePulse);
            for (std::size_t slot = 0; slot < state->particleCount; ++slot) {
                const float phase = std::fmod(
                    static_cast<float>(state->angle * .42) +
                    static_cast<float>(slot) /
                        static_cast<float>(state->particleCount), 1.0f);
                const float life = std::sin(phase * 3.14159265f);
                const float lane = .72f + static_cast<float>(slot % 7) * .055f;
                const float radius = (.18f + life * lane) * (1.0f + pulse * .8f);
                const float azimuth = static_cast<float>(slot) * goldenAngle +
                                      phase * twoPi * 2.25f;
                const float y = -.42f + phase * 3.85f +
                                std::sin(azimuth * .5f) * .12f;
                const float scale = (.105f + life * .13f) *
                                    (1.0f + pulse * .35f);
                set3DTransform(state->document, state->view,
                               state->entities[state->particleStart + slot],
                               std::cos(azimuth) * radius, y,
                               std::sin(azimuth) * radius, azimuth,
                               scale, scale, scale);
            }
        } else if (state->demoId == 2 && state->entities.size() >= 2) {
            // This example is specifically an in-place rotation demo. Keep
            // the cube centred above its receiver plane and preserve the
            // setup size; orbiting and shrinking it made the named feature
            // difficult to see and no longer demonstrated the advertised
            // transform.
            set3DTransform(state->document, state->view, state->entities[1],
                           0.0f, -0.125f, 0.0f,
                           static_cast<float>(state->angle),
                           1.25f, 1.25f, 1.25f);
        } else if (state->demoId == 14 && state->entities.size() >= 13 &&
                   !state->updateSystemPaused) {
            constexpr float twoPi = 6.28318531f;
            const std::size_t count = state->entities.size() - 1;
            for (std::size_t i = 1; i < state->entities.size(); ++i) {
                const std::size_t slot = i - 1;
                const float band = static_cast<float>(slot % 3);
                const float phase = static_cast<float>(state->angle) *
                    (.72f + band * .13f) +
                    static_cast<float>(slot) * twoPi /
                    static_cast<float>(count);
                const float direction = slot % 2 ? -1.0f : 1.0f;
                const float radius = 1.45f + band * .78f;
                const float y = -.12f + band * .42f +
                    std::sin(phase * 2.0f) * .24f;
                const float scale = .48f + band * .08f;
                set3DTransform(state->document, state->view, state->entities[i],
                               std::cos(phase) * radius, y,
                               std::sin(phase) * radius, phase * direction,
                               scale, scale, scale);
            }
            ++state->updateSystemTicks;
        } else if (state->demoId == 16 && state->entities.size() >= 4) {
            const float rootAngle = static_cast<float>(state->angle * .55);
            set3DTransform(state->document, state->view, state->entities[1],
                           0.0f, .10f, 0.0f, rootAngle,
                           .85f, .85f, .85f);
            const float childAngle =
                std::sin(static_cast<float>(state->angle * 1.35)) * .38f;
            set3DTransform(state->document, state->view, state->entities[2],
                           2.25f, .65f, 0.0f, childAngle,
                           1.55f, .30f, .42f);
            if (state->sceneGrandchildOnChild) {
                set3DTransform(state->document, state->view, state->entities[3],
                               .82f, 0.0f, 0.0f, 0.0f,
                               .25f, 1.28f, .92f);
            } else {
                set3DTransform(state->document, state->view, state->entities[3],
                               -2.25f, 1.25f, 0.0f, -rootAngle * .45f,
                               .58f, .58f, .58f);
            }
        } else if (state->demoId == 18 && state->entities.size() >= 8 &&
                   (!state->skeletonPaused || state->skeletonPoseDirty)) {
            static constexpr float poses[3][3] = {
                {-.35f, .72f, -.48f}, {.22f, -1.05f, .88f},
                {.62f, .35f, -1.12f},
            };
            const float rootAngle = state->skeletonPaused
                ? poses[state->skeletonPose][0]
                : std::sin(static_cast<float>(state->angle * .72)) * .42f;
            const float elbowAngle = state->skeletonPaused
                ? poses[state->skeletonPose][1]
                : std::sin(static_cast<float>(state->angle * 1.85)) * .82f;
            const float wristAngle = state->skeletonPaused
                ? poses[state->skeletonPose][2]
                : std::cos(static_cast<float>(state->angle * 2.55)) * .95f;
            set3DTransform(state->document, state->view, state->entities[1],
                           -1.45f, .10f, 0.0f, rootAngle,
                           .82f, .82f, .82f);
            set3DTransform(state->document, state->view, state->entities[2],
                           1.35f, 0.0f, 0.0f, 0.0f,
                           1.42f, .18f, .24f);
            set3DTransform(state->document, state->view, state->entities[3],
                           2.75f, 0.0f, 0.0f, elbowAngle,
                           1.0f, 1.0f, 1.0f);
            set3DTransform(state->document, state->view, state->entities[4],
                           1.25f, 0.0f, 0.0f, 0.0f,
                           1.32f, .17f, .22f);
            set3DTransform(state->document, state->view, state->entities[5],
                           2.55f, 0.0f, 0.0f, wristAngle,
                           .88f, .88f, .88f);
            set3DTransform(state->document, state->view, state->entities[6],
                           1.05f, 0.0f, 0.0f, 0.0f,
                           1.08f, .22f, .30f);
            set3DTransform(state->document, state->view, state->entities[7],
                           2.15f, 0.0f, 0.0f, -wristAngle * .35f,
                           .72f, .72f, .72f);
            state->skeletonPoseDirty = false;
            if (!state->skeletonPaused) ++state->skeletonTicks;
        } else if (!state->entities.empty() &&
                   state->demoId != 10 && state->demoId != 11 && state->demoId != 12 &&
                   state->demoId != 14 && state->demoId != 15 &&
                   state->demoId != 24 && state->demoId != 25) {
            const std::size_t first = state->entities.size() > 1 ? 1 : 0;
            for (std::size_t i = first; i < state->entities.size(); ++i) {
                if (state->demoId != 14) break;
                const float orbit = static_cast<float>(state->angle + i * 0.23);
                set3DTransform(state->document, state->view, state->entities[i],
                               std::cos(orbit) * (1.2f + (i % 7) * 0.2f),
                               0.4f + (i % 5) * 0.15f,
                               std::sin(orbit) * (1.2f + (i % 7) * 0.2f), orbit,
                               0.45f, 0.45f, 0.45f);
            }
        }

        if (state->statusUpdateCountdown > 0) {
            --state->statusUpdateCountdown;
            return;
        }
        state->statusUpdateCountdown = 14; // 15 * 33ms: update telemetry at ~2 Hz.

        auto stats = invoke3D(state->document, state->view, "getRenderStats", {}, 4);
        std::string text = "Extension: active\nEntities: " + std::to_string(state->entities.size());
        if (stats && stats.values.size() == 4) {
            static const char* paths[] = {"CPU readback", "GPU zero-copy", "fallback", "failed"};
            const auto path = stats.values[0].intValue;
            text += "\nRender path: ";
            text += paths[path >= 0 && path < 4 ? path : 3];
            text += "\nBorrow attempts/failures: " + std::to_string(stats.values[1].intValue) +
                    "/" + std::to_string(stats.values[2].intValue);
            text += "\nCPU frames: " + std::to_string(stats.values[3].intValue);
        }
        if (!state->bodies.empty() && state->demoId != 12) {
            auto pos = invoke3D(state->document, state->view, "getRigidbodyPosition",
                                {extInteger(state->bodies.back())}, 3);
            if (pos && pos.values.size() == 3) {
                text += "\nBody Y: " + std::to_string(pos.values[1].floatValue);
                if (state->demoId == 10 || state->demoId == 11) {
                    auto velocity = invoke3D(
                        state->document, state->view, "getLinearVelocity",
                        {extInteger(state->bodies.back())}, 3);
                    if (velocity && velocity.values.size() == 3) {
                        const double velocityY = velocity.values[1].floatValue;
                        if (state->demoId == 10) {
                            text += "\nBody Vy: " + std::to_string(velocityY);
                            const bool settled = pos.values[1].floatValue <= .051 &&
                                                 std::abs(velocityY) < .01;
                            state->featureStatus = settled
                                ? "Bullet gravity: settled on plane at y=0.05"
                                : "Bullet gravity: falling from y=5";
                        } else {
                            text += "\nVelocity: " +
                                std::to_string(velocity.values[0].floatValue) + ", " +
                                std::to_string(velocityY) + ", " +
                                std::to_string(velocity.values[2].floatValue);
                        }
                    }
                }
                if (state->demoId == 15) {
                    auto velocity = invoke3D(
                        state->document, state->view, "getLinearVelocity",
                        {extInteger(state->bodies.back())}, 3);
                    text += state->renderPhysicsLinked
                        ? "\nRender component: linked"
                        : "\nRender component: detached";
                    if (velocity && velocity.values.size() == 3) {
                        text += "\nPhysics Vy: " +
                                std::to_string(velocity.values[1].floatValue);
                    }
                }
            }
        }
        if (state->demoId == 12 && state->bodies.size() >= 6) {
            auto physicsStats = invoke3D(state->document, state->view,
                                         "getPhysicsStats", {}, 4);
            std::int64_t manifoldCount = 0;
            std::int64_t collisionEnterCount = 0;
            bool paused = false;
            if (physicsStats && physicsStats.values.size() == 4) {
                text += "\nBullet bodies: " +
                        std::to_string(physicsStats.values[0].intValue);
                manifoldCount = physicsStats.values[1].intValue;
                text += "\nContact manifolds: " + std::to_string(manifoldCount);
                collisionEnterCount = physicsStats.values[2].intValue;
                text += "\nCollision enters: " +
                        std::to_string(collisionEnterCount);
                paused = physicsStats.values[3].boolValue;
                text += paused ? "\nPhysics: paused" : "\nPhysics: running";
            }
            int movingBodies = 0;
            double highestY = -1000.0;
            for (std::size_t i = 1; i < state->bodies.size(); ++i) {
                auto pos = invoke3D(state->document, state->view,
                                    "getRigidbodyPosition",
                                    {extInteger(state->bodies[i])}, 3);
                auto velocity = invoke3D(state->document, state->view,
                                         "getLinearVelocity",
                                         {extInteger(state->bodies[i])}, 3);
                if (pos && pos.values.size() == 3 &&
                    pos.values[1].floatValue > highestY) {
                    highestY = pos.values[1].floatValue;
                }
                if (velocity && velocity.values.size() == 3) {
                    const double vx = velocity.values[0].floatValue;
                    const double vy = velocity.values[1].floatValue;
                    const double vz = velocity.values[2].floatValue;
                    if (vx * vx + vy * vy + vz * vz > .01) ++movingBodies;
                }
            }
            text += "\nBodies with velocity: " +
                    std::to_string(movingBodies) + "/5";
            text += "\nHighest body Y: " + std::to_string(highestY);
            if (paused) {
                state->featureStatus = "Bullet collision: simulation paused";
            } else if (manifoldCount > 0) {
                state->featureStatus = "Bullet collision: active contacts detected";
            } else if (collisionEnterCount > 0) {
                state->featureStatus = "Bullet collision: contacts recorded; bodies moving";
            } else if (movingBodies > 0) {
                state->featureStatus = "Bullet collision: bodies converging/falling";
            } else {
                state->featureStatus = "Bullet collision: bodies stationary";
            }
        }
        if (state->demoId == 24) {
            auto hit = invoke3D(state->document, state->view, "raycast", {
                extNumber(0.0), extNumber(8.0), extNumber(0.0),
                extNumber(0.0), extNumber(-2.0), extNumber(0.0),
            }, 9);
            if (hit && hit.values.size() == 9) {
                text += hit.values[0].boolValue
                    ? "\nRaycast: HIT body " + std::to_string(hit.values[1].intValue) +
                      " at y=" + std::to_string(hit.values[3].floatValue)
                    : "\nRaycast: MISS";
            }
        }
        if (state->demoId == 14) {
            text += "\nSystem ticks: " +
                    std::to_string(state->updateSystemTicks);
            text += "\nUpdated entities/tick: " +
                    std::to_string(state->entities.size() > 0
                        ? state->entities.size() - 1 : 0);
            text += state->updateSystemPaused
                ? "\nSystem state: paused" : "\nSystem state: running";
        }
        if (state->demoId == 16) {
            text += state->sceneGrandchildOnChild
                ? "\nHierarchy depth: 3" : "\nHierarchy depth: 2";
            text += state->sceneGrandchildOnChild
                ? "\nEndpoint parent: orange child"
                : "\nEndpoint parent: blue root";
            const int yawDegrees = static_cast<int>(
                std::fmod(state->angle * .55 * 57.2957795, 360.0));
            text += "\nRoot yaw: " + std::to_string(yawDegrees) + " deg";
        }
        if (state->demoId == 17) {
            text += state->loadedAsset ? "\nLoaded assets: 1" : "\nLoaded assets: 0";
            text += "\nAsset handle: " + std::to_string(state->loadedAsset);
            const int yawDegrees = static_cast<int>(std::lround(
                std::fmod(state->modelYaw * 57.2957795, 360.0)));
            text += "\nModel yaw: " + std::to_string(yawDegrees) + " deg";
        }
        if (state->demoId == 18) {
            text += "\nSkeleton joints: 4";
            text += "\nVisible bone segments: 3";
            text += "\nAnimation ticks: " + std::to_string(state->skeletonTicks);
            text += state->skeletonPaused
                ? "\nAnimation state: paused / key pose " +
                      std::to_string(state->skeletonPose + 1)
                : "\nAnimation state: playing";
            static const char* speeds[] = {"0.5x", "1.0x", "1.75x"};
            text += "\nPlayback speed: " +
                    std::string(speeds[state->skeletonSpeedIndex]);
        }
        if (state->demoId == 19) {
            static const char* toneNames[] = {"Linear", "ACES", "Filmic", "AgX"};
            text += state->bloom ? "\nBloom: enabled" : "\nBloom: disabled";
            text += "\nTone mapping: " +
                    std::string(toneNames[state->postToneMapping]);
            text += state->postFog ? "\nDepth fog: enabled" : "\nDepth fog: disabled";
            text += "\nPost scene objects: 9";
        }
        if (state->demoId == 26) {
            auto count = invoke3D(state->document, state->view,
                                  "getPhysicsDebugLineCount", {}, 1);
            if (count && !count.values.empty()) {
                text += "\nBullet debug lines: " +
                        std::to_string(count.values[0].intValue);
            }
        }
        if (!state->featureStatus.empty()) text += "\n" + state->featureStatus;
        if (text != state->lastStatusText) {
            state->status->setTextContent(text);
            state->lastStatusText = std::move(text);
        }
    }, 33);

    return Platform::run();
}

} // namespace kui::examples
