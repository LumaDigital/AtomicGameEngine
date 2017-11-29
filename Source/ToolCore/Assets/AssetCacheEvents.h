#pragma once

#include <Atomic/Core/Object.h>
#include <Atomic/IO/File.h>

namespace ToolCore
{

ATOMIC_EVENT(E_ASSETCACHE_FETCH_SUCCESS, AssetCacheFetchSuccess)
{
    ATOMIC_PARAM(P_CACHEFILENAME, FileName);            // string
    ATOMIC_PARAM(P_CACHEMD5, Md5);                 // string
}

ATOMIC_EVENT(E_ASSETCACHE_FETCH_FAIL, AssetCacheFetchFail)
{
    ATOMIC_PARAM(P_CACHEFILENAME, FileName);            // string
    ATOMIC_PARAM(P_CACHEMD5, Md5);                 // string
}

}