#pragma once

#include <Atomic/Core/Object.h>
#include <Atomic/IO/File.h>
#include <Atomic/Container/Vector.h>
#include <Atomic/Container/Pair.h>
#include <Atomic/Network/Network.h>

#include "AssetCacheManager.h"
#include "AssetCacheFileReceiver.h"
#include "AssetCacheFileSender.h"

using namespace Atomic;

namespace ToolCore
{

    class AssetCacheManagerNetwork : public AssetCacheManager
    {

        ATOMIC_OBJECT(AssetCacheManagerNetwork, AssetCacheManager);

    public:

        AssetCacheManagerNetwork(Context* context);

        /// requests a cache file, fires an event when it is loaded to subscribers
        virtual void FetchCacheFile(const String& fileName, const String& md5);

        /// process the fact that a file has been added to the cache (externally, by an asset importer etc)
        virtual void OnFileAddedToCache(const String& fileName, const String& md5);

    protected:

        enum ConnectionStatus
        {
            NOT_CONNECTED,
            CONNECTING,
            CONNECTED
        };

        enum CommsStatus
        {
            IDLE,
            FILE_DOWNLOAD_REQUESTED,
            FILE_DOWNLOADING,
            FILE_UPLOAD_REQUESTED,
            FILE_UPLOADING,
            FILE_UPLOAD_COMPLETED,
        };

        bool FetchCacheFileLocal(const String& fileName, const String& md5);
        void FetchCacheFileRemote(const String& fileName, const String& md5);
        bool ConnectToCacheServer();                
        
        bool RequestNextFileDownload();
        bool RequestNextFileUpload();

        void StartDownload(VariantMap& fileTransferData);
        void StartUpload();

        void ReQueueOutstandingRequests();

        bool HasPendingServerRequests();
        int NumPendingServerRequests();

        void OnConnected(StringHash eventType, VariantMap& eventData);
        void OnDisconnected(StringHash eventType, VariantMap& eventData);
        void OnConnectionFailed(StringHash eventType, VariantMap& eventData);

        void SetOutstandingServerRequestsToFailed();

        /// Handle message from the server
        void HandleServerMessage(StringHash eventType, VariantMap& eventData);
        /// Handle begin frame event.
        void HandleBeginFrame(StringHash eventType, VariantMap& eventData);
        /// Handle server disconnect

        void Update(float timeStep);
        void ProcessServerComms(float timeStep);

        /// Inserts an MD5 into a filename immediately preceding the extension
        String CreateMD5FileName(const String& fileName, const String& md5);
        /// Removes an MD5 from a filename if found
        void SplitMD5FileName(const String& md5FileName, String& fileName, String& md5);

        Network* network_;
        ConnectionStatus connectionStatus_;

        CommsStatus commsStatus_;

        Vector<String> pendingFileDownloads_;
        Vector<String> pendingFileUploads_;

        String currentFileProcessing_; // the name of either the file to be downloaded or the file being uploaded, depending on state

        String serverIP_;
        int serverPort_;

        SharedPtr<AssetCacheFileReceiver> fileReceiveManager_;
        SharedPtr<AssetCacheFileSender> fileSendManager_;

        float connectRetryDelay_;
        int retryFailureCounter_;

        const int maxConnectRetries_; // maximum number of times to try reconnecting to the server.

    };

}