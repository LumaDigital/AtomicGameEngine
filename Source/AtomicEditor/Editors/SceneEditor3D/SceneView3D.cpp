// Portions Copyright (c) 2008-2015 the Urho3D project.

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

#include <Atomic/IO/Log.h>
#include <Atomic/Core/CoreEvents.h>
#include <Atomic/Scene/SceneEvents.h>
#include <Atomic/Scene/Scene.h>
#include <Atomic/Scene/PrefabComponent.h>
#include <Atomic/Graphics/Camera.h>

#include <Atomic/Graphics/Graphics.h>
#include <Atomic/Graphics/DebugRenderer.h>
#include <Atomic/Graphics/Viewport.h>
#include <Atomic/Graphics/Octree.h>
#include <Atomic/Graphics/Material.h>

#include <Atomic/Atomic3D/Model.h>
#include <Atomic/Atomic3D/StaticModel.h>
#include <Atomic/Atomic3D/AnimatedModel.h>
#include <Atomic/Atomic3D/AnimationController.h>

#include <Atomic/Input/Input.h>

#include <Atomic/IO/FileSystem.h>
#include <Atomic/Resource/ResourceCache.h>
#include <Atomic/Resource/XMLFile.h>
#include <Atomic/Physics/PhysicsWorld.h>
#include <Atomic/UI/UI.h>
#include <Atomic/UI/UIEvents.h>

#include <Atomic/Resource/ResourceEvents.h>

#include <ToolCore/Assets/Asset.h>
#include <ToolCore/Assets/AssetDatabase.h>

#include "../../EditorMode/AEEditorEvents.h"

#include "SceneView3D.h"
#include "SceneEditor3D.h"
#include "SceneEditor3DEvents.h"
#include "SceneSelection.h"

using namespace ToolCore;

