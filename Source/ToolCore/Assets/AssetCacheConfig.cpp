
#include "../../Atomic/Core/Context.h"
#include "../../Atomic/IO/Log.h"
#include "../../Atomic/IO/File.h"
#include "../../Atomic/IO/FileSystem.h"
#include "../Atomic/Resource/JSONFile.h"
#include "../../Atomic/Graphics/GraphicsDefs.h"
#include "AssetCacheConfig.h"

namespace Atomic
{

    AssetCacheConfig AssetCacheConfig::assetCacheConfig_;

    bool AssetCacheConfig::LoadAssetCacheConfig(const JSONValue& jAssetCacheConfig)
    {
        for (JSONObject::ConstIterator i = jAssetCacheConfig.Begin(); i != jAssetCacheConfig.End(); ++i)
        {
            String key = i->first_;
            const JSONValue& jvalue = i->second_;

            if (key == "useServer")
                valueMap_["UseServer"] = GetBoolValue(jvalue, false);
            else if (key == "serverIP")
                valueMap_["ServerIP"] = GetStringValue(jvalue, "127.0.0.1");
            else if (key == "serverPort")
                valueMap_["ServerPort"] = GetIntValue(jvalue, 20);
        }

        return true;
    }

    bool AssetCacheConfig::LoadDesktopConfig(JSONValue root)
    {
        const JSONValue& jdesktop = root["desktop"];

        if (!jdesktop.IsObject())
            return false;

        const JSONValue& jAssetCacheConfig = jdesktop["AssetCache"];
        if (jAssetCacheConfig.IsObject())
            LoadAssetCacheConfig(jAssetCacheConfig);
        return true;
    }

}