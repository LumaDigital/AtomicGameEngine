//
// Copyright (c) 2014-2016 THUNDERBEAST GAMES LLC
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <Poco/Process.h>
#include <Poco/File.h>

#include <Atomic/Core/StringUtils.h>
#include <Atomic/Core/CoreEvents.h>
#include <Atomic/IO/Log.h>
#include <Atomic/Input/Input.h>
#include <Atomic/IO/File.h>

#include "../ToolSystem.h"
#include "../ToolEnvironment.h"
#include "../Project/Project.h"
#include "../Build/BuildEvents.h"
#include "../Build/BuildSystem.h"


#include "CacheServerCmd.h"



namespace ToolCore
{

    CacheServerCmd::CacheServerCmd(Context* context) : Command(context)
    {
    }

    CacheServerCmd::~CacheServerCmd()
    {

    }

    bool CacheServerCmd::ParseInternal(const Vector<String>& arguments, unsigned startIndex, String& errorMsg)
    {
        String argument = arguments[startIndex].ToLower();
        cacheFolderPath_ = startIndex + 1 < arguments.Size() ? arguments[startIndex + 1] : String::EMPTY;
        String portString = startIndex + 2 < arguments.Size() ? arguments[startIndex + 2] : String::EMPTY;
        String diskSpaceLimitString = startIndex + 3 < arguments.Size() ? arguments[startIndex + 3] : String::EMPTY;

        if (argument != "cacheserver")
        {
            errorMsg = "Unable to parse cache server command";
            return false;
        }

        if (!cacheFolderPath_.Length())
        {
            errorMsg = "Unable to parse cache server folder path";
            return false;
        }

        if (!portString.Length())
        {
            port_ = 20;
        }
        else
        {
            port_ = ToInt(portString);
        }

        if (!diskSpaceLimitString.Length())
        {
            diskSpaceLimitMB_ = 1024;
        }
        else
        {
            diskSpaceLimitMB_ = ToInt(diskSpaceLimitString);
        }

        return true;
    }

    void CacheServerCmd::Run()
    {
        cacheServer_ = SharedPtr<AssetCacheServer>(new AssetCacheServer(context_, cacheFolderPath_, port_, diskSpaceLimitMB_));
        cacheServer_->Start();
    }

}