namespace AtomicEditor
{

SceneView3D ::SceneView3D(Context* context, SceneEditor3D *sceneEditor) :
    UISceneView(context),
    yaw_(0.0f),
    pitch_(0.0f),
    mouseLeftDown_(false),
    mouseMoved_(false),
    enabled_(true),
    cameraMove_(false),
    cameraMoveSpeed_(20.0f),
    gridEnabled_(false),
    newCameraYPos_(0),
    perspectCamPosition_(0, 0, 0),
    perspectiveYaw_(0),
    perspectivePitch_(0),
    perspectivePositionSaved_(false)
{

    sceneEditor_ = sceneEditor;

    ResourceCache* cache = GetSubsystem<ResourceCache>();

    scene_ = sceneEditor->GetScene();

    debugRenderer_ = scene_->GetComponent<DebugRenderer>();

    if (debugRenderer_.Null())
    {
        debugRenderer_ = scene_->CreateComponent<DebugRenderer>();
        debugRenderer_->SetTemporary(true);
    }

    octree_ = scene_->GetComponent<Octree>();

    if (octree_.Null())
    {
        LOGWARNING("Scene without an octree loaded");
        octree_ = scene_->CreateComponent<Octree>();
    }

    cameraNode_ = scene_->CreateChild("__atomic_sceneview3d_camera");
    cameraNode_->SetTemporary(true);
    camera_ = cameraNode_->CreateComponent<Camera>();

    debugRenderer_ = scene_->GetComponent<DebugRenderer>();
    assert(debugRenderer_.NotNull());
    octree_ = scene_->GetComponent<Octree>();
    assert(octree_.NotNull());

    cameraNode_->SetPosition(Vector3(0, 0, -10));

    SetView(scene_, camera_);
    SetAutoUpdate(false);

    SubscribeToEvent(E_UPDATE, HANDLER(SceneView3D, HandleUpdate));
    SubscribeToEvent(E_POSTRENDERUPDATE, HANDLER(SceneView3D, HandlePostRenderUpdate));

    SubscribeToEvent(E_MOUSEMOVE, HANDLER(SceneView3D,HandleMouseMove));

    SubscribeToEvent(this, E_DRAGENTERWIDGET, HANDLER(SceneView3D, HandleDragEnterWidget));
    SubscribeToEvent(this, E_DRAGEXITWIDGET, HANDLER(SceneView3D, HandleDragExitWidget));
    SubscribeToEvent(this, E_DRAGENDED, HANDLER(SceneView3D, HandleDragEnded));

    SubscribeToEvent(E_UIUNHANDLEDSHORTCUT, HANDLER(SceneView3D, HandleUIUnhandledShortcut));
    SubscribeToEvent(E_UIWIDGETFOCUSESCAPED, HANDLER(SceneView3D, HandleUIWidgetFocusEscaped));

    SetIsFocusable(true);

}

SceneView3D::~SceneView3D()
{

}

void SceneView3D::Enable()
{
    if (enabled_)
        return;

    enabled_ = true;

}

void SceneView3D::Disable()
{
    if (!enabled_)
        return;

    enabled_ = false;

}

bool SceneView3D::GetOrbitting()
{
    Input* input = GetSubsystem<Input>();
    return framedBBox_.defined_ && MouseInView() && input->GetKeyDown(KEY_ALT) && input->GetMouseButtonDown(MOUSEB_LEFT);
}

bool SceneView3D::GetZooming()
{
    Input* input = GetSubsystem<Input>();
    return MouseInView() && input->GetMouseMoveWheel() && !input->GetMouseButtonDown(MOUSEB_RIGHT);
}

bool SceneView3D::GetChangingCameraSpeed()
{
    Input* input = GetSubsystem<Input>();
    return MouseInView() && input->GetMouseMoveWheel() && input->GetMouseButtonDown(MOUSEB_RIGHT);
}

void SceneView3D::CheckCameraSpeedBounds()
{
    const float MAX_CAMERA_SPEED = 80.0f;
    const float MIN_CAMERA_SPEED = 2.0f;

    if (cameraMoveSpeed_ >= MAX_CAMERA_SPEED)
    {
        cameraMoveSpeed_ = MAX_CAMERA_SPEED;
    }
    if (cameraMoveSpeed_ <= MIN_CAMERA_SPEED)
        cameraMoveSpeed_ = MIN_CAMERA_SPEED;
}


void SceneView3D::MoveCamera(float timeStep)
{
    if (!enabled_ && !GetFocus())
        return;

    Input* input = GetSubsystem<Input>();
    bool shiftDown = false;
    if (input->GetKeyDown(KEY_LSHIFT) || input->GetKeyDown(KEY_RSHIFT))
        shiftDown = true;

    bool mouseInView = MouseInView();
    bool orbitting = GetOrbitting();
    bool zooming = GetZooming();
    bool changingCameraSpeed = GetChangingCameraSpeed();

    // Mouse sensitivity as degrees per pixel
    const float MOUSE_SENSITIVITY = 0.2f;
    // Tempo at which mouse speed increases using mousewheel
    const float CAMERA_MOVE_TEMPO = 5.0f;
    // Tempo used when zooming in and out
    const float ZOOM_TEMPO = 0.6f;

    // Use this frame's mouse motion to adjust camera node yaw and pitch. Clamp the pitch between -90 and 90 degrees
    if ((mouseInView && input->GetMouseButtonDown(MOUSEB_RIGHT)) || orbitting)
    {
        SetFocus();
        IntVector2 mouseMove = input->GetMouseMove();
        yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
        pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
        pitch_ = Clamp(pitch_, -90.0f, 90.0f);
    }

    // Construct new orientation for the camera scene node from yaw and pitch. Roll is fixed to zero
    Quaternion q(pitch_, yaw_, 0.0f);

    if (!zooming || !changingCameraSpeed)
        cameraNode_->SetRotation(q);

    if (orbitting)
    {
        zooming = false;
        BoundingBox bbox;
        sceneEditor_->GetSelection()->GetBounds(bbox);
        if (bbox.defined_)
        {
            Vector3 centerPoint = bbox.Center();
            Vector3 d = cameraNode_->GetWorldPosition() - centerPoint;
            cameraNode_->SetWorldPosition(centerPoint - q * Vector3(0.0, 0.0, d.Length()));

        }
    }

    if (zooming)
    {
        orbitting = false;
        Ray ray = GetCameraRay();
        Vector3 wpos = cameraNode_->GetWorldPosition();
        wpos += ray.direction_ * (float(input->GetMouseMoveWheel()) * ZOOM_TEMPO);
        cameraNode_->SetWorldPosition(wpos);
    }

    if (changingCameraSpeed)
    {
        if (input->GetMouseMoveWheel() > 0)
            cameraMoveSpeed_ += input->GetMouseMoveWheel() * CAMERA_MOVE_TEMPO;
        else
            if (input->GetMouseMoveWheel() < 0)
                cameraMoveSpeed_ += input->GetMouseMoveWheel() * CAMERA_MOVE_TEMPO;

        CheckCameraSpeedBounds();
    }

#ifdef ATOMIC_PLATFORM_WINDOWS
    bool superdown = input->GetKeyDown(KEY_LCTRL) || input->GetKeyDown(KEY_RCTRL);
#else
    bool superdown = input->GetKeyDown(KEY_LGUI) || input->GetKeyDown(KEY_RGUI);
#endif

    if (!orbitting && mouseInView && !superdown && input->GetMouseButtonDown(MOUSEB_RIGHT)) {

        // Read WASD keys and move the camera scene node to the corresponding direction if they are pressed
        // Use the Translate() function (default local space) to move relative to the node's orientation.
        if (input->GetKeyDown(KEY_W))
        {
            SetFocus();
            cameraNode_->Translate(Vector3::FORWARD * cameraMoveSpeed_ * timeStep);
        }
        if (input->GetKeyDown(KEY_S))
        {
            SetFocus();
            cameraNode_->Translate(Vector3::BACK * cameraMoveSpeed_ * timeStep);
        }
        if (input->GetKeyDown(KEY_A))
        {
            SetFocus();
            cameraNode_->Translate(Vector3::LEFT * cameraMoveSpeed_ * timeStep);
        }
        if (input->GetKeyDown(KEY_D))
        {
            SetFocus();
            cameraNode_->Translate(Vector3::RIGHT * cameraMoveSpeed_ * timeStep);
        }
        if (input->GetKeyDown(KEY_E))
        {
            SetFocus();
            cameraNode_->Translate(Vector3::UP * cameraMoveSpeed_ * timeStep);
        }
        if (input->GetKeyDown(KEY_Q))
        {
            SetFocus();
            cameraNode_->Translate(Vector3::DOWN * cameraMoveSpeed_ * timeStep);
        }
    }

    if (cameraMove_)
    {

        cameraMoveTime_ += timeStep * 3.0f;

        if (cameraMoveTime_ > 1.0f)
        {
            cameraMove_ = false;
            cameraMoveTime_ = 1.0f;
        }

        Vector3 pos = cameraMoveStart_.Lerp(cameraMoveTarget_, cameraMoveTime_);
        cameraNode_->SetWorldPosition(pos);

    }
}

void SceneView3D::SnapCameraToView(int snapView)
{
    Vector3 selectedNodePos(0, 0, 0);

    // the distance the camera snaps from the selected object
    const int DISTANCE = 5;

    int xVal = 0;
    int yVal = 0;
    int zVal = 0;

    camera_->SetOrthographic(true);
    perspectivePositionSaved_ = true;

    // if no object is selected will snap to view relative to camera position
    if (sceneEditor_->GetSelection()->GetNodes().Size() > 0)
    {
        selectedNodePos = sceneEditor_->GetSelection()->GetSelectedNode(0)->GetPosition();

        switch (snapView)
        {
            case 1:
                selectedNodePos.y_ = selectedNodePos.y_ + DISTANCE;
                break;
            case 2:
                selectedNodePos.y_ = selectedNodePos.y_ - DISTANCE;
                break;
            case 3:
                selectedNodePos.x_ = selectedNodePos.x_ + DISTANCE;
                break;
            case 4:
                selectedNodePos.x_ = selectedNodePos.x_ - DISTANCE;
                break;
            case 5:
                selectedNodePos.z_ = selectedNodePos.z_ - DISTANCE;
                break;
            case 6:
                selectedNodePos.z_ = selectedNodePos.z_ + DISTANCE;
                break;
        }
        cameraNode_->SetPosition(selectedNodePos);
    }
    else
        cameraNode_->SetPosition(cameraNode_->GetPosition());


    cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0));

}

