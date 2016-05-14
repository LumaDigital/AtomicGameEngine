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

#include "ImageAtlasGenerator.h"

using namespace Atomic;
using namespace AtomicEditor;

ImageAtlasGenerator::ImageAtlasGenerator(Context* context) :
    Object(context)
{
}

ImageAtlasGenerator::~ImageAtlasGenerator()
{
}

SharedPtr<Image> ImageAtlasGenerator::GenerateAtlassedImage(const Vector<SharedPtr<Image>>& images, Vector<IntRect>& rects)
{
    if (0 == images.Size())
        return SharedPtr<Image>();

    rects.Resize(images.Size());
    Vector<AtlasElement> elements(images.Size());
    unsigned components = 0;
    int maxWidth = 0;
    int maxHeight = 0;
    for (unsigned i = 0; i < images.Size(); i++)
    {
        AtlasElement& element = elements[i];
        element.index_ = i;
        element.image_ = images[i];

        if (components < element.image_->GetComponents())
            components = element.image_->GetComponents();
        if (maxWidth < element.image_->GetWidth())
            maxWidth = element.image_->GetWidth();
        if (maxHeight < element.image_->GetHeight())
            maxHeight = element.image_->GetHeight();
    }

    // Get a POT starting rect big enough for the largest image
    int width = 2;
    int height = 2;
    while (width < maxWidth)
        width *= 2;
    while (height < maxHeight)
        height *= 2;
    SharedPtr<Node> bucketNode(new Node(0, 0, width, height));

    // Keep subdividing the rect to place individual images
    // Based on http://www.blackpawn.com/texts/lightmaps/default.html
    bool allPlaced;
    int maxRight = 0;
    int maxBottom = 0;
    do
    {
        allPlaced = true;
        Vector<AtlasElement>::Iterator elementIter;
        for (elementIter = elements.Begin(); elementIter != elements.End(); elementIter++)
        {
            if (!elementIter->placed_)
            {
                Node* node = bucketNode->Insert(*elementIter);
                if (NULL != node)
                {
                    elementIter->placed_ = true;
                    rects[elementIter->index_] = *node;

                    if (node->right_ > maxRight)
                        maxRight = node->right_;
                    if (node->bottom_ > maxBottom)
                        maxBottom = node->bottom_;
                }
                else
                    allPlaced = false;
            }
        }

        if (!allPlaced)
        {
            // If there are still some images to be placed, double the top level rect along its shortest edge and try again
            // Try to use any easily recoverable space on the edges of the top level rect
            IntRect rect;
            if (width > height)
            {
                height *= 2;
                rect.top_ = maxBottom;
                rect.bottom_ = height;
                rect.right_ = width;
            }
            else
            {
                width *= 2;
                rect.left_ = maxRight;
                rect.right_ = width;
                rect.bottom_ = height;
            }

            bucketNode = new Node(rect);
        }
    } while (!allPlaced);

    // Create the atlas and copy in each of the images    
    SharedPtr<Image> atlas(new Image(context_));
    atlas->SetSize(width, height, components);
    Vector<AtlasElement>::Iterator elementIter;
    for (elementIter = elements.Begin(); elementIter != elements.End(); elementIter++)
    {
        atlas->SetSubimage(elementIter->image_, rects[elementIter->index_]);
    }

    return atlas;
}

ImageAtlasGenerator::Node* ImageAtlasGenerator::Node::Insert(AtlasElement& element)
{
    if (NULL != child1_)
    {
        Node* node = child1_->Insert(element);
        if (NULL != node)
            return node;

        assert(NULL != child2_); // Should always be created in pairs
        return child2_->Insert(element);
    }
    else
    {
        if (occupied_)
            return NULL;

        int elemWidth = element.image_->GetWidth();
        int nodeWidth = Width();
        if (elemWidth > nodeWidth)
            return NULL;

        int elemHeight = element.image_->GetHeight();
        int nodeHeight = Height();
        if (elemHeight > nodeHeight)
            return NULL;

        if (elemWidth == nodeWidth &&
            elemHeight == nodeHeight)
        {
            occupied_ = true;
            return this;
        }

        int dw = nodeWidth - elemWidth;
        int dh = nodeHeight - elemHeight;
        if (dw > dh)
        {
            child1_ = new Node(left_, top_, left_ + elemWidth, bottom_);
            child2_ = new Node(left_+ elemWidth, top_, right_, bottom_);
        }
        else
        {
            child1_ = new Node(left_, top_, right_, top_ + elemHeight);
            child2_ = new Node(left_, top_+ elemHeight, right_, bottom_);
        }

        return child1_->Insert(element);
    }
}
