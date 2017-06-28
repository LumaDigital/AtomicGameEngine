#pragma once

#include <Atomic/Core/Object.h>

namespace ToolCore
{
    ATOMIC_EVENT(E_ASSETCACHESERVER_CONNECTED, AssetCacheServerConnected)
    {
    }

    ATOMIC_EVENT(E_ASSETCACHE_FILE_TRANSFER_START, AssetCacheFileTransferStart)
    {
        ATOMIC_PARAM(P_FILENAME, FileName);    // String
        ATOMIC_PARAM(P_FILESIZE, FileSize);    // unsigned int
        ATOMIC_PARAM(P_NUMFRAGMENTS, NumFragments); // unsigned int
    }

    ATOMIC_EVENT(E_ASSETCACHE_FILE_FRAGMENT, AssetCacheFileFragment)
    {
        ATOMIC_PARAM(P_DATA, Data);             // Vector Buffer
        ATOMIC_PARAM(P_FRAGMENTID, FragmentID); // unsigned int
    }
}
