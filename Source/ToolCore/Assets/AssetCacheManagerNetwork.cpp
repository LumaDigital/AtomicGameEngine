#include <Atomic/IO/File.h>
#include <Atomic/IO/FileSystem.h>
#include <Atomic/Core/CoreEvents.h>
#include <Atomic/IO/MemoryBuffer.h>
#include <Atomic/Network/NetworkEvents.h>
#include <Atomic/IO/Log.h>

#include "../Project/Project.h"

#include "../ToolSystem.h"
#include "AssetDatabase.h"
#include "AssetCacheManagerNetwork.h"
#include "AssetCacheServer.h"
#include "AssetCacheServerEvents.h"
#include "AssetCacheConfig.h"

using namespace ToolCore;

AssetCacheManagerNetwork::AssetCacheManagerNetwork(Context* context) :
    AssetCacheManager(context),
    connectionStatus_(ConnectionStatus::NOT_CONNECTED),
    commsStatus_(CommsStatus::IDLE),
    connectRetryDelay_(0.0),
    fileReceiveManager_(nullptr),
    fileSendManager_(nullptr),
    retryFailureCounter_(0),
    serverIP_(""),
    serverPort_(0),
    maxConnectRetries_(5)
{
    SubscribeToEvent(E_BEGINFRAME, ATOMIC_HANDLER(AssetCacheManagerNetwork, HandleBeginFrame));
    SubscribeToEvent(E_NETWORKMESSAGE, ATOMIC_HANDLER(AssetCacheManagerNetwork, HandleServerMessage));
    SubscribeToEvent(E_SERVERDISCONNECTED, ATOMIC_HANDLER(AssetCacheManagerNetwork, OnDisconnected));
    SubscribeToEvent(E_SERVERCONNECTED, ATOMIC_HANDLER(AssetCacheManagerNetwork, OnConnected));
    SubscribeToEvent(E_CONNECTFAILED, ATOMIC_HANDLER(AssetCacheManagerNetwork, OnConnectionFailed));
    
    VariantMap assetCacheMap;
    AssetCacheConfig::ApplyConfig(assetCacheMap);

    serverIP_ = assetCacheMap["ServerIP"].GetString();
    serverPort_ = assetCacheMap["ServerPort"].GetInt();

    network_ = GetSubsystem<Network>();
}

void AssetCacheManagerNetwork::FetchCacheFile(const String& fileName, const String& md5)
{
    if (!FetchCacheFileLocal(fileName, md5))
    {
        FetchCacheFileRemote(fileName, md5);
    }
}


void AssetCacheManagerNetwork::OnFileAddedToCache(const String& fileName, const String& md5)
{
    String md5FileName = CreateMD5FileName(fileName, md5);
    if (!pendingFileUploads_.Contains(md5FileName))
    {
        //add the fetch request to the queue to be processed
        pendingFileUploads_.Push(md5FileName);
    }
}

bool AssetCacheManagerNetwork::FetchCacheFileLocal(const String& fileName, const String& md5)
{
    FileSystem* fileSystem = GetSubsystem<FileSystem>();

    AssetDatabase* db = GetSubsystem<AssetDatabase>();
    String cachePath = db->GetCachePath();

    String fullFilePath = cachePath + fileName;

    if (fileSystem->FileExists(fullFilePath))
    {
        NotifyCacheFileFetchSuccess(fileName, md5);

        return true;
    }
    else
    {
        return false;
    }

}

void AssetCacheManagerNetwork::FetchCacheFileRemote(const String& fileName, const String& md5)
{
    String md5FileName = CreateMD5FileName(fileName, md5);
    if (!pendingFileDownloads_.Contains(md5FileName))
    {
        //add the fetch request to the queue to be processed
        pendingFileDownloads_.Push(md5FileName);
    }
}

void AssetCacheManagerNetwork::HandleBeginFrame(StringHash eventType, VariantMap& eventData)
{
    using namespace BeginFrame;

    Update(eventData[P_TIMESTEP].GetFloat());
}

void AssetCacheManagerNetwork::Update(float timeStep)
{
    if (connectRetryDelay_ > 0.0f)
    {
        connectRetryDelay_ -= timeStep;
    }
        
    // Try to connect if not connected
    if (connectionStatus_ == ConnectionStatus::NOT_CONNECTED)
    {   
        if (HasPendingServerRequests() && connectRetryDelay_ <= 0.0f)
        {
            if (ConnectToCacheServer())
            {
                retryFailureCounter_ = 0;
            }
            else if (++retryFailureCounter_ >= maxConnectRetries_)
            {
                // TODO - delay and retry a few times before failing the requests.
                ATOMIC_LOGINFOF("Remote Cache Server unreachable.");
                SetOutstandingServerRequestsToFailed();
            }
        }
    }    
    else if (connectionStatus_ == ConnectionStatus::CONNECTED)
    {
        ProcessServerComms(timeStep);
    }
}

