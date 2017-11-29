#include <Atomic/IO/Log.h>
#include <Atomic/IO/File.h>
#include <Atomic/IO/FileSystem.h>
#include <Atomic/IO/MemoryBuffer.h>
#include <Atomic/Network/Network.h>
#include <Atomic/Network/NetworkEvents.h>
#include <Atomic/Core/CoreEvents.h>


#include "AssetCacheServerConnectedClient.h"
#include "AssetCacheServer.h"
#include "AssetCacheServerEvents.h"


using namespace Atomic;
using namespace ToolCore;


AssetCacheServerConnectedClient::AssetCacheServerConnectedClient(Context* context, Connection* client, String cacheFolderPath, unsigned diskSpaceLimitMB) :
    Object(context),
    client_(client),
    cacheFolderPath_(cacheFolderPath),
    fileReceiveManager_(nullptr),
    fileSendManager_(nullptr),
    diskSpaceLimitMB_(diskSpaceLimitMB)
{
    SubscribeToEvent(E_BEGINFRAME, ATOMIC_HANDLER(AssetCacheServerConnectedClient, HandleBeginFrame));
    SubscribeToEvent(E_NETWORKMESSAGE, ATOMIC_HANDLER(AssetCacheServerConnectedClient, HandleClientMessage));
}

String AssetCacheServerConnectedClient::GetCachePathForFile(String fileName)
{
    return cacheFolderPath_ + fileName;
}

void AssetCacheServerConnectedClient::CancelUpload()
{
    fileReceiveManager_ = nullptr;
}

void AssetCacheServerConnectedClient::HandleBeginFrame(StringHash eventType, VariantMap& eventData)
{
    using namespace BeginFrame;

    Update(eventData[P_TIMESTEP].GetFloat());
}

void AssetCacheServerConnectedClient::Update(float timeStep)
{
    if (fileReceiveManager_)
    {
        if (fileReceiveManager_->GetStatus() == AssetCacheFileReceiver::Status::COMPLETED)
        {
            client_->SendMessage((int)AssetCacheServer::MessageID::UPLOAD_COMPLETE, true, true, nullptr, 0);
            fileReceiveManager_ = nullptr;
        }
        else if (fileReceiveManager_->GetStatus() == AssetCacheFileReceiver::Status::FAILED)
        {   
            fileReceiveManager_ = nullptr;
        }
        else
        {
            fileReceiveManager_->Update(timeStep);
        }
    }

    if (fileSendManager_)
    {
        if (fileSendManager_->GetStatus() == AssetCacheFileSender::Status::SENT)
        {
            fileSendManager_ = nullptr;
        }
        else if (fileSendManager_->GetStatus() == AssetCacheFileSender::Status::FAILED)
        {   
            client_->SendMessage((int)AssetCacheServer::MessageID::DOWNLOAD_FAILED_ERROR, true, true, nullptr, 0);
            fileSendManager_ = nullptr;
        }
        else
        {
            fileSendManager_->Update(timeStep);
        }
    }
}

void AssetCacheServerConnectedClient::HandleClientMessage(StringHash eventType, VariantMap& eventData)
{
    using namespace NetworkMessage;

    Connection* client = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());    

    //ignore if it's not a message from "our" client
    if (client != client_)
        return;

    int msgId = eventData[P_MESSAGEID].GetInt();

    VectorBuffer buffer(eventData[P_DATA].GetBuffer());

    switch (msgId)
    {
        case (int)AssetCacheServer::MessageID::REQUEST_DOWNLOAD:
        {
            OnClientRequestDownload(buffer.ReadString());
        }
        break;
        case (int)AssetCacheServer::MessageID::REQUEST_UPLOAD:
        {
            OnClientRequestUpload(buffer.ReadString());
        }
        break;
        case (int)AssetCacheServer::MessageID::FILE_TRANSFER_START:
        {
            StartReceiveUpload(buffer.ReadVariantMap());
        }
        break;
        case (int)AssetCacheServer::MessageID::UPLOAD_FAILED_ERROR:
        {
            // handle client telling us the upload broke, clear our receive buffer.
            fileReceiveManager_ = nullptr;
        }
        break;
    }
}