void SceneView3D::SelectView()
{
    Input* input = GetSubsystem<Input>();
    Vector3 camPos(0, 0, 0);

    int snapView = 0;

    if (MouseInView())
    {
        if (input->GetKeyPress(KEY_1))
        {
            snapView = 1;
            SetPerspectiveCameraPosition();
        }
        if (input->GetKeyPress(KEY_2))
        {
            snapView = 2;
            SetPerspectiveCameraPosition();
        }
        if (input->GetKeyPress(KEY_3))
        {
            snapView = 3;
            SetPerspectiveCameraPosition();
        }
        if (input->GetKeyPress(KEY_4))
        {
            snapView = 4;
            SetPerspectiveCameraPosition();
        }
        if (input->GetKeyPress(KEY_5))
        {
            snapView = 5;
            SetPerspectiveCameraPosition();
        }
        if (input->GetKeyPress(KEY_6))
        {
            snapView = 6;
            SetPerspectiveCameraPosition();
        }

        if (input->GetKeyPress(KEY_P))
        {
            perspectivePositionSaved_ = false;
            camera_->SetOrthographic(false);

            pitch_ = perspectivePitch_;
            yaw_ = perspectiveYaw_;
            cameraNode_->SetPosition(perspectCamPosition_);
            cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0));
        }

        if (input->GetKeyPress(KEY_O))
        {
            if (camera_->IsOrthographic())
                perspectivePositionSaved_ = true;

            if (!camera_->IsOrthographic())
                perspectivePositionSaved_ = false;

            camera_->SetOrthographic(!camera_->IsOrthographic());
        }

    }

    switch (snapView)
    {
        case 1:
            // View top
            yaw_ = 0;
            pitch_ = 90;
            SnapCameraToView(snapView);
            break;
        case 2:
            // View bottom
            yaw_ = 0;
            pitch_ = -90;
            SnapCameraToView(snapView);
            break;
        case 3:
            // View left
            yaw_ = -90;
            pitch_ = 0;
            SnapCameraToView(snapView);
            break;
        case 4:
            // View right
            yaw_ = 90;
            pitch_ = 0;
            SnapCameraToView(snapView);
            break;
        case 5:
            // View front
            yaw_ = 0;
            pitch_ = 0;
            SnapCameraToView(snapView);
            break;
        case 6:
            // View back
            yaw_ = 180;
            pitch_ = 0;
            SnapCameraToView(snapView);
            break;
        }
}