void AssetCacheManagerNetwork::ProcessServerComms(float timeStep)
{
    Connection* serverConnection = network_->GetServerConnection();

    if (!serverConnection)
        return;

    switch (commsStatus_)
    {
        case (CommsStatus::IDLE):
        {
            //first try to process the next download request. If there aren't any, then try upload.
            if (!RequestNextFileDownload())
            {
                RequestNextFileUpload();
            }
        }
        break;
        case (CommsStatus::FILE_DOWNLOADING):
        {
            if (fileReceiveManager_->GetStatus() == AssetCacheFileReceiver::Status::COMPLETED)
            {
                String fileName;
                String md5;
                SplitMD5FileName(fileReceiveManager_->GetFileName(), fileName, md5);
                NotifyCacheFileFetchSuccess(fileName, md5);

                fileReceiveManager_ = nullptr;
                commsStatus_ = CommsStatus::IDLE;
            }
            else if (fileReceiveManager_->GetStatus() == AssetCacheFileReceiver::Status::FAILED)
            {
                String fileName;
                String md5;
                SplitMD5FileName(fileReceiveManager_->GetFileName(), fileName, md5);
                NotifyCacheFileFetchFail(fileName, md5);

                fileReceiveManager_ = nullptr;
                commsStatus_ = CommsStatus::IDLE;
            }
            else
            {
                fileReceiveManager_->Update(timeStep);
            }
        }
        break;
        case (CommsStatus::FILE_UPLOADING):
        {
            fileSendManager_->Update(timeStep);

            if (fileSendManager_->GetStatus() == AssetCacheFileSender::Status::SENT)
            {
                fileSendManager_ = nullptr;
                commsStatus_ = CommsStatus::FILE_UPLOAD_COMPLETED;
            }
            else if (fileSendManager_->GetStatus() == AssetCacheFileSender::Status::FAILED)
            {
                String fileName;
                String md5;
                SplitMD5FileName(fileSendManager_->GetFileName(), fileName, md5);
                ATOMIC_LOGERRORF("Failed to upload asset file to server [%s], something went wrong.", fileName.CString());
                commsStatus_ = CommsStatus::IDLE;
                fileSendManager_ = nullptr;
                serverConnection->SendMessage((int)AssetCacheServer::MessageID::UPLOAD_FAILED_ERROR, true, true, nullptr, 0);

                // TODO - retry maybe?
            }

        }
        break;
    }
}

bool AssetCacheManagerNetwork::HasPendingServerRequests()
{
    return (pendingFileDownloads_.Size() > 0) || (pendingFileUploads_.Size() > 0);
}

int AssetCacheManagerNetwork::NumPendingServerRequests()
{
    return pendingFileDownloads_.Size() + pendingFileUploads_.Size();
}

void AssetCacheManagerNetwork::OnConnectionFailed(StringHash eventType, VariantMap& eventData)
{
    ATOMIC_LOGERRORF("AssetCacheManagerNetwork - Connection Failed, retrying after delay");
    connectRetryDelay_ = 1.0f;

    connectionStatus_ = ConnectionStatus::NOT_CONNECTED;
}

void AssetCacheManagerNetwork::OnConnected(StringHash eventType, VariantMap& eventData)
{
    ATOMIC_LOGINFOF("AssetCacheManagerNetwork - On Connect.");
    connectionStatus_ = ConnectionStatus::CONNECTED;
}

void AssetCacheManagerNetwork::OnDisconnected(StringHash eventType, VariantMap& eventData)
{
    ATOMIC_LOGINFOF("AssetCacheManagerNetwork - On Disconnect.");

    ReQueueOutstandingRequests();

    connectionStatus_ = ConnectionStatus::NOT_CONNECTED;
    commsStatus_ = CommsStatus::IDLE;    
}

void AssetCacheManagerNetwork::ReQueueOutstandingRequests()
{    
    // How we'd get into this function if this is empty, I don't know, but let's guard against it.
    if (currentFileProcessing_.Empty())
        return;

    if ( (commsStatus_ == CommsStatus::FILE_DOWNLOADING || 
         commsStatus_ == CommsStatus::FILE_DOWNLOAD_REQUESTED))
    {
        pendingFileDownloads_.Push(currentFileProcessing_);
        currentFileProcessing_ = "";
    }

    if ((commsStatus_ == CommsStatus::FILE_UPLOADING ||
        commsStatus_ == CommsStatus::FILE_UPLOAD_REQUESTED))
    {
        pendingFileUploads_.Push(currentFileProcessing_);
        currentFileProcessing_ = "";
    }

    //TODO - drop any existing File Transfer Managers (receive and send).
}

