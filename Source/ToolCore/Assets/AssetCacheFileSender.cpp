#include <Atomic/IO/FileSystem.h>
#include <Atomic/IO/Log.h>
#include <kNet/Network.h>

#include "AssetCacheFileSender.h"
#include "AssetCacheServer.h"
#include "AssetCacheServerEvents.h"

using namespace Atomic;
using namespace ToolCore;

const unsigned maxFragmentSize = 1024 * 1024;
const unsigned outboundMsgQueueSize = 1000;


AssetCacheFileSender::AssetCacheFileSender(Context* context, Connection* connection, String fileName, String filePath) :
    Object(context),
    connection_(connection),
    fileName_(fileName),
    filePath_(filePath),
    bytesSent_(0),
    nextFragmentID_(0),
    status_(STARTING),
    sendFile_(nullptr),
    fileSize_(0),
    nextSendDelay_(0),
    sendInterval_(0.10f)
{

}

void AssetCacheFileSender::Update(float timeStep)
{
    if (!connection_ || !connection_->IsConnected())
    {
        if (status_ == Status::STARTING || status_ == Status::SENDING)
        {
            ATOMIC_LOGINFOF("AssetCacheFileSender - connection dropped while sending file [%s]", fileName_.CString());

            status_ = Status::FAILED;
        }

        return;
    }
    
    if (status_ == Status::STARTING)
    {
        if (StartSend())
        {
            status_ = Status::SENDING;
        }
        else
        {
            status_ = Status::FAILED;
        }
    }
    else if (status_ == Status::SENDING)
    {
        nextSendDelay_ -= timeStep;

        if (nextSendDelay_ <= 0.0f)
        {
            if (SendFileFragments())
            {
                nextSendDelay_ = sendInterval_;
            }
            else
            {
                ATOMIC_LOGINFOF("AssetCacheFileSender - failed to send file fragments for file [%s]", fileName_.CString());

                status_ = Status::FAILED;
            }
        }
    }
}

bool AssetCacheFileSender::StartSend()
{
    FileSystem* fileSystem = GetSubsystem<FileSystem>();

    if (fileSystem->FileExists(filePath_))
    {
        sendFile_ = SharedPtr<File>(new File(context_, filePath_));
        fileSize_ = sendFile_->GetSize();

        if (fileSize_ == 0)
        {
            ATOMIC_LOGINFOF("AssetCacheFileSender - skipping 0b file [%s]", fileName_.CString());
            return false;
        }

        bytesSent_ = 0;
        nextFragmentID_ = 0;

        VariantMap map;
        using namespace AssetCacheFileTransferStart;

        fragmentSize_ = min(maxFragmentSize, fileSize_);
        const unsigned numFragments = (fileSize_ + fragmentSize_ - 1) / fragmentSize_;

        map[P_FILENAME] = fileName_;
        map[P_NUMFRAGMENTS] = numFragments;
        map[P_FILESIZE] = fileSize_;

        VectorBuffer sendBuff;
        sendBuff.WriteVariantMap(map);
        
        ATOMIC_LOGINFOF("AssetCacheFileSender - client [%s] - sending file [%s] - size[%d] - numFragments[%d]", connection_->GetAddress().CString(), fileName_.CString(), fileSize_, numFragments);

        timer_.Reset();

        connection_->SendMessage((int)AssetCacheServer::MessageID::FILE_TRANSFER_START, true, true, sendBuff.GetData(), sendBuff.GetSize());

        return true;
    }
    else
    {
        ATOMIC_LOGERRORF("AssetCacheFileSender - file not found! [%s]", fileName_.CString());
        return false;
    }
}

bool AssetCacheFileSender::SendFileFragments()
{
    // Add new data fragments into the queue.        
    PODVector<unsigned char> readBuffer(fragmentSize_);
    unsigned i = outboundMsgQueueSize - connection_->GetMessageConnection()->NumOutboundMessagesPending();

    while (i-- > 0 && bytesSent_ < fileSize_)
    {
        unsigned bytesInThisFragment = min((int)fragmentSize_, fileSize_ - bytesSent_);

        VariantMap map;

        using namespace AssetCacheFileFragment;

        unsigned readSize = sendFile_->Read(readBuffer.Buffer(), bytesInThisFragment);

        if (readSize != bytesInThisFragment)
        {
            return false;
        }

        VectorBuffer dataBuff(readBuffer.Buffer(), bytesInThisFragment);

        map[P_DATA] = dataBuff;
        map[P_FRAGMENTID] = nextFragmentID_;

        VectorBuffer sendBuff;
        sendBuff.WriteVariantMap(map);
        bytesSent_ += bytesInThisFragment;

        connection_->SendMessage((int)AssetCacheServer::MessageID::FILE_TRANSFER_FRAGMENT, true, true, sendBuff.GetData(), sendBuff.GetSize());

        nextFragmentID_++;
    }

    if (bytesSent_ == fileSize_)
    {
        status_ = Status::SENT;
        sendFile_->Close();

        unsigned timeSpan = timer_.GetMSec(false);

        float timeSpanSec = (float)timeSpan / 1000.0f;

        ATOMIC_LOGINFOF("AssetCacheFileSender sent file[%s] - bytes[%d]. Total time = %f, transfer rate = %f", fileName_.CString(), bytesSent_, timeSpanSec, ((float)bytesSent_ / timeSpanSec) / 1000.0f);

        sendFile_ = nullptr;
    }

    return true;
}