void SceneView3D::SetPerspectiveCameraPosition()
{
    if (perspectivePositionSaved_)
        return;

    if (!perspectivePositionSaved_ && !camera_->IsOrthographic())
    {
        perspectCamPosition_ = cameraNode_->GetPosition();
        perspectivePitch_ = pitch_;
        perspectiveYaw_ = yaw_;
    }
}

Ray SceneView3D::GetCameraRay()
{
    Ray camRay;

    Input* input = GetSubsystem<Input>();
    IntVector2 cpos = input->GetMousePosition();

    IntRect rect = GetRect();

    if (!rect.Width() || !rect.Height())
        return camRay;

    int x = rect.left_;
    int y = rect.top_;
    GetInternalWidget()->ConvertToRoot(x, y);

    return  camera_->GetScreenRay(float(cpos.x_ - x) / rect.Width(),
                                  float(cpos.y_ - y) / rect.Height());
}

bool SceneView3D::MouseInView()
{
    if (!GetInternalWidget())
        return false;

    if (!TBWidget::hovered_widget || TBWidget::hovered_widget->GetDelegate() != this)
        return false;

    Input* input = GetSubsystem<Input>();
    IntVector2 pos = input->GetMousePosition();

    IntRect rect = GetRect();

    GetInternalWidget()->ConvertToRoot(rect.left_, rect.top_);
    GetInternalWidget()->ConvertToRoot(rect.right_, rect.bottom_);

    return rect.IsInside(pos);

}