bool AssetCacheManagerNetwork::ConnectToCacheServer()
{
    VariantMap identityVMap;

    identityVMap["id"] = assetCacheClientIdentifier;

    if (network_->Connect(serverIP_, serverPort_, kNet::SocketOverTCP, nullptr, identityVMap))
    {
        ATOMIC_LOGINFOF("AssetCacheManagerNetwork - connecting to address[%s] port [%d] as [%s] ", serverIP_.CString(), serverPort_, identityVMap["id"].GetString().CString());

        connectionStatus_ = ConnectionStatus::CONNECTING;
        return true;
    }

    ATOMIC_LOGINFOF("AssetCacheManagerNetwork - failed to connect to address[%s] port [%d] as [%s] ", serverIP_.CString(), serverPort_, identityVMap["id"].GetString().CString());

    return false;
}




bool AssetCacheManagerNetwork::RequestNextFileDownload()
{
    if (pendingFileDownloads_.Size() == 0)
        return false;

    currentFileProcessing_ = pendingFileDownloads_.Back();
    pendingFileDownloads_.Pop();

    Connection* serverConnection = network_->GetServerConnection();
    //Pack the fileName into a mem buffer for passing across the network below.
    VectorBuffer memBuff(currentFileProcessing_.CString(), currentFileProcessing_.Length());

    ATOMIC_LOGINFOF("AssetCacheManagerNetwork - requesting file download - [%s].", currentFileProcessing_.CString());

    serverConnection->SendMessage((int)AssetCacheServer::MessageID::REQUEST_DOWNLOAD, true, true, memBuff.GetData(), memBuff.GetSize());

    commsStatus_ = CommsStatus::FILE_DOWNLOAD_REQUESTED;

    return true;
}

bool AssetCacheManagerNetwork::RequestNextFileUpload()
{
    if (pendingFileUploads_.Size() == 0)
        return false;

    currentFileProcessing_ = pendingFileUploads_.Back();
    pendingFileUploads_.Pop();

    Connection* serverConnection = network_->GetServerConnection();
    //Pack the fileName into a mem buffer for passing across the network below. TODO - Do I need to do this? Maybe not. Maybe just read string?
    VectorBuffer memBuff(currentFileProcessing_.CString(), currentFileProcessing_.Length());

    ATOMIC_LOGINFOF("AssetCacheManagerNetwork - requesting file upload - [%s].", currentFileProcessing_.CString());

    serverConnection->SendMessage((int)AssetCacheServer::MessageID::REQUEST_UPLOAD, true, true, memBuff.GetData(), memBuff.GetSize());

    commsStatus_ = CommsStatus::FILE_UPLOAD_REQUESTED;

    return true;
}

void AssetCacheManagerNetwork::HandleServerMessage(StringHash eventType, VariantMap& eventData)
{
    if (!network_->GetServerConnection())
        return;

    using namespace NetworkMessage;

    int msgId = eventData[P_MESSAGEID].GetInt();
    VectorBuffer buffer(eventData[P_DATA].GetBuffer());
        
    switch (msgId)
    {
        case (int)AssetCacheServer::MessageID::CONNECTION_ACCEPTED:
        {
            // This is here because sometimes the server can disconnect then reconnect to a client without the client realizing it was disconnected. :/
            // Because of that, we make the server send a connection accepted message so that if the client needs to flush and resend the messages, it can.
            // Otherwise both sides just sit there waiting for the other.

            if (commsStatus_ != CommsStatus::IDLE)
            {  
                ATOMIC_LOGINFOF("HandleServerMessage - connection received but state isn't IDLE, requeing requests");

                ReQueueOutstandingRequests();
            }
        }
        break;
        case (int)AssetCacheServer::MessageID::FILE_TRANSFER_START:
        {
            assert(commsStatus_ == CommsStatus::FILE_DOWNLOAD_REQUESTED);

            VariantMap map = buffer.ReadVariantMap();

            StartDownload(map);
        }            
        break;
        case (int)AssetCacheServer::MessageID::DOWNLOAD_REQUEST_REJECT_UNAVAILABLE:
        {
            String md5FileName = buffer.ReadString();

            if (md5FileName == currentFileProcessing_)
            {

                ATOMIC_LOGINFOF("AssetCacheManagerNetwork - download [%s] request rejected, file doesn't exist on server.", md5FileName.CString());
                
                currentFileProcessing_ = String::EMPTY;
                commsStatus_ = CommsStatus::IDLE;

                String fileName;
                String md5;
                SplitMD5FileName(md5FileName, fileName, md5);
                NotifyCacheFileFetchFail(fileName, md5);
            }
            else
            {
                ATOMIC_LOGERRORF("AssetCacheManagerNetwork - received download [%s] request rejected message, but didn't request it, something has gone wrong!", md5FileName.CString());
            }
        }
        break;
        case (int)AssetCacheServer::MessageID::UPLOAD_REQUEST_ACCEPT:
        {
            assert(commsStatus_ == CommsStatus::FILE_UPLOAD_REQUESTED);

            StartUpload();
        }
        break;
        case (int)AssetCacheServer::MessageID::UPLOAD_REQUEST_REJECT_FILE_EXISTS:
        {
            assert(commsStatus_ == CommsStatus::FILE_UPLOAD_REQUESTED);

            commsStatus_ = CommsStatus::IDLE;

        }
        break;
        case (int)AssetCacheServer::MessageID::UPLOAD_FAILED_ERROR:
        {
            // Either uploading or completed is valid. For small enough files,
            // the first packet will contain the entire file
            assert(commsStatus_ == CommsStatus::FILE_UPLOADING ||
                commsStatus_ == CommsStatus::FILE_UPLOAD_COMPLETED);

            commsStatus_ = CommsStatus::IDLE;

            fileSendManager_ = nullptr;

        }
        break;
        case (int)AssetCacheServer::MessageID::UPLOAD_COMPLETE:
        {
            if (commsStatus_ != CommsStatus::FILE_UPLOAD_COMPLETED)
            {
                //TODO - something more intelligent here.
                assert(false);
            }

            commsStatus_ = CommsStatus::IDLE;
        }
        break;        
    }
}

