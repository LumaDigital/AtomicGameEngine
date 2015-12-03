//
// Copyright (c) 2014-2015, THUNDERBEAST GAMES LLC All rights reserved
// LICENSE: Atomic Game Engine Editor and Tools EULA
// Please see LICENSE_ATOMIC_EDITOR_AND_TOOLS.md in repository root for
// license information: https://github.com/AtomicGameEngine/AtomicGameEngine
//

var audioCreateSource = new Atomic.UIMenuItemSource();

audioCreateSource.addItem(new Atomic.UIMenuItem("SoundListener", "SoundListener"));
audioCreateSource.addItem(new Atomic.UIMenuItem("SoundSource", "SoundSource"));
audioCreateSource.addItem(new Atomic.UIMenuItem("SoundSource3D", "SoundSource3D"));

var _2DCreateSource = new Atomic.UIMenuItemSource();
_2DCreateSource.addItem(new Atomic.UIMenuItem("PhysicsWorld2D", "PhysicsWorld2D"));
_2DCreateSource.addItem(new Atomic.UIMenuItem("StaticSprite2D", "StaticSprite2D"));
_2DCreateSource.addItem(new Atomic.UIMenuItem("AnimatedSprite2D", "AnimatedSprite2D"));
_2DCreateSource.addItem(new Atomic.UIMenuItem("ParticleEmitter2D", "ParticleEmitter2D"));
_2DCreateSource.addItem(new Atomic.UIMenuItem("PointLight2D", "PointLight2D"));
_2DCreateSource.addItem(new Atomic.UIMenuItem("DirectionalLight2D", "DirectionalLight2D"));
_2DCreateSource.addItem(new Atomic.UIMenuItem("RigidBody2D", "RigidBody2D"));
_2DCreateSource.addItem(new Atomic.UIMenuItem("CollisionBox2D", "CollisionBox2D"));
_2DCreateSource.addItem(new Atomic.UIMenuItem("CollisionCircle2D", "CollisionCircle2D"));
_2DCreateSource.addItem(new Atomic.UIMenuItem("TileMap2D", "TileMap2D"));

var geometryCreateSource = new Atomic.UIMenuItemSource();

geometryCreateSource.addItem(new Atomic.UIMenuItem("StaticModel", "StaticModel"));
geometryCreateSource.addItem(new Atomic.UIMenuItem("AnimatedModel", "AnimatedModel"));
geometryCreateSource.addItem(new Atomic.UIMenuItem("AnimationController", "AnimationController"));
geometryCreateSource.addItem(new Atomic.UIMenuItem("BillboardSet", "create component"));
geometryCreateSource.addItem(new Atomic.UIMenuItem("CustomGeometry", "create component"));
geometryCreateSource.addItem(new Atomic.UIMenuItem("ParticleEmitter", "create component"));
geometryCreateSource.addItem(new Atomic.UIMenuItem("Skybox", "SkyBox"));
geometryCreateSource.addItem(new Atomic.UIMenuItem("StaticModelGroup", "create component"));
geometryCreateSource.addItem(new Atomic.UIMenuItem("Terrain", "create component"));
geometryCreateSource.addItem(new Atomic.UIMenuItem("Text3D", "create component"));
geometryCreateSource.addItem(new Atomic.UIMenuItem("Water", "create component"));

var logicCreateSource = new Atomic.UIMenuItemSource();

logicCreateSource.addItem(new Atomic.UIMenuItem("JSComponent", "JSComponent"));
logicCreateSource.addItem(new Atomic.UIMenuItem("CSComponent", "CSComponent"));
logicCreateSource.addItem(new Atomic.UIMenuItem("AnimationController", "create component"));
logicCreateSource.addItem(new Atomic.UIMenuItem("SplinePath", "create component"));

var navigationCreateSource = new Atomic.UIMenuItemSource();

