//
// Copyright (c) 2014-2015, THUNDERBEAST GAMES LLC All rights reserved
// LICENSE: Atomic Game Engine Editor and Tools EULA
// Please see LICENSE_ATOMIC_EDITOR_AND_TOOLS.md in repository root for
// license information: https://github.com/AtomicGameEngine/AtomicGameEngine
//

#include <Atomic/Atomic.h>
#include <Atomic/Engine/Engine.h>
#include <Atomic/Engine/EngineConfig.h>
#include <Atomic/IO/FileSystem.h>
#include <Atomic/IO/Log.h>
#include <Atomic/IO/IOEvents.h>
#include <Atomic/Input/InputEvents.h>
#include <Atomic/Core/Main.h>
#include <Atomic/Core/ProcessUtils.h>
#include <Atomic/Resource/ResourceCache.h>
#include <Atomic/Resource/ResourceEvents.h>
#include <Atomic/UI/UI.h>

// Move me
#include <Atomic/Environment/Environment.h>

#include <AtomicJS/Javascript/Javascript.h>

#ifdef ATOMIC_DOTNET
#include <AtomicNET/NETCore/NETCore.h>
#include <AtomicNET/NETScript/NETScript.h>
#endif


#include "../PlayerMode/AEPlayerMode.h"
#include <AtomicPlayer/Player.h>

#include "../PlayerMode/AEPlayerEvents.h"

#include "AEPlayerApp.h"

#include <Atomic/DebugNew.h>

#ifdef __APPLE__
#include <unistd.h>
#endif

namespace AtomicPlayer
{
    extern void jsapi_init_atomicplayer(JSVM* vm);
}

// Luma
extern void vse_lib_init(Context* context, JSVM* vm);

