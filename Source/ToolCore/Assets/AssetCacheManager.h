#pragma once

#include <Atomic/Core/Object.h>
#include <Atomic/IO/File.h>
#include <Atomic/Container/Vector.h>
#include <Atomic/Container/Pair.h>

using namespace Atomic;

namespace ToolCore
{

    class AssetCacheManager : public Object
    {
        ATOMIC_OBJECT(AssetCacheManager, Object);

    public:

        AssetCacheManager(Context* context);

        /// requests a cache file, fires an event when it is loaded to subscribers
        virtual void FetchCacheFile(const String& fileName, const String& md5) = 0;

        /// process the fact that a file has been added to the cache (externally, by an asset importer etc)
        virtual void OnFileAddedToCache(const String& fileName, const String& md5) = 0;

    protected:
        virtual void NotifyCacheFileFetchSuccess(const String& fileName, const String& md5);
        virtual void NotifyCacheFileFetchFail(const String& fileName, const String& md5);
    };

}