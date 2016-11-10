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

import InspectorWidget = require("./InspectorWidget");
import ArrayEditWidget = require("./ArrayEditWidget");
import InspectorUtils = require("./InspectorUtils");
import EditorEvents = require("editor/EditorEvents");
import EditorUI = require("ui/EditorUI");

class AnimationBlenderInspector extends InspectorWidget {

    constructor() {

        super();

        this.subscribeToEvent(this, "WidgetEvent", (data) => this.handleWidgetEvent(data));

    }

    handleWidgetEvent(ev: Atomic.UIWidgetEvent): boolean {
        return false;
    }

    inspect(asset: ToolCore.Asset, animationBlender: Luma.AnimationBlender) {

        this.asset = asset;
        this.animationBlender = animationBlender;
        this.stateCount = this.animationBlender.getBlendStatesSize();

        // node attr layout
        var rootLayout = this.rootLayout;

        // Blender Section
        var blenderLayout = this.createSection(rootLayout, "AnimationBlender", 1);

        var editField = InspectorUtils.createAttrEditField("Name", blenderLayout);

        var lp = new Atomic.UILayoutParams();
        editField.readOnly = true;
        editField.text = asset.name;

        //This should preferably be onClick
        editField.subscribeToEvent(editField, "UIWidgetFocusChanged", (ev: Atomic.UIWidgetFocusChangedEvent) => {

            if (ev.widget == editField && editField.focus) {
                this.sendEvent(EditorEvents.InspectorProjectReference, { "path": asset.getRelativePath() });
            }
        });

        // States Section
        var animationLayout = this.createSection(rootLayout, "AnimationStates", 1);

        this.importBlendStateArray = new ArrayEditWidget("State Count");
        animationLayout.addChild(this.importBlendStateArray);

        this.importBlendStateArray.onCountChanged = (count) => this.onStateCountChanged(count);
        this.stateCountEdit = this.importBlendStateArray.countEdit;

        var nlp = new Atomic.UILayoutParams();
        nlp.width = 310;

        var animLayout = this.animationInfoLayout = new Atomic.UILayout();

        animLayout.spacing = 4;

        animLayout.layoutDistribution = Atomic.UI_LAYOUT_DISTRIBUTION_GRAVITY;
        animLayout.layoutPosition = Atomic.UI_LAYOUT_POSITION_LEFT_TOP;
        animLayout.layoutParams = nlp;
        animLayout.axis = Atomic.UI_AXIS_Y;
        animLayout.gravity = Atomic.UI_GRAVITY_ALL;

        animationLayout.addChild(animLayout);

        this.createAnimationStateEntries();

        // apply button
        rootLayout.addChild(this.createApplyButton());

    }

    createAnimationStateEntries() {

        var layout = this.animationInfoLayout;
        layout.deleteAllChildren();

        var count = this.stateCount;

        this.importBlendStateArray.countEdit.text = count.toString();
        this.stateWidget = [];
        this.blendStateStruct = [];
        this.importBlendDataArray = [];

        for (var i = 0; i < count; i++) {

            var blendState;

            if (i >= this.animationBlender.getBlendStatesSize()) {
                blendState = new Luma.BlendState();
            }
            else {
                blendState = this.animationBlender.getBlendState(i);
            }

            this.blendStateStruct.push(blendState);

            var stateLayout = this.createSection(layout, "State: " + blendState.stateName, 0);
            var stateNameEdit = InspectorUtils.createAttrEditField("State Name", stateLayout);
            stateNameEdit.tooltip = "The name of your custom animaiton State.";

            var layerEdit = InspectorUtils.createAttrEditField("Layer", stateLayout);
            layerEdit.tooltip = "The animation layer the animations within this state must play on.";

            var weightEdit = InspectorUtils.createAttrEditField("Weight", stateLayout);
            weightEdit.tooltip = "The weight this animation state has on it's assigned layer.";


            layerEdit.text = blendState.layer.toString();
            weightEdit.text = blendState.weight.toString();
            stateNameEdit.text = blendState.stateName;

            var animationRootLayout = new Atomic.UILayout();
            stateLayout.addChild(animationRootLayout);

            this.stateWidget[i] = new StateWidget();
            this.stateWidget[i].stateNameEdits = stateNameEdit;
            this.stateWidget[i].layerEdits = layerEdit;
            this.stateWidget[i].weightEdits = weightEdit;

            this.createAnimationDataEntries(this.blendStateStruct[i], animationRootLayout, i);

            InspectorUtils.createSeparator(layout);
        }
    }

