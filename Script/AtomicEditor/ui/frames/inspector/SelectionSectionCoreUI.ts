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

import EditorUI = require("ui/EditorUI");
import InspectorUtils = require("./InspectorUtils");
import SelectionSection = require("./SelectionSection");
import SelectionSectionUI = require("./SelectionSectionUI");
import SerializableEditType = require("./SerializableEditType");

import ProgressModal = require("ui/modal/ProgressModal");


class CollisionShapeSectionUI extends SelectionSectionUI {

    createUI(editType: SerializableEditType) {

        this.editType = editType;

        var button = new Atomic.UIButton();
        button.fontDescription = InspectorUtils.attrFontDesc;
        button.gravity = Atomic.UI_GRAVITY_RIGHT;
        button.text = "Set from StaticModel";

        button.onClick = () => {

            for (var i in this.editType.objects) {

                var shape = <Atomic.CollisionShape>this.editType.objects[i];
                var model = <Atomic.Drawable>shape.node.getComponent("StaticModel");

                if (model) {
                    var box = model.boundingBox;
                    shape.setBox([box[3] - box[0], box[4] - box[1], box[5] - box[2]]);
                }

            }

        };

        this.addChild(button);

    }

}

class CubemapGeneratorSectionUI extends SelectionSectionUI {

    createUI(editType: SerializableEditType) {

        this.editType = editType;

        var button = new Atomic.UIButton();
        button.fontDescription = InspectorUtils.attrFontDesc;
        button.gravity = Atomic.UI_GRAVITY_RIGHT;
        button.text = "Render Cubemap";

        button.onClick = () => {

            var scene = null;
            var count = 0;
            var progressModal = new ProgressModal("Rendering Cubemaps", "Rendering Cubemaps, please wait...");

            for (var i in this.editType.objects) {

                var gen = <Editor.CubemapGenerator>this.editType.objects[i];

                if (!scene) {

                    scene = gen.scene;

                    this.subscribeToEvent(scene, "CubemapRenderBegin", () => {

                        count++;

                    });

                    this.subscribeToEvent(scene, "CubemapRenderEnd", () => {

                        count--;

                        if (!count)
                            progressModal.hide();

                    });


                }

                if (!gen.render()) {

                    //TODO: Cancel other renders if any and report better error information

                    EditorUI.showModalError("Render Cubemaps",
                        "There was an error rendering cubemaps, please see the application log");

                    scene = null;
                    break;

                }
            }

            if (scene) {

                progressModal.show();

            }

        };

        this.addChild(button);

    }

}


class SceneLightmapGeneratorSectionUI extends SelectionSectionUI {

    createUI(editType: SerializableEditType) {

        this.editType = editType;

        let button = new Atomic.UIButton();
        button.fontDescription = InspectorUtils.attrFontDesc;
        button.gravity = Atomic.UI_GRAVITY_RIGHT;
        button.text = "Generate Lightmap";

        button.onClick = () => {

            let progressModal : ProgressModal = null;
            let gen : Editor.LightmapGenerator = null;

            // Should really be exactly one
            for (var i in this.editType.objects) {

                gen = <Editor.LightmapGenerator>this.editType.objects[i];

                if (gen) 
                    break;
            }

            if (gen) {
                this.subscribeToEvent(gen, "LightmapGenerateEnd", () => {
                    if (progressModal)
                        progressModal.hide();
                });

                progressModal = new ProgressModal("Generating Lightmap", "Generating Lightmap, please wait...");
                progressModal.show();

                if (!gen.generateLightmap()) {
                    progressModal.hide();
                    progressModal = null;

                    EditorUI.showModalError("Generating Lightmap",
                        "There was an error generating lightmaps, please see the application log");
                }
                else {
                    gen.getScene().sendEvent("SceneEditEnd");
                }

            }
        };

        this.addChild(button);
    }

}

SelectionSection.registerCustomSectionUI("CollisionShape", CollisionShapeSectionUI);
SelectionSection.registerCustomSectionUI("CubemapGenerator", CubemapGeneratorSectionUI);
SelectionSection.registerCustomSectionUI("LightmapGenerator", SceneLightmapGeneratorSectionUI);
