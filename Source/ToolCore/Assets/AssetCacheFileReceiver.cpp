#include <Atomic/IO/FileSystem.h>
#include <Atomic/IO/Log.h>
#include <kNet/Network.h>

#include "AssetCacheFileReceiver.h"
#include "AssetCacheServer.h"
#include "AssetCacheServerEvents.h"

using namespace Atomic;
using namespace ToolCore;

AssetCacheFileReceiver::AssetCacheFileReceiver(Context* context, Connection* connection, String fileName, String filePath, unsigned fileSize, unsigned numFragments) :
    Object(context),
    connection_(connection),
    fileName_(fileName),
    filePath_(filePath),
    fileSize_(fileSize),
    totalFragments_(numFragments),
    receivedFile_(nullptr),
    nextFragmentID_(0),
    bytesReceived_(0),
    status_(Status::RECEIVING)
{
    SubscribeToEvent(E_NETWORKMESSAGE, ATOMIC_HANDLER(AssetCacheFileReceiver, HandleClientMessage));

    ATOMIC_LOGINFOF("AssetCacheFileReceiver receiving file[%s] - size [%d] numfragments [%d]", fileName_.CString(), fileSize_, totalFragments_);

    receivedFile_ = SharedPtr<File>(new Atomic::File(context_, filePath_, Atomic::FileMode::FILE_WRITE));
    timer_.Reset();
}

AssetCacheFileReceiver::~AssetCacheFileReceiver()
{
    // handle deletion while file is still being received.
    if (status_ == Status::RECEIVING)
    {
        // do the check, maybe the file is finished
        CheckComplete();

        // if we're not complete after the check, we're shutting down mid-transfer, let's cleanup
        if (status_ != Status::COMPLETED)
        {
            ATOMIC_LOGERRORF("AssetCacheFileReceiver - transfer stopped part way, deleting file fragment [%s]", fileName_.CString());

            receivedFile_->Close();
            receivedFile_ = nullptr;

            // delete the fragment, since transfer never completed
            FileSystem* fileSystem = GetSubsystem<FileSystem>();
            fileSystem->Delete(fileName_);
        }
    }

}

void AssetCacheFileReceiver::Update(float timeStep)
{
    if (status_ == Status::RECEIVING)
    {
        CheckComplete();
    }
}

void AssetCacheFileReceiver::CheckComplete()
{
    if (nextFragmentID_ == totalFragments_)
    {
        ATOMIC_LOGINFOF("AssetCacheFileReceiver finished receiving file[%s] - bytes received [%d] numfragments [%d]", fileName_.CString(), bytesReceived_, totalFragments_);

        receivedFile_->Close();
        receivedFile_ = nullptr;

        status_ = Status::COMPLETED;
    }
}

void AssetCacheFileReceiver::HandleClientMessage(StringHash eventType, VariantMap& eventData)
{
    if (status_ != Status::RECEIVING)
    {
        return;
    }

    using namespace NetworkMessage;

    Connection* clientConnection = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());

    if (clientConnection != connection_)
    {
        return;
    }

    int msgId = eventData[P_MESSAGEID].GetInt();
    VectorBuffer buffer(eventData[P_DATA].GetBuffer());

    if (msgId != (int)AssetCacheServer::MessageID::FILE_TRANSFER_FRAGMENT)
    {
        // Will get registered for network messages while the FILE_TRANSFER_START is still being processed
        if (msgId != (int)AssetCacheServer::MessageID::FILE_TRANSFER_START)
        {
            ATOMIC_LOGERRORF("AssetCacheFileReceiver received an invalid message id %d", msgId);
        }
        return;
    }

    VariantMap map = buffer.ReadVariantMap();
    using namespace AssetCacheFileFragment;
    int fragID = map[P_FRAGMENTID].GetUInt();
    VectorBuffer dataBuffer(map[P_DATA].GetBuffer());

    if (fragID == nextFragmentID_)
    {
        nextFragmentID_++;

        receivedFile_->Write(dataBuffer.GetData(), dataBuffer.GetSize());

        bytesReceived_ += dataBuffer.GetSize();
        WriteFinishedFragments();
    }
    else
    {
        if (!fragments.Contains(fragID))
        {
            FileFragment f;

            f.data = dataBuffer;
            f.fragmentIndex = fragID;
            fragments[fragID] = f;
            bytesReceived_ += dataBuffer.GetSize();
        }
    }
}

void AssetCacheFileReceiver::WriteFinishedFragments()
{
    HashMap<unsigned, FileFragment>::Iterator iter = fragments.Find(nextFragmentID_);
    while (iter != fragments.End())
    {
        VectorBuffer* fragBuff = &(iter->second_.data);
        receivedFile_->Write(fragBuff->GetData(), fragBuff->GetSize());

        fragments.Erase(nextFragmentID_);
        nextFragmentID_++;
        iter = fragments.Find(nextFragmentID_);
    }
}