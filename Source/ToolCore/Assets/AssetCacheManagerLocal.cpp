#include <Atomic/IO/File.h>
#include <Atomic/IO/FileSystem.h>
#include "../ToolSystem.h"
#include "AssetDatabase.h"

#include "AssetCacheManagerLocal.h"


using namespace ToolCore;

void AssetCacheManagerLocal::FetchCacheFile(const String& fileName, const String& md5)
{
    FileSystem* fileSystem = GetSubsystem<FileSystem>();

    AssetDatabase* db = GetSubsystem<AssetDatabase>();
    String cachePath = db->GetCachePath();
    
    String fullFilePath = cachePath + fileName;

    if (fileSystem->FileExists(fullFilePath))
    {
        NotifyCacheFileFetchSuccess(fileName, md5);
    }
    else
    {
        NotifyCacheFileFetchFail(fileName, md5);
    }

}