void AssetCacheManagerNetwork::StartDownload(VariantMap& fileTransferData)
{
    using namespace AssetCacheFileTransferStart;

    String md5FileName = fileTransferData[P_FILENAME].GetString();
    unsigned fileSize = fileTransferData[P_FILESIZE].GetUInt();
    unsigned numFragments = fileTransferData[P_NUMFRAGMENTS].GetUInt();

    assert(!md5FileName.Empty());
    assert(fileSize > 0);
    assert(numFragments > 0);

    AssetDatabase* db = GetSubsystem<AssetDatabase>();
    String fileName;
    String md5;
    SplitMD5FileName(md5FileName, fileName, md5);
    String filePath = db->GetCachePath() + fileName;

    AssetCacheFileReceiver* newReceiveManager = new AssetCacheFileReceiver(
        context_,
        network_->GetServerConnection(),
        md5FileName,
        filePath,
        fileSize,
        numFragments);

    fileReceiveManager_ = SharedPtr<AssetCacheFileReceiver>(newReceiveManager);

    commsStatus_ = CommsStatus::FILE_DOWNLOADING;

}

void AssetCacheManagerNetwork::StartUpload()
{
    commsStatus_ = CommsStatus::FILE_UPLOADING;

    AssetDatabase* db = GetSubsystem<AssetDatabase>();
    String fileName;
    String md5;
    SplitMD5FileName(currentFileProcessing_, fileName, md5);
    String filePath = db->GetCachePath() + fileName;

    AssetCacheFileSender* newSendManager = new AssetCacheFileSender(
        context_,
        network_->GetServerConnection(),
        currentFileProcessing_,
        filePath);
    fileSendManager_ = SharedPtr<AssetCacheFileSender>(newSendManager);
}

void AssetCacheManagerNetwork::SetOutstandingServerRequestsToFailed()
{
    Vector<String>::Iterator itr = pendingFileDownloads_.Begin();

    while (itr != pendingFileDownloads_.End())
    {
        String fileName;
        String md5;
        SplitMD5FileName(*itr, fileName, md5);

        NotifyCacheFileFetchFail(fileName, md5);
        itr++;
    }

    pendingFileDownloads_.Clear();

    // just clear these, I suppose. No mechanism to remember them when next we run the app and connect to the server.
    pendingFileUploads_.Clear();

}

String AssetCacheManagerNetwork::CreateMD5FileName(const String& fileName, const String& md5)
{
    unsigned pos = fileName.FindLast('.');
    if (pos == String::NPOS)
        return fileName + ".[" + md5 + "]";

    String result = fileName;
    result.Insert(pos, ".[" + md5 + "]");

    return result;
}

void AssetCacheManagerNetwork::SplitMD5FileName(const String& md5FileName, String& fileName, String& md5)
{
    fileName = md5FileName;

    unsigned startPos = fileName.FindLast(".[");
    unsigned endPos = fileName.Find(']', startPos);
    if (startPos == String::NPOS || endPos == String::NPOS)
    {
        md5 = String::EMPTY;
        return;
    }

    fileName.Erase(startPos, endPos - startPos + 1);
    md5 = md5FileName.Substring(startPos + 2, endPos - startPos - 2);
}