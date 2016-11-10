//
// Copyright (c) 2014-2016 THUNDERBEAST GAMES LLC
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

import EditorEvents = require("editor/EditorEvents");
import EditorUI = require("ui/EditorUI");
import HierarchyFrame = require("ui/frames/HierarchyFrame");
import InspectorUtils = require("ui/frames/inspector/InspectorUtils");
import ResourceOps = require("resources/ResourceOps");

class AnimationViewer extends Atomic.UIWidget {

    constructor(parent: Atomic.UIWidget, asset: ToolCore.Asset) {

        super();

        this.load("AtomicEditor/editor/ui/animationviewer.tb.txt");
        this.asset = asset;

        this.leftAnimContainer = <Atomic.UILayout>this.getWidget("leftanimcontainer");
        this.rightAnimContainer = <Atomic.UILayout>this.getWidget("rightanimcontainer");
        this.blendFileContainer = <Atomic.UILayout>this.getWidget("blendcontainer");

        this.subscribeToEvent(this, "WidgetEvent", (ev) => this.handleWidgetEvent(ev));
        this.subscribeToEvent(EditorEvents.ActiveSceneEditorChange, (data) => this.handleActiveSceneEditorChanged(data));
        this.subscribeToEvent(EditorEvents.SceneClosed, (data) => this.handleSceneClosed(data));

        var leftAnimationField = InspectorUtils.createAttrEditFieldWithSelectButton("Animation A", this.leftAnimContainer);
        leftAnimationField.selectButton.onClick = function () { this.openAnimationSelectionBox(leftAnimationField.editField, this.leftAnim); }.bind(this);

        var rightAnimationField = InspectorUtils.createAttrEditFieldWithSelectButton("Animation B", this.rightAnimContainer);
        rightAnimationField.selectButton.onClick = function () { this.openAnimationSelectionBox(rightAnimationField.editField, this.rightAnim); }.bind(this);

        this.leftAnimEditfield = leftAnimationField.editField;
        this.rightAnimEditfield = rightAnimationField.editField;

        var resourcePath = ToolCore.getToolSystem().project.resourcePath;
        var sceneName = "BlendScene";
        this.sceneAssetPath = Atomic.addTrailingSlash(resourcePath) + sceneName;

        if (this.sceneAssetPath.indexOf(".scene") == -1) this.sceneAssetPath += ".scene";

        Atomic.fileSystem.delete(this.sceneAssetPath);

        if (ResourceOps.CreateNewScene(this.sceneAssetPath, sceneName)) {
            this.sendEvent(EditorEvents.EditResource, { path: this.sceneAssetPath });
        }

        this.stateDropDownLeft = new Atomic.UISelectDropdown(true);
        this.stateDropDownRight = new Atomic.UISelectDropdown(true);

        this.populateStateDropDownList(this.stateDropDownLeft);
        this.populateStateDropDownList(this.stateDropDownRight);

        var leftStateContainer = <Atomic.UILayout>this.getWidget("leftstatedropdown");
        var rightStateContainer = <Atomic.UILayout>this.getWidget("rightstatedropdown");

        this.stateDropDownLeft.text = "STATE";
        this.stateDropDownRight.text = "STATE";

        this.stateDropDownLeft.id = "leftdrop";
        this.stateDropDownLeft.id = "rightdrop";

        leftStateContainer.addChild(this.stateDropDownLeft);
        rightStateContainer.addChild(this.stateDropDownRight);

        parent.addChild(this);
    }

    handleWidgetEvent(ev: Atomic.UIWidgetEvent): boolean {

        if (ev.type == Atomic.UI_EVENT_TYPE_CLICK) {
            if (this.animationController != null) {

                if (ev.target.id == "play_left") {
                    if (this.animationBlender != null && this.stateDropDownLeft.text != "NONE")
                        this.blenderController.playAnimation(this.stateDropDownLeft.text, this.leftAnimEditfield.text);
                    else
                        this.animationController.playExclusive(this.leftAnimEditfield.text, 0, true);
                    return true;
                }
                if (ev.target.id == "play_right") {
                    if (this.animationBlender != null && this.stateDropDownRight.text != "NONE")
                        this.blenderController.playAnimation(this.stateDropDownRight.text, this.rightAnimEditfield.text);
                    else
                        this.animationController.playExclusive(this.rightAnimEditfield.text, 0, true);
                    return true;
                }
                if (ev.target.id == "blend_left") {
                        this.animationController.playExclusive(this.leftAnimEditfield.text, 0, true, 0.5);
                    return true;
                }
                if (ev.target.id == "blend_right") {
                    this.animationController.playExclusive(this.rightAnimEditfield.text, 0, true, 0.5);
                    return true;
                }
                if (ev.target.id == "stop") {
                    this.animationController.stopAll();

                    this.animationBlender = this.blenderController.getAnimationBlender();
                    return true;
                }
                if (ev.target.id == "leftdrop") {
                    this.populateStateDropDownList(this.stateDropDownLeft);
                    return true;
                }
                if (ev.target.id == "rightdrop") {

                    this.populateStateDropDownList(this.stateDropDownRight);
                    return true;
                }
            }
        }
        return true;
    }

