//
// Copyright (c) 2014-2015, THUNDERBEAST GAMES LLC All rights reserved
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

#pragma once

#include "UIWidget.h"

namespace Atomic
{


class UISection : public UIWidget
{
    ATOMIC_OBJECT(UISection, UIWidget)

public:

    UISection(Context* context, bool createWidget = true);
    virtual ~UISection();

    void AddChild(UIWidget* child);
    void AddChildAfter(UIWidget* child, UIWidget* otherChild);
    void AddChildBefore(UIWidget* child, UIWidget* otherChild);
    void AddChildRelative(UIWidget* child, UI_WIDGET_Z_REL z, UIWidget* reference);

    void RemoveChild(UIWidget* child, bool cleanup = true);
    void DeleteAllChildren();

    UIWidget* GetFirstChild();
    UIWidget* GetWidget(const String& id);


protected:

    virtual bool OnEvent(const tb::TBWidgetEvent &ev);

private:

};

}
