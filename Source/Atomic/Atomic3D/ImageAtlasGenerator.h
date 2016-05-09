//
// Copyright (c) 2008-2015 the Urho3D project.
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
#include "../Container/Ptr.h"
#include "../Container/Vector.h"
#include "../Math/Rect.h"
#include "../Resource/Image.h"

namespace Atomic
{
    class Scene;
    class StaticModel;;

class ATOMIC_API ImageAtlasGenerator : public Object
{
    OBJECT(ImageAtlasGenerator);
    BASEOBJECT(ImageAtlasGenerator);
public:
    ImageAtlasGenerator(Context* context);
    ~ImageAtlasGenerator();

    /// Builds an atlassed image from the input images. The final atlas is returned if successful and rects array is populated with image rects in the the atlas.
    SharedPtr<Image> GenerateAtlassedImage(const Vector<SharedPtr<Image>>& images, Vector<IntRect>& rects);

protected:
    struct AtlasElement
    {
        unsigned index_;
        Image* image_;
        bool placed_;

        AtlasElement() : placed_(false) {}
    };

    class Node : 
        public IntRect,
        public RefCounted
    {
        OBJECT(Node);
        BASEOBJECT(Node);
    public:
        Node() : occupied_(false) {}
        Node(const IntRect& rect) : occupied_(false), IntRect(rect) {}
        Node(int left, int top, int right, int bottom) : occupied_(false), IntRect(left, top, right, bottom) {}

        ImageAtlasGenerator::Node* Insert(AtlasElement& element);
    private:
        SharedPtr<Node> child1_;
        SharedPtr<Node> child2_;
        bool occupied_;
    };
};
}

