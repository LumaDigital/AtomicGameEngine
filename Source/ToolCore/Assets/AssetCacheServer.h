#pragma once

#include <Atomic/Network/Connection.h>
#include <Atomic/Container/HashSet.h>
#include <Atomic/Container/List.h>
#include <Atomic/Core/Mutex.h>
#include <Atomic/IO/File.h>
#include <Atomic/Resource/Resource.h>

#include "AssetCacheFileReceiver.h"
#include "AssetCacheFileSender.h"
#include "AssetCacheServerConnectedClient.h"


using namespace Atomic;

namespace ToolCore
{
    static const String assetCacheClientIdentifier = "AssetCacheClient";

    class AssetCacheServer : public Object
    {
        ATOMIC_OBJECT(AssetCacheServer, Object);


    public:

        enum MessageID
        {
            //CLIENT MESSAGES
            REQUEST_DOWNLOAD = 100,
            REQUEST_UPLOAD,
            UPLOAD_FAILED_ERROR,

            //SERVER MESSAGES
            CONNECTION_ACCEPTED,
            DOWNLOAD_REQUEST_REJECT_UNAVAILABLE,
            DOWNLOAD_FAILED_ERROR,
            UPLOAD_REQUEST_ACCEPT,
            UPLOAD_REQUEST_REJECT_FILE_EXISTS,
            UPLOAD_COMPLETE,
            
            //Shared FILE TRANSFER MESSAGES
            FILE_TRANSFER_START,
            FILE_TRANSFER_FRAGMENT,

            //SERVER FILE COMMANDS
            FETCH_ASSET_FILE_SUCCESS,
            FETCH_ASSET_FILE_FAILURE,
            ADD_ASSET_FILE_SUCCESS,
            ADD_ASSET_FILE_FAILURE
        };

        /// Construct.
        AssetCacheServer(Context* context, String cacheFolderPath, unsigned short port, unsigned diskSpaceLimit);
        /// Destruct.
        virtual ~AssetCacheServer();

        bool Start();
        void Stop();

    private:

        String GetCachePathForFile(String fileName);

        void ValidateClientIdentity(StringHash eventType, VariantMap& eventData);

        void OnClientConnected(StringHash eventType, VariantMap& eventData);
        void OnClientDisconnected(StringHash eventType, VariantMap& eventData);

        SharedPtr<AssetCacheServerConnectedClient> GetConnectedClient(Connection* client);
        void AddConnectedClient(Connection* client);
        void RemoveConnectedClient(Connection* client);

        unsigned short port_;
        String cacheFolderPath_;
        unsigned diskSpaceLimitMB_;

        Vector<SharedPtr<AssetCacheServerConnectedClient>> connectedClients_;
    };
}