void SceneView3D::ToggleGrid()
{
    Input* input = GetSubsystem<Input>();

    if (input->GetKeyPress(KEY_G))
        gridEnabled_ = !gridEnabled_;
}

void SceneView3D::HandleUIUnhandledShortcut(StringHash eventType, VariantMap& eventData)
{
    if (!enabled_)
        return;

    unsigned id = eventData[UIUnhandledShortcut::P_REFID].GetUInt();

    if (id == TBIDC("undo"))
        sceneEditor_->Undo();
    else if (id == TBIDC("redo"))
        sceneEditor_->Redo();
    else if (id == TBIDC("copy"))
        sceneEditor_->Copy();
    else if (id == TBIDC("cut"))
        sceneEditor_->Cut();
    else if (id == TBIDC("paste"))
        sceneEditor_->Paste();

    return;

}

void SceneView3D::HandleUIWidgetFocusEscaped(StringHash eventType, VariantMap& eventData)
{
    if (!enabled_)
        return;

    SetFocus();
}

void SceneView3D::HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData)
{

    if (!MouseInView() || GetOrbitting())
        return;

    Input* input = GetSubsystem<Input>();

    mouseLeftDown_ = false;
    bool shiftDown = input->GetKeyDown(KEY_LSHIFT) || input->GetKeyDown(KEY_RSHIFT);

    if (input->GetMouseButtonPress(MOUSEB_LEFT))
    {
        SetFocus();

        if (!mouseMoved_ && !sceneEditor_->GetGizmo()->Selected())
        {
            Ray camRay  = GetCameraRay();
            PODVector<RayQueryResult> result;

            RayOctreeQuery query(result, camRay, RAY_TRIANGLE, camera_->GetFarClip(), DRAWABLE_GEOMETRY, 0x7fffffff);
            octree_->RaycastSingle(query);

            if (query.result_.Size())
            {
                const RayQueryResult& r = result[0];

                if (r.drawable_)
                {

                    VariantMap neventData;
                    Node* node = r.drawable_->GetNode();

                    // if temporary, this is a prefab
                    // TODO: if we use temporary for other stuff
                    // fix this to look for prefab
                    if (node->IsTemporary())
                        node = node->GetParent();

                    if (sceneEditor_->GetSelection()->Contains(node) && shiftDown)
                    {
                        sceneEditor_->GetSelection()->RemoveNode(node);
                    }
                    else
                    {
                        sceneEditor_->GetSelection()->AddNode(node, !shiftDown);
                    }

                }
            }
            else
            {
                sceneEditor_->GetSelection()->Clear();
            }
        }

        mouseMoved_ = false;

    }
    else if (!input->GetMouseButtonDown(MOUSEB_LEFT))
    {

        Ray camRay  = GetCameraRay();
        PODVector<RayQueryResult> result;

        mouseMoved_ = false;

        /*
        Array<int> pickModeDrawableFlags = {
            DRAWABLE_GEOMETRY,
            DRAWABLE_LIGHT,
            DRAWABLE_ZONE
        };
        */

        RayOctreeQuery query(result, camRay, RAY_TRIANGLE, camera_->GetFarClip(), DRAWABLE_GEOMETRY, 0x7fffffff);
        octree_->RaycastSingle(query);

        if (query.result_.Size())
        {
            const RayQueryResult& r = result[0];

            if (r.drawable_)
            {
                debugRenderer_->AddNode(r.drawable_->GetNode(), 1.0, false);
                r.drawable_->DrawDebugGeometry(debugRenderer_, false);
            }
        }
    }
    else
    {
        mouseLeftDown_ = true;
        if (Abs(input->GetMouseMoveX() > 3 || input->GetMouseMoveY() >  3))
        {
            mouseMoved_ = true;
        }
    }

}