navigationCreateSource.addItem(new Atomic.UIMenuItem("Navigable", "create component"));
navigationCreateSource.addItem(new Atomic.UIMenuItem("NavigationMesh", "create component"));
navigationCreateSource.addItem(new Atomic.UIMenuItem("OffMeshConnection", "create component"));

var networkCreateSource = new Atomic.UIMenuItemSource();

networkCreateSource.addItem(new Atomic.UIMenuItem("Network Priority", "create component"));

var physicsCreateSource = new Atomic.UIMenuItemSource();

physicsCreateSource.addItem(new Atomic.UIMenuItem("CollisionShape", "CollisionShape"));
physicsCreateSource.addItem(new Atomic.UIMenuItem("Constraint", "create component"));
physicsCreateSource.addItem(new Atomic.UIMenuItem("RigidBody", "RigidBody"));

var sceneCreateSource = new Atomic.UIMenuItemSource();

sceneCreateSource.addItem(new Atomic.UIMenuItem("Camera", "Camera"));
sceneCreateSource.addItem(new Atomic.UIMenuItem("Light", "Light"));
sceneCreateSource.addItem(new Atomic.UIMenuItem("Zone", "Zone"));

var subsystemCreateSource = new Atomic.UIMenuItemSource();

subsystemCreateSource.addItem(new Atomic.UIMenuItem("DebugRenderer", "create component"));
subsystemCreateSource.addItem(new Atomic.UIMenuItem("Octree", "create component"));
subsystemCreateSource.addItem(new Atomic.UIMenuItem("PhysicsWorld", "create component"));

var editorCreateSource = new Atomic.UIMenuItemSource();

editorCreateSource.addItem(new Atomic.UIMenuItem("CubemapGenerator", "CubemapGenerator"));


var componentCreateSource = new Atomic.UIMenuItemSource();

var lumaCreateSource = new Atomic.UIMenuItemSource();
lumaCreateSource.addItem(new Atomic.UIMenuItem("GridPositioner", "ComponentGridPositioner"));
lumaCreateSource.addItem(new Atomic.UIMenuItem("TestEvents", "ComponentTestEvents"));
lumaCreateSource.addItem(new Atomic.UIMenuItem("DynamicDataTexture2D", "ComponentDynamicDataTexture2D"));
lumaCreateSource.addItem(new Atomic.UIMenuItem("BrandingApplier", "ComponentBrandingApplier"));

var sources = {
    Audio: audioCreateSource,
    "2D": _2DCreateSource,
    Geometry: geometryCreateSource,
    Logic: logicCreateSource,
    Navigation: navigationCreateSource,
    Network: networkCreateSource,
    Physics: physicsCreateSource,
    Scene: sceneCreateSource,
    SubSystem: subsystemCreateSource,
	Editor : editorCreateSource,
	Luma: lumaCreateSource
}

for (var sub in sources) {

    var item = new Atomic.UIMenuItem(sub);
    item.subSource = sources[sub];
    componentCreateSource.addItem(item);

}


class CreateComponentButton extends Atomic.UIButton {

    constructor() {

        super();

        this.fd.id = "Vera";
        this.fd.size = 11;

        this.text = "Create Component";

        this.subscribeToEvent("WidgetEvent", (data) => this.handleWidgetEvent(data));

    }

    // note instance method
    onClick = () => {

        var menu = new Atomic.UIMenuWindow(this, "create component popup");
        menu.fontDescription = this.fd;
        menu.show(componentCreateSource);
    }

    handleWidgetEvent(ev: Atomic.UIWidgetEvent) {

        if (ev.type != Atomic.UI_EVENT_TYPE_CLICK)
            return;

        if (ev.target && ev.target.id == "create component popup") {

            this.sendEvent("SelectionCreateComponent", { componentTypeName : ev.refid});

            return true;

        }

    }

    fd: Atomic.UIFontDescription = new Atomic.UIFontDescription();

}

export = CreateComponentButton;