    createAnimationDataEntries(blendState: Luma.BlendState, animationRootLayout: Atomic.UILayout, dataCountIndex: number) {

        this.stateWidget[dataCountIndex].clearWidgets();

        var layout = new Atomic.UILayout();

        layout.layoutDistribution = Atomic.UI_LAYOUT_DISTRIBUTION_GRAVITY;
        layout.layoutPosition = Atomic.UI_LAYOUT_POSITION_LEFT_TOP;
        layout.axis = Atomic.UI_AXIS_Y;
        layout.gravity = Atomic.UI_GRAVITY_ALL;

        this.animationInfoLayout.setFocus();
        animationRootLayout.deleteAllChildren();
        animationRootLayout.addChild(layout);

        var arrayWidget = new ArrayEditWidget("Animation Count");
        layout.addChild(arrayWidget);

        this.importBlendDataArray[dataCountIndex] = arrayWidget;

        var count = blendState.animationCount;

        arrayWidget.onCountChanged = (count) => this.onDataCountChanged(count, dataCountIndex, layout);
        arrayWidget.countEdit.text = count.toString();
        blendState.resizeBlendData(count);

        for (var i = 0; i < count; i++) {

            var blendData;

            if (i >= blendState.getBlendDataSize()) {
                blendData = new Luma.BlendData();
            }
            else {
                blendData = blendState.getBlendData(i);
            }

            var animationLayout = this.createSection(layout, "Animation: " + blendData.animationName, 0);

            var leftAnimationField = InspectorUtils.createAttrEditFieldWithSelectButton("Animation", animationLayout);
            leftAnimationField.selectButton.onClick = function () { this.openAnimationSelectionBox(leftAnimationField.editField, this.leftAnim); }.bind(this);
            leftAnimationField.editField.text = blendData.animationName;
            leftAnimationField.editField.tooltip = "Select/Enter Animation Clip";

            var loopedEdit = InspectorUtils.createAttrCheckBox("Looped", animationLayout);
            loopedEdit.checkBox.value = Number(blendData.isLooped);
            loopedEdit.checkBox.tooltip = "Enable if the selected animation must loop";

            var boneEdit = InspectorUtils.createAttrEditField("Starting Bone", animationLayout);
            boneEdit.text = blendData.startingBone;
            boneEdit.tooltip = "Assign a root bone the selected animation must play from.";

            var startEdit = InspectorUtils.createAttrEditField("Start Frame", animationLayout);
            startEdit.text = blendData.startFrame.toString();
            startEdit.tooltip = "Starting frame of the seleceted animation.";

            var endEdit = InspectorUtils.createAttrEditField("End Frame", animationLayout);
            endEdit.text = blendData.endFrame.toString();
            endEdit.tooltip = "Final frame of the seleceted animation.";

            var syncEdit = InspectorUtils.createAttrEditField("Sync Frame", animationLayout);
            syncEdit.text = blendData.syncFrame.toString();
            syncEdit.tooltip = "Frame that this animation has in common with others within it's shared state.";

            var speedEdit = InspectorUtils.createAttrEditField("Speed", animationLayout);
            speedEdit.text = blendData.animationSpeed.toString();
            speedEdit.tooltip = "Playback speed of the selected animation. Default speed is 1.0";

            var blendTimeEdit = InspectorUtils.createAttrEditField("Blend In Time", animationLayout);
            blendTimeEdit.text = blendData.blendTime.toString();
            speedEdit.tooltip = "The time it takes for the selected animation to blend into full weighting";

            this.stateWidget[dataCountIndex].animNameEdits.push(leftAnimationField.editField);
            this.stateWidget[dataCountIndex].startBoneEdits.push(boneEdit);
            this.stateWidget[dataCountIndex].loopedEdits.push(loopedEdit.checkBox);
            this.stateWidget[dataCountIndex].startFrameEdits.push(startEdit);
            this.stateWidget[dataCountIndex].endFrameEdits.push(endEdit);
            this.stateWidget[dataCountIndex].syncFrameEdits.push(syncEdit);
            this.stateWidget[dataCountIndex].animSpeedEdits.push(speedEdit);
            this.stateWidget[dataCountIndex].blendTimeEdits.push(blendTimeEdit);
        }
    }

    onStateCountChanged(count: number) {
        if (this.stateCount == count)
            return;

        this.stateCount = count;
        this.createAnimationStateEntries();
    }

    onDataCountChanged(count: number, dataCountIndex: number, layout: Atomic.UILayout) {

        var prevAnimationCount = this.blendStateStruct[dataCountIndex].animationCount;

        if (count == prevAnimationCount)
            return;

        this.blendStateStruct[dataCountIndex].animationCount = count;

        if (count > prevAnimationCount) {
            var prevDataCount = this.blendStateStruct[dataCountIndex].getBlendDataSize();

            for (var i = 0; i < (count - prevDataCount); i++) {
                this.blendStateStruct[dataCountIndex].addBlendData(new Luma.BlendData());
            }
        }
        this.createAnimationStateEntries();
    }

