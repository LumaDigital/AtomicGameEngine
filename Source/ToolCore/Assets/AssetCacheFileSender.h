#pragma once

#include <Atomic/Network/Connection.h>
#include <Atomic/IO/File.h>

using namespace Atomic;

namespace ToolCore
{

    class AssetCacheFileSender : public Object
    {
        ATOMIC_OBJECT(AssetCacheFileSender, Object);

    public:

        enum Status
        {
            STARTING,
            SENDING,
            SENT,
            FAILED
        };

        AssetCacheFileSender(Context* context, Connection* connection, String fileName, String filePath);

        void Update(float timeStep);
        bool StartSend();
        bool SendFileFragments();

        Status GetStatus() { return status_; }
        String GetFileName() { return fileName_; }
        String GetFilePath() { return filePath_; }
        Connection* GetConnection() { return connection_; }

    private:

        Connection* connection_;
        String fileName_;
        String filePath_;
        Status status_;

        unsigned fileSize_;
        unsigned bytesSent_;
        unsigned nextFragmentID_;

        SharedPtr<File> sendFile_;

        float nextSendDelay_;

        Timer timer_;

        const float sendInterval_; // the delay between sending each batch of file fragments.
    };

}