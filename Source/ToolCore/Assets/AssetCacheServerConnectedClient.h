#pragma once

#include <Atomic/Network/Connection.h>
#include <Atomic/Container/HashSet.h>
#include <Atomic/Container/List.h>
#include <Atomic/IO/File.h>

#include "AssetCacheFileReceiver.h"
#include "AssetCacheFileSender.h"

using namespace Atomic;

namespace ToolCore
{
    // server helper class representing a connected client.
    // manages file transfer to and from the client.
    class AssetCacheServerConnectedClient : public Object
    {
        ATOMIC_OBJECT(AssetCacheServerConnectedClient, Object);

    public:

        AssetCacheServerConnectedClient(Context* context, Connection* client, String cacheFolderPath, unsigned diskSpaceLimit);

        void HandleBeginFrame(StringHash eventType, VariantMap& eventData);
        void Update(float timeStep);

        void HandleClientMessage(StringHash eventType, VariantMap& eventData);

        void OnClientRequestDownload(String fileName);
        void OnClientRequestUpload(String fileName);
        void StartReceiveUpload(VariantMap& fileTransferData);

        const Connection* GetClient() { return client_; }
        const SharedPtr<AssetCacheFileReceiver> GetFileReceiveManager() { return fileReceiveManager_; }
        const SharedPtr<AssetCacheFileSender> GetFileSendManager() { return fileSendManager_; }

        void CancelUpload();

    private:

        String GetCachePathForFile(String fileName);
        bool EnsureAvailableCacheSpace(unsigned fileSize);

        Connection* client_;
        String cacheFolderPath_;
        unsigned diskSpaceLimitMB_;

        SharedPtr<AssetCacheFileReceiver> fileReceiveManager_;
        SharedPtr<AssetCacheFileSender> fileSendManager_;
    };
}