bool SceneView3D::OnEvent(const TBWidgetEvent &ev)
{
    if (ev.type == EVENT_TYPE_SHORTCUT)
    {
        if (ev.ref_id == TBIDC("close"))
            return false;
    }
    if (ev.type == EVENT_TYPE_KEY_UP)
    {
        if (ev.special_key == TB_KEY_ESC)
        {
            sceneEditor_->GetSelection()->Clear();
        }
    }
    if (ev.type == EVENT_TYPE_KEY_DOWN)
        ToggleGrid();

    if (ev.type == EVENT_TYPE_KEY_DOWN)
            SelectView();

    return sceneEditor_->OnEvent(ev);
}


void SceneView3D::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
    int FARCLIP_INCREMENT = 8;

    // parent is the contentRoot for our tab, when tab isn't active it will not be visible
    if (!GetParent() || GetParent()->GetVisibility() != UI_WIDGET_VISIBILITY_VISIBLE)
    {
        Disable();
        return;
    }

    Enable();

    // Timestep parameter is same no matter what event is being listened to
    float timeStep = eventData[Update::P_TIMESTEP].GetFloat();

    MoveCamera(timeStep);

    QueueUpdate();

    if (gridEnabled_)
    {
        int cameraYPos = cameraNode_->GetPosition().y_;
        int farClip = camera_->GetFarClip();

        if (cameraYPos == newCameraYPos_)
        {
            farClip = camera_->GetFarClip();
        }
        else
        {
            if (cameraYPos > newCameraYPos_)
            {
                farClip += FARCLIP_INCREMENT;
                camera_->SetFarClip(farClip);
            }
            else
            {
                farClip -= FARCLIP_INCREMENT;
                camera_->SetFarClip(farClip);
            }
        }
        newCameraYPos_ = cameraYPos;

        debugRenderer_->CreateGrid(Color::GRAY, true, cameraNode_->GetPosition());
    }

    if (preloadResourceScene_.NotNull())
    {
        if (preloadResourceScene_->GetAsyncProgress() == 1.0f)
        {
            ResourceCache* cache = GetSubsystem<ResourceCache>();
            XMLFile* xml = cache->GetResource<XMLFile>(dragAssetGUID_);

            if (dragNode_.NotNull())
            {
                dragNode_->LoadXML(xml->GetRoot());
                UpdateDragNode(0, 0);

                AnimationController* controller = dragNode_->GetComponent<AnimationController>();
                if (controller)
                {
                    controller->PlayExclusive("Idle", 0, true);

                    dragNode_->GetScene()->SetUpdateEnabled(true);
                }
            }

            preloadResourceScene_ = 0;
            dragAssetGUID_ = "";

        }
    }

}


void SceneView3D::UpdateDragNode(int mouseX, int mouseY)
{
    if (dragNode_.Null())
        return;

    Ray ray = GetCameraRay();

    Vector3 pos = ray.origin_;
    pos += ray.direction_ * 10;

    dragNode_->SetWorldPosition(pos);

}

void SceneView3D::HandleMouseMove(StringHash eventType, VariantMap& eventData)
{
    if (dragNode_.Null())
        return;

    Input* input = GetSubsystem<Input>();

    if (!input->IsMouseVisible())
        return;

    using namespace MouseMove;

    int x = eventData[P_X].GetInt();
    int y = eventData[P_Y].GetInt();

    UpdateDragNode(x, y);

}