namespace AtomicEditor
{

AEPlayerApplication::AEPlayerApplication(Context* context) :
    AEEditorCommon(context),
    debugPlayer_(false),
    runningFromEditorPlay_(false)
{
}

void AEPlayerApplication::Setup()
{
    AEEditorCommon::Setup();

    // Read the project engine configuration
    ReadEngineConfig();

    engine_->SetAutoExit(false);

    engineParameters_.InsertNew("WindowTitle", "AtomicPlayer");

    // Set defaults not already set from config
#if (ATOMIC_PLATFORM_ANDROID)
    engineParameters_.InsertNew("FullScreen", true);
    engineParameters_.InsertNew("ResourcePaths", "CoreData;AtomicResources");
#elif ATOMIC_PLATFORM_WEB
    engineParameters_.InsertNew("FullScreen", false);
    engineParameters_.InsertNew("ResourcePaths", "AtomicResources");
    // engineParameters_.InsertNew("WindowWidth", 1280);
    // engineParameters_.InsertNew("WindowHeight", 720);
#elif ATOMIC_PLATFORM_IOS
    engineParameters_.InsertNew("FullScreen", false);
    engineParameters_.InsertNew("ResourcePaths", "AtomicResources)";
#else
    engineParameters_.InsertNew("FullScreen", false);
    engineParameters_.InsertNew("WindowWidth", 1280);
    engineParameters_.InsertNew("WindowHeight", 720);
#endif

    engineParameters_.InsertNew("LogLevel", LOG_DEBUG);

#if ATOMIC_PLATFORM_WINDOWS || ATOMIC_PLATFORM_LINUX
    engineParameters_.InsertNew("WindowIcon", "Images/AtomicLogo32.png");
    engineParameters_.InsertNew("ResourcePrefixPath", "AtomicPlayer_Resources");
#elif ATOMIC_PLATFORM_ANDROID
    //engineParameters_.InsertNew("ResourcePrefixPath", "assets");
#elif ATOMIC_PLATFORM_OSX
    engineParameters_.InsertNew("ResourcePrefixPath", "../Resources");
#endif

    // Read command line arguments, potentially overwriting project settings
    ReadCommandLineArguments();

    // Re-apply project settings if running from editor play button
    if (runningFromEditorPlay_)
        EngineConfig::ApplyConfig(engineParameters_, true);

    // Use the script file name as the base name for the log file
    engineParameters_["LogName"] = GetSubsystem<FileSystem>()->GetAppPreferencesDir("AtomicPlayer", "Logs") + "AtomicPlayer.log";
}

void AEPlayerApplication::ReadEngineConfig()
{
    // find the project path from the command line args

    String projectPath;
    const Vector<String>& arguments = GetArguments();

    for (unsigned i = 0; i < arguments.Size(); ++i)
    {
        if (arguments[i].Length() > 1)
        {
            String argument = arguments[i].ToLower();
            String value = i + 1 < arguments.Size() ? arguments[i + 1] : String::EMPTY;

            if (argument == "--project" && value.Length())
            {
                projectPath = AddTrailingSlash(value);
                break;
            }

        }
    }

    if (!projectPath.Length())
        return;

    String filename = projectPath + "Settings/Engine.json";

    FileSystem* fileSystem = GetSubsystem<FileSystem>();
    if (!fileSystem->FileExists(filename))
        return;

    if (EngineConfig::LoadFromFile(context_, filename))
    {
        EngineConfig::ApplyConfig(engineParameters_);
    }

}

void AEPlayerApplication::ReadCommandLineArguments()
{
    const Vector<String>& arguments = GetArguments();
    FileSystem* fileSystem = GetSubsystem<FileSystem>();

    for (unsigned i = 0; i < arguments.Size(); ++i)
    {
        if (arguments[i].Length() > 1)
        {
            String argument = arguments[i].ToLower();
            String value = i + 1 < arguments.Size() ? arguments[i + 1] : String::EMPTY;

            if (argument == "--log-std")
            {
                SubscribeToEvent(E_LOGMESSAGE, HANDLER(AEPlayerApplication, HandleLogMessage));
            }
            else if (argument == "--fromeditorplay")
            {
                runningFromEditorPlay_ = true;
            }
            else if (argument == "--debug")
            {
                debugPlayer_ = true;
            }
            else if (argument == "--project" && value.Length())
            {
                engineParameters_["ResourcePrefixPath"] = "";

                value = AddTrailingSlash(value);

                // check that cache exists
                if (!fileSystem->DirExists(value + "Cache"))
                {
                    ErrorExit("Project cache folder does not exist, projects must be loaded into the Atomic Editor at least once before using the --player command line mode");
                    return;
                }

#ifdef ATOMIC_DEV_BUILD

                String resourcePaths = ToString("%s/Resources/CoreData;%s/Resources/PlayerData;%sResources;%s;%sCache",
                    ATOMIC_ROOT_SOURCE_DIR, ATOMIC_ROOT_SOURCE_DIR, value.CString(), value.CString(), value.CString());

#else

#ifdef __APPLE__
                engineParameters_["ResourcePrefixPath"] = "../Resources";
#else
                engineParameters_["ResourcePrefixPath"] = fileSystem->GetProgramDir() + "Resources";
#endif

                String resourcePaths = ToString("CoreData;PlayerData;%s/;%s/Resources;%s;%sCache",
                    value.CString(), value.CString(), value.CString(), value.CString());
#endif

                LOGINFOF("Adding ResourcePaths: %s", resourcePaths.CString());

                engineParameters_["ResourcePaths"] = resourcePaths;

#ifdef ATOMIC_DOTNET
                NETCore* netCore = GetSubsystem<NETCore>();
                String assemblyLoadPath = GetNativePath(ToString("%sResources/Assemblies/", value.CString()));
                netCore->AddAssemblyLoadPath(assemblyLoadPath);
#endif

            }
            else if (argument == "--windowposx" && value.Length())
            {
                engineParameters_["WindowPositionX"] = atoi(value.CString());
            }
            else if (argument == "--windowposy" && value.Length())
            {
                engineParameters_["WindowPositionY"] = atoi(value.CString());
            }
            else if (argument == "--windowwidth" && value.Length())
            {
                engineParameters_["WindowWidth"] = atoi(value.CString());
            }
            else if (argument == "--windowheight" && value.Length())
            {
                engineParameters_["WindowHeight"] = atoi(value.CString());
            }
            else if (argument == "--resizable")
            {
                engineParameters_["WindowResizable"] = true;
            }
            else if (argument == "--maximize")
            {
                engineParameters_["WindowMaximized"] = true;
            }
        }
    }
}

void AEPlayerApplication::Start()
{
    AEEditorCommon::Start();

    UI* ui = GetSubsystem<UI>();
    ui->Initialize("DefaultUI/language/lng_en.tb.txt");
    ui->LoadDefaultPlayerSkin();

    context_->RegisterSubsystem(new PlayerMode(context_));
    PlayerMode* playerMode = GetSubsystem<PlayerMode>();
    playerMode->ProcessArguments();

    SubscribeToEvent(E_JSERROR, HANDLER(AEPlayerApplication, HandleJSError));

#ifdef ATOMIC_DOTNET
        if (debugPlayer_)
        {
           GetSubsystem<NETCore>()->WaitForDebuggerConnect();
        }
#endif

    vm_->SetModuleSearchPaths("Modules");

    // Instantiate and register the Player subsystem
    context_->RegisterSubsystem(new AtomicPlayer::Player(context_));
    AtomicPlayer::jsapi_init_atomicplayer(vm_);

#ifdef ATOMIC_DOTNET
    // Initialize Scripting Subsystem
    NETScript* netScript = new NETScript(context_);
    context_->RegisterSubsystem(netScript);
    netScript->Initialize();
    netScript->ExecMainAssembly();
#endif

    // Luma
    vse_lib_init(context_, vm_);

    if (!playerMode->launchedByEditor())
    {
        JSVM* vm = JSVM::GetJSVM(0);

        if (!vm->ExecuteMain())
        {
            SendEvent(E_EXITREQUESTED);
        }
    }

    SubscribeToEvent(E_PLAYERQUIT, HANDLER(AEPlayerApplication, HandleQuit));

    return;
}

void AEPlayerApplication::HandleQuit(StringHash eventType, VariantMap& eventData)
{
    engine_->Exit();
}

void AEPlayerApplication::Stop()
{
    AEEditorCommon::Stop();
}

void AEPlayerApplication::HandleScriptReloadStarted(StringHash eventType, VariantMap& eventData)
{
}

void AEPlayerApplication::HandleScriptReloadFinished(StringHash eventType, VariantMap& eventData)
{
}

void AEPlayerApplication::HandleScriptReloadFailed(StringHash eventType, VariantMap& eventData)
{
    ErrorExit();
}

void AEPlayerApplication::HandleLogMessage(StringHash eventType, VariantMap& eventData)
{
    using namespace LogMessage;

    int level = eventData[P_LEVEL].GetInt();
    // The message may be multi-line, so split to rows in that case
    Vector<String> rows = eventData[P_MESSAGE].GetString().Split('\n');

    for (unsigned i = 0; i < rows.Size(); ++i)
    {
        if (level == LOG_ERROR)
        {
            fprintf(stderr, "%s\n", rows[i].CString());
        }
        else
        {
            fprintf(stdout, "%s\n", rows[i].CString());
        }
    }

}

void AEPlayerApplication::HandleJSError(StringHash eventType, VariantMap& eventData)
{
    using namespace JSError;
    //String errName = eventData[P_ERRORNAME].GetString();
    //String errStack = eventData[P_ERRORSTACK].GetString();
    String errMessage = eventData[P_ERRORMESSAGE].GetString();
    String errFilename = eventData[P_ERRORFILENAME].GetString();
    int errLineNumber = eventData[P_ERRORLINENUMBER].GetInt();

    String errorString = ToString("%s - %s - Line: %i",
                                  errFilename.CString(), errMessage.CString(), errLineNumber);

}


}
