
#include <Atomic/IO/Log.h>
#include <Atomic/IO/File.h>
#include <Atomic/IO/FileSystem.h>
#include <Atomic/IO/MemoryBuffer.h>
#include <Atomic/Network/Network.h>

#include <kNet/MessageConnection.h>

#include <Atomic/Network/NetworkEvents.h>
#include <Atomic/Core/CoreEvents.h>

#include "AssetDatabase.h"
#include "AssetCacheServer.h"
#include "AssetCacheServerEvents.h"

using namespace Atomic;
using namespace ToolCore;

AssetCacheServer::AssetCacheServer(Context* context, String cacheFolderPath, unsigned short port, unsigned diskSpaceLimitMB) :
    Object(context),
    cacheFolderPath_(AddTrailingSlash(cacheFolderPath)),
    port_(port),
    diskSpaceLimitMB_(diskSpaceLimitMB)
{
    SubscribeToEvent(E_CLIENTIDENTITY, ATOMIC_HANDLER(AssetCacheServer, ValidateClientIdentity));
    SubscribeToEvent(E_CLIENTCONNECTED, ATOMIC_HANDLER(AssetCacheServer, OnClientConnected));
    SubscribeToEvent(E_CLIENTDISCONNECTED, ATOMIC_HANDLER(AssetCacheServer, OnClientDisconnected));
}

AssetCacheServer::~AssetCacheServer()
{
    Stop();
}

bool AssetCacheServer::Start()
{
    // check if we have a valid asset cache folder first
    FileSystem* fileSystem = GetSubsystem<FileSystem>();

    if (fileSystem->DirExists(cacheFolderPath_))
    {
        ATOMIC_LOGINFOF("AssetCacheServer found cache folder : %s", cacheFolderPath_.CString());
    }
    else
    {
        if (fileSystem->CreateDir(cacheFolderPath_))
        {
            ATOMIC_LOGINFOF("AssetCacheServer created cache folder : %s", cacheFolderPath_.CString());
        }
        else
        {
            ATOMIC_LOGERRORF("AssetCacheServer failed to find or create cache folder : %s", cacheFolderPath_.CString());
            return false;
        }
    }

    Network* network = GetSubsystem<Network>();

    network->StartServer(port_, kNet::SocketOverTCP);

    if (network->IsServerRunning())
    {
        ATOMIC_LOGINFOF("AssetCacheServer started network server on port %d", port_);
    }
    else
    {
        ATOMIC_LOGERRORF("AssetCacheServer failed to start network server on port %d", port_);
        return false;
    }
    
    return true;
}

void AssetCacheServer::Stop()
{
    connectedClients_.Clear();

    Network* network = GetSubsystem<Network>();
    network->StopServer();
}

void AssetCacheServer::ValidateClientIdentity(StringHash eventType, VariantMap& eventData)
{
    using namespace ClientIdentity;

    VariantMap connectionIdentity = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr())->GetIdentity();

    String id = connectionIdentity["id"].GetString();

    ATOMIC_LOGINFOF("AssetCacheServer received connection with id [%s]", id.CString());

    if (id == assetCacheClientIdentifier)
    {
        ATOMIC_LOGINFOF("Connection id valid, accepting");
        eventData[P_ALLOW] = true;
    }
    else
    {
        ATOMIC_LOGERRORF("Connection id invalid, rejecting");
        eventData[P_ALLOW] = false;
    }
}

void AssetCacheServer::OnClientConnected(StringHash eventType, VariantMap& eventData)
{
    using namespace ClientConnected;

    Connection* client = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());

    ATOMIC_LOGINFOF("Client connected");

    client->SendMessage((int)MessageID::CONNECTION_ACCEPTED, true, true, NULL, 0);

    AddConnectedClient(client);
}

void AssetCacheServer::OnClientDisconnected(StringHash eventType, VariantMap& eventData)
{   
    using namespace ClientDisconnected;

    Connection* client = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());

    ATOMIC_LOGINFOF("Client disconnected");
    
    RemoveConnectedClient(client);
    
}

String AssetCacheServer::GetCachePathForFile(String fileName)
{
    return cacheFolderPath_ + fileName;
}


SharedPtr<AssetCacheServerConnectedClient> AssetCacheServer::GetConnectedClient(Connection* client)
{
    for (Vector<SharedPtr<AssetCacheServerConnectedClient>>::Iterator i = connectedClients_.Begin(); i != connectedClients_.End(); i++)
    {
        if ((*i)->GetClient() == client)
        {
            return *i;
        }
    }

    return SharedPtr<AssetCacheServerConnectedClient>(nullptr);
}

void AssetCacheServer::AddConnectedClient(Connection* client)
{
    connectedClients_.Push(SharedPtr<AssetCacheServerConnectedClient>(new AssetCacheServerConnectedClient(context_, client, cacheFolderPath_, diskSpaceLimitMB_)));
}

void AssetCacheServer::RemoveConnectedClient(Connection* client)
{
    for (Vector<SharedPtr<AssetCacheServerConnectedClient>>::Iterator i = connectedClients_.Begin(); i != connectedClients_.End(); i++)
    {
        if ((*i)->GetClient() == client)
        {
            connectedClients_.Erase(i);
            return;
        }
    }

    ATOMIC_LOGERRORF("AssetCacheServer tried to remove a connected client, but it doesn't exist in our list!");
    
}