    openAnimationSelectionBox(animationWidget: Atomic.UIEditField, animationSlot: Atomic.Animation) {

        EditorUI.getModelOps().showResourceSelection("Select Animation", "ModelImporter", "Animation", function (resource: Atomic.Animation, args: any) {
            console.log("asset.path: " + resource.name);
            var animation = resource;

            if (animation) {
                animationSlot = animation;
                animationWidget.text = animation.getAnimationName();
            }
        });
    }

    onApply() {

        this.stateCount = Number(this.stateCountEdit.text);
        this.animationBlender.clearBlendStates();

        var blendDataIndex = 0;

        for (var j = 0; j < this.stateCount; j++) {

            var blendState = this.blendStateStruct[j];
            var stateNameEdit = this.stateWidget[j].stateNameEdits;
            var layerEdit = this.stateWidget[j].layerEdits;
            var weightEdit = this.stateWidget[j].weightEdits;

            blendState.stateName = stateNameEdit.text;
            blendState.animationCount = this.stateWidget[j].animNameEdits.length;
            blendState.layer = Number(layerEdit.text);
            blendState.weight = Number(weightEdit.text);

            for (var i = 0; i < blendState.animationCount; i++) {

                var blendData = blendState.getBlendData(i);
                var animNameEdit = this.stateWidget[j].animNameEdits[i];

                var startBoneEdit = this.stateWidget[j].startBoneEdits[i];
                var loopedEdit = this.stateWidget[j].loopedEdits[i];

                var startFrameEdit = this.stateWidget[j].startFrameEdits[i];
                var endFrameEdit = this.stateWidget[j].endFrameEdits[i];
                var syncFrameEdit = this.stateWidget[j].syncFrameEdits[i];
                var animSpeedEdit = this.stateWidget[j].animSpeedEdits[i];
                var blendTimeEdit = this.stateWidget[j].blendTimeEdits[i];

                blendData.startFrame = Number(startFrameEdit.text);
                blendData.isLooped = Boolean((loopedEdit.value));
                blendData.startingBone = startBoneEdit.text;
                blendData.endFrame = Number(endFrameEdit.text);
                blendData.syncFrame = Number(syncFrameEdit.text);
                blendData.animationSpeed = Number(animSpeedEdit.text);
                blendData.blendTime = Number(blendTimeEdit.text);
                blendData.animationName = animNameEdit.text;

                blendDataIndex++;
            }
            this.animationBlender.addBlendState(blendState);
        }
        var importer:any; 
		importer = this.asset.importer;
        importer.saveAnimationBlender();

    }

    stateCount: number;

    stateCountEdit: Atomic.UIEditField;
    blendStateStruct: Luma.BlendState[];
    importBlendStateArray: ArrayEditWidget;
    importBlendDataArray: ArrayEditWidget[];
    animationInfoLayout: Atomic.UILayout;
    stateWidget: StateWidget[];

    asset: ToolCore.Asset;
    animationBlender: Luma.AnimationBlender;
    importer: Luma.AnimationBlenderImporter;
}
export = AnimationBlenderInspector;

class StateWidget {

    constructor() {
        this.stateNameEdits = new Atomic.UIEditField();
        this.layerEdits = new Atomic.UIEditField();
        this.weightEdits = new Atomic.UIEditField();

        this.animNameEdits=[];
        this.startBoneEdits=[];
        this.loopedEdits=[];
        this.startFrameEdits=[];
        this.endFrameEdits=[];
        this.syncFrameEdits=[];

        this.animSpeedEdits=[];
        this.blendTimeEdits=[];
    }

    clearWidgets() {
        this.animNameEdits = [];
        this.startBoneEdits = [];
        this.loopedEdits = [];
        this.startFrameEdits = [];
        this.endFrameEdits = [];
        this.syncFrameEdits = [];

        this.animSpeedEdits = [];
        this.blendTimeEdits = [];
    }
    stateNameEdits: Atomic.UIEditField;
    layerEdits: Atomic.UIEditField;
    weightEdits: Atomic.UIEditField;

    animNameEdits: Atomic.UIEditField[];
    startBoneEdits: Atomic.UIEditField[];
    loopedEdits: Atomic.UICheckBox[];
    startFrameEdits: Atomic.UIEditField[];
    endFrameEdits: Atomic.UIEditField[];
    syncFrameEdits: Atomic.UIEditField[];

    animSpeedEdits: Atomic.UIEditField[];
    blendTimeEdits: Atomic.UIEditField[];
}
