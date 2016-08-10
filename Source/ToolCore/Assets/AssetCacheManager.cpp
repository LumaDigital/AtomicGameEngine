#include "AssetCacheEvents.h"
#include "AssetCacheManager.h"

using namespace ToolCore;

AssetCacheManager::AssetCacheManager(Context* context) :
    Object(context)
{}

void AssetCacheManager::NotifyCacheFileFetchSuccess(const String& fileName, const String& md5)
{
    VariantMap& eventData = GetEventDataMap();
    eventData[AssetCacheFetchSuccess::P_CACHEFILENAME] = fileName;
    eventData[AssetCacheFetchSuccess::P_CACHEMD5] = md5;
    SendEvent(E_ASSETCACHE_FETCH_SUCCESS, eventData);
}

void AssetCacheManager::NotifyCacheFileFetchFail(const String& fileName, const String& md5)
{
    VariantMap& eventData = GetEventDataMap();
    eventData[AssetCacheFetchFail::P_CACHEFILENAME] = fileName;
    eventData[AssetCacheFetchFail::P_CACHEMD5] = md5;
    SendEvent(E_ASSETCACHE_FETCH_FAIL, eventData);
}