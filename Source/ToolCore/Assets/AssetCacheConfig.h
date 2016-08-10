#pragma once

#include "../../Atomic/Core/Variant.h"
#include "../../Atomic/Resource/JSONValue.h"
#include "../../Atomic/Resource/Configuration.h"

namespace Atomic
{
    class Context;

    class AssetCacheConfig :
        Configuration
    {

    public:

        static bool LoadFromFile(Context* context, const String& filename) { return assetCacheConfig_.Configuration::LoadFromFile(context, filename); }
        static bool LoadFromJSON(const String& json) { return assetCacheConfig_.Configuration::LoadFromJSON(json); }
        static void Clear() { assetCacheConfig_.Configuration::Clear(); }

        /// Apply the configuration to a setting variant map, values that exist will not be overriden
        static void ApplyConfig(VariantMap& settings, bool overwrite = false) { return assetCacheConfig_.Configuration::ApplyConfig(settings, overwrite); }

        static bool IsLoaded() { return assetCacheConfig_.Configuration::IsLoaded(); };

    private:

        virtual bool LoadDesktopConfig(JSONValue root);
        bool LoadAssetCacheConfig(const JSONValue& jAssetCacheConfig);

        static AssetCacheConfig assetCacheConfig_;
    };

}