void SceneView3D::HandleDragEnterWidget(StringHash eventType, VariantMap& eventData)
{
    using namespace DragEnterWidget;

    UIWidget* widget = static_cast<UIWidget*>(eventData[P_WIDGET].GetPtr());

    if (widget != this)
        return;

    UIDragObject* dragObject = static_cast<UIDragObject*>(eventData[P_DRAGOBJECT].GetPtr());

    Object* object = dragObject->GetObject();

    if (!object)
        return;

    if (object->GetType() == Asset::GetTypeStatic())
    {
        Asset* asset = (Asset*) object;
        dragNode_ = asset->InstantiateNode(scene_, asset->GetName());

        if (dragNode_.NotNull())
        {
            Input* input = GetSubsystem<Input>();
            IntVector2 pos = input->GetMousePosition();
            UpdateDragNode(pos.x_, pos.y_);
        }

        //LOGINFOF("Dropped %s : %s on SceneView3D", asset->GetPath().CString(), asset->GetGUID().CString());
    }

}

void SceneView3D::HandleDragExitWidget(StringHash eventType, VariantMap& eventData)
{
    if (preloadResourceScene_.NotNull())
    {
        preloadResourceScene_->StopAsyncLoading();
        preloadResourceScene_ = 0;
    }

    if (dragNode_.NotNull())
    {
        scene_->RemoveChild(dragNode_);
    }

    dragAssetGUID_ = "";
    dragNode_ = 0;
}


void SceneView3D::HandleDragEnded(StringHash eventType, VariantMap& eventData)
{
    using namespace DragEnded;

    UIDragObject* dragObject = static_cast<UIDragObject*>(eventData[P_DRAGOBJECT].GetPtr());

    if (dragNode_.NotNull())
    {
        VariantMap nodeCreatedEvent;
        nodeCreatedEvent[SceneEditNodeCreated::P_NODE] = dragNode_;
        scene_->SendEvent(E_SCENEEDITNODECREATED, nodeCreatedEvent);
    }

    if (dragObject && dragObject->GetObject()->GetType() == ToolCore::Asset::GetTypeStatic())
    {
        Asset* asset = (ToolCore::Asset*) dragObject->GetObject();

        if (asset->GetImporterTypeName() == "MaterialImporter") {

            Material* material = GetSubsystem<ResourceCache>()->GetResource<Material>(asset->GetPath());

            if (material) {

                material = material;

                Ray camRay  = GetCameraRay();

                PODVector<RayQueryResult> result;

                RayOctreeQuery query(result, camRay, RAY_TRIANGLE, camera_->GetFarClip(), DRAWABLE_GEOMETRY, 0x7fffffff);
                octree_->RaycastSingle(query);

                if (query.result_.Size())
                {
                    const RayQueryResult& r = result[0];

                    if (r.drawable_ && (r.drawable_->GetType() == StaticModel::GetTypeStatic() || r.drawable_->GetType() == AnimatedModel::GetTypeStatic()))
                    {
                        ((StaticModel*)r.drawable_)->SetMaterial(material);

                    }
                }

            }

        }

    }

    dragAssetGUID_ = "";
    dragNode_ = 0;
}

void SceneView3D::FrameSelection()
{
    BoundingBox bbox;
    sceneEditor_->GetSelection()->GetBounds(bbox);

    if (!bbox.defined_)
        return;

    Sphere sphere(bbox);

    if (sphere.radius_ < .01f || sphere.radius_ > 512)
        return;

    framedBBox_ = bbox;
    cameraMoveStart_ = cameraNode_->GetWorldPosition();
    cameraMoveTarget_ = bbox.Center() - (cameraNode_->GetWorldDirection() * sphere.radius_ * 3);
    cameraMoveTime_ = 0.0f;
    cameraMove_ = true;

}


}
