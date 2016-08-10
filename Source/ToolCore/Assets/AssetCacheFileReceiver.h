#pragma once

#include <Atomic/Network/Connection.h>
#include <Atomic/Network/Network.h>
#include <Atomic/Network/NetworkEvents.h>

#include <Atomic/IO/File.h>

using namespace Atomic;

namespace ToolCore
{

    class AssetCacheFileReceiver : public Object
    {
        struct FileFragment
        {
            VectorBuffer data;
            unsigned long fragmentIndex;
        };
        
        ATOMIC_OBJECT(AssetCacheFileReceiver, Object);

    public:

        enum Status
        {
            RECEIVING,
            COMPLETED,
            FAILED
        };

        AssetCacheFileReceiver(Context* context, Connection* connection, String fileName, String filePath, unsigned fileSize, unsigned numFragments);
        ~AssetCacheFileReceiver();

        void HandleClientMessage(StringHash eventType, VariantMap& eventData);

        Status GetStatus() { return status_; }
        String GetFileName() { return fileName_; }
        String GetFilePath() { return filePath_; }
        Connection* GetConnection() { return connection_; }

        void Update(float timeStep);

    private:

        void WriteFinishedFragments();
        void CheckComplete();

        Connection* connection_;
        String fileName_;
        String filePath_;

        SharedPtr<File> receivedFile_;        

        unsigned fileSize_;
        unsigned nextFragmentID_;
        unsigned totalFragments_;
        unsigned bytesReceived_;

        Status status_;

        Timer timer_;

        // Used by the receiver to store partial received data.
        HashMap<unsigned, FileFragment> fragments;
    };

}