void AssetCacheServerConnectedClient::OnClientRequestDownload(String fileName)
{
    //LOGINFOF("AssetCacheServer - fetching file [%s] for client", fileName.CString());

    FileSystem* fileSystem = GetSubsystem<FileSystem>();
    String fullFilePath = GetCachePathForFile(fileName);
    FileInfo fileInfo = fileSystem->GetFileInfo(fullFilePath);

    if (fileInfo.exists_)
    {
        if (fileInfo.size_ != 0)
        {
            // Touch the file for cache cleanup in reverse order of file request
            Time* time = GetSubsystem<Time>();
            fileSystem->SetLastModifiedTime(fullFilePath, time->GetTimeSinceEpoch());

            AssetCacheFileSender* newSendManager = new AssetCacheFileSender(
                context_,
                client_,
                fileName,
                cacheFolderPath_ + fileName);

            fileSendManager_ = SharedPtr<AssetCacheFileSender>(newSendManager);

            return;
        }
        else
        {
            // Delete empty files and treat as if not found
            fileSystem->Delete(fullFilePath);
        }
    }

    ATOMIC_LOGINFOF("AssetCacheServer - request for file [%s] rejected, doesn't exist in cache", fileName.CString());

    VectorBuffer memBuff(fileName.CString(), fileName.Length());

    client_->SendMessage((int)AssetCacheServer::MessageID::DOWNLOAD_REQUEST_REJECT_UNAVAILABLE, true, true, memBuff.GetData(), memBuff.GetSize());
}

void AssetCacheServerConnectedClient::OnClientRequestUpload(String fileName)
{
    FileSystem* fileSystem = GetSubsystem<FileSystem>();

    String fullFilePath = GetCachePathForFile(fileName);

    if (fileSystem->FileExists(fullFilePath))
    {
        ATOMIC_LOGINFOF("AssetCacheServer - upload request for file [%s] rejected, file already exists", fileName.CString());

        VectorBuffer memBuff(fileName.CString(), fileName.Length());
        client_->SendMessage((int)AssetCacheServer::MessageID::UPLOAD_REQUEST_REJECT_FILE_EXISTS, true, true, memBuff.GetData(), memBuff.GetSize());
    }
    else
    {
        VectorBuffer memBuff(fileName.CString(), fileName.Length());
        client_->SendMessage((int)AssetCacheServer::MessageID::UPLOAD_REQUEST_ACCEPT, true, true, memBuff.GetData(), memBuff.GetSize());
    }
}

void AssetCacheServerConnectedClient::StartReceiveUpload(VariantMap& fileTransferData)
{
    using namespace AssetCacheFileTransferStart;

    String fileName = fileTransferData[P_FILENAME].GetString();
    unsigned fileSize = fileTransferData[P_FILESIZE].GetUInt();
    unsigned numFragments = fileTransferData[P_NUMFRAGMENTS].GetUInt();

    assert(!fileName.Empty());
    assert(fileSize > 0);
    assert(numFragments > 0);

    if (!EnsureAvailableCacheSpace(fileSize))
    {
        ATOMIC_LOGINFOF("AssetCacheServer - upload request for file [%s] rejected, could not clear cache space", fileName.CString());
        client_->SendMessage((int)AssetCacheServer::MessageID::UPLOAD_FAILED_ERROR, true, true, nullptr, 0);
        return;
    }

    fileReceiveManager_ = new AssetCacheFileReceiver(
        context_,
        client_,
        fileName,
        cacheFolderPath_ + fileName,
        fileSize,
        numFragments);
}

bool AssetCacheServerConnectedClient::EnsureAvailableCacheSpace(unsigned fileSize)
{
    long diskSpaceLimitB = diskSpaceLimitMB_ * 1024 * 1024;
    long fileSizeLong = (long)fileSize;
    assert(diskSpaceLimitB > fileSizeLong);

    FileSystem* fileSystem = GetSubsystem<FileSystem>();

    Vector<String> fileNames;
    Vector<FileInfo> fileInfos;
    long cacheSize = 0;
    
    fileSystem->ScanDir(fileNames, cacheFolderPath_, String::EMPTY, SCAN_FILES, true);
    for (int i = fileNames.Size() - 1; i >= 0; --i)
    {
        FileInfo fileInfo = fileSystem->GetFileInfo(cacheFolderPath_ + fileNames[i]);
        cacheSize += fileInfo.size_;
        fileInfos.Push(fileInfo);
    }

    if (diskSpaceLimitB - cacheSize > fileSizeLong)
        return true;

    // Delete least recently updated/requested until enough space or no more files
    Sort(fileInfos.Begin(), fileInfos.End(), CompareFileInfoByLastModifiedAscending);
    while (fileInfos.Size() > 0 &&
        diskSpaceLimitB - cacheSize < fileSizeLong)
    {
        FileInfo& fileInfo = fileInfos[0];
        if (fileSystem->Delete(fileInfo.fullPath_))
            cacheSize -= fileInfo.size_;

        fileInfos.Erase(0);
    }

    return diskSpaceLimitB - cacheSize > fileSizeLong;
}