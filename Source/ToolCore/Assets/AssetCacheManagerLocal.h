#pragma once

#include <Atomic/Core/Object.h>
#include <Atomic/IO/File.h>
#include <Atomic/Container/Vector.h>
#include <Atomic/Container/Pair.h>

#include "AssetCacheManager.h"

using namespace Atomic;

namespace ToolCore
{

class AssetCacheManagerLocal : public AssetCacheManager
{
    ATOMIC_OBJECT(AssetCacheManagerLocal, AssetCacheManager);

public:

    AssetCacheManagerLocal(Context* context) : AssetCacheManager(context) {}

    /// requests a cache file, fires an event when it is loaded to subscribers
    virtual void FetchCacheFile(const String& fileName, const String& md5);

    /// process the fact that a file has been added to the cache (externally, by an asset importer etc)
    virtual void OnFileAddedToCache(const String& fileName, const String& md5) {}

};

}