    handleSceneClosed(ev: EditorEvents.SceneClosedEvent) {
        if (ev.scene == this.scene) {
            Atomic.fileSystem.delete(this.sceneAssetPath);
            this.remove();
        }
    }

    closeViewer() {
        if (this.scene) {
            this.sceneEditor.close();
        }
    }

    handleActiveSceneEditorChanged(event: EditorEvents.ActiveSceneEditorChangeEvent) {

        if (event.sceneEditor.scene == this.scene) {
            this.sceneEditor.close;
        }
        if (this.scene) {
            this.unsubscribeFromEvents(this.scene);
            return;
        }

        if (!event.sceneEditor)
            return;

        this.sceneEditor = event.sceneEditor;
        this.scene = event.sceneEditor.scene;
        this.scene.setUpdateEnabled(true);

        var modelNode = this.asset.instantiateNode(this.scene, this.asset.name);

        var blenderComponent = new Luma.BlenderController();
        modelNode.addComponent(blenderComponent, 0, 0);
        this.sceneEditor.selection.addNode(modelNode, true);
        this.sceneEditor.sceneView3D.frameSelection();

        this.animatedModel = <Atomic.AnimatedModel>modelNode.getComponent("AnimatedModel");
        this.animationController = <Atomic.AnimationController>modelNode.getComponent("AnimationController");
        var model = this.animatedModel.model;
        this.animatedModel.setBoneCreationOverride(true);
        this.animatedModel.setModel(model, true);

        this.blenderController = blenderComponent;
        this.blenderController.setAnimationController(this.animationController);

        var animComp = new Atomic.AnimatedModel();
        var animContComp = new Atomic.AnimationController();

    }

    openAnimationSelectionBox(animationWidget: Atomic.UIEditField, animationSlot: Atomic.Animation) {

        this.populateStateDropDownList(this.stateDropDownLeft);
        this.populateStateDropDownList(this.stateDropDownRight);

        EditorUI.getModelOps().showResourceSelection("Select Animation", "ModelImporter", "Animation", function (resource: Atomic.Animation, args: any) {
            var animation = resource;
            if (animation) {
                animationSlot = animation;
                animationWidget.text = animation.getAnimationName();
            }
        });
    }

    populateStateDropDownList(stateDropDown: Atomic.UISelectDropdown) {

        this.animationBlender = this.blenderController.animationBlender;

        if (this.animationBlender == null)
            return;

        this.refreshStates();
        var previousState = stateDropDown.text;
        stateDropDown.setSource(null);

        var stateNameSource = new Atomic.UISelectItemSource();

        for (var i = 0; i < this.stateDropDownList.length; i++) {
            var size = new Atomic.UISelectItem();
            size.setString(this.stateDropDownList[i]);
            stateNameSource.addItem(size);
        }
        stateDropDown.setSource(stateNameSource);
        stateDropDown.text = previousState;
    }

    refreshStates() {
        this.stateDropDownList = [];
        this.stateDropDownList.push("NONE");

        if (this.animationBlender != null) {
            for (var i = 0; i < this.animationBlender.getBlendStatesSize(); i++) {
                this.stateDropDownList.push(this.animationBlender.getBlendState(i).stateName);
            }
        }
    }

    animationController: Atomic.AnimationController;
    blenderController: Luma.BlenderController;
    animatedModel: Atomic.AnimatedModel;
    scene: Atomic.Scene = null;
    sceneEditor: Editor.SceneEditor3D;

    leftAnimContainer: Atomic.UILayout;
    rightAnimContainer: Atomic.UILayout;
    blendFileContainer: Atomic.UILayout;
    leftAnimEditfield: Atomic.UIEditField;
    rightAnimEditfield: Atomic.UIEditField;
    stateDropDownLeft: Atomic.UISelectDropdown;
    stateDropDownRight: Atomic.UISelectDropdown;

    leftAnim: Atomic.Animation;
    rightAnim: Atomic.Animation;
    animationBlender: Luma.AnimationBlender;

    asset: ToolCore.Asset;
    sceneAssetPath: string;
    stateDropDownList: string[];

}

export = AnimationViewer;


