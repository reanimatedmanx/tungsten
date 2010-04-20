/*
 * Copyright (C) 2007, 2008 Rob Buis <buis@kde.org>
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 *           (C) 2007 Eric Seidel <eric@webkit.org>
 *           (C) 2009 Google, Inc.  All rights reserved.
 *           (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#if ENABLE(SVG)
#include "SVGRenderSupport.h"

#include "AffineTransform.h"
#include "Document.h"
#include "ImageBuffer.h"
#include "RenderObject.h"
#include "RenderSVGContainer.h"
#include "RenderSVGResource.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceMarker.h"
#include "RenderSVGResourceMasker.h"
#include "RenderView.h"
#include "SVGResourceFilter.h"
#include "SVGStyledElement.h"
#include "SVGURIReference.h"
#include "TransformState.h"
#include <wtf/UnusedParam.h>

namespace WebCore {

SVGRenderBase::~SVGRenderBase()
{
}

IntRect SVGRenderBase::clippedOverflowRectForRepaint(RenderObject* object, RenderBoxModelObject* repaintContainer)
{
    // Return early for any cases where we don't actually paint
    if (object->style()->visibility() != VISIBLE && !object->enclosingLayer()->hasVisibleContent())
        return IntRect();

    // Pass our local paint rect to computeRectForRepaint() which will
    // map to parent coords and recurse up the parent chain.
    IntRect repaintRect = enclosingIntRect(object->repaintRectInLocalCoordinates());
    object->computeRectForRepaint(repaintContainer, repaintRect);
    return repaintRect;
}

void SVGRenderBase::computeRectForRepaint(RenderObject* object, RenderBoxModelObject* repaintContainer, IntRect& repaintRect, bool fixed)
{
    object->style()->svgStyle()->inflateForShadow(repaintRect);

    // Translate to coords in our parent renderer, and then call computeRectForRepaint on our parent
    repaintRect = object->localToParentTransform().mapRect(repaintRect);
    object->parent()->computeRectForRepaint(repaintContainer, repaintRect, fixed);
}

void SVGRenderBase::mapLocalToContainer(const RenderObject* object, RenderBoxModelObject* repaintContainer, bool fixed , bool useTransforms, TransformState& transformState)
{
    ASSERT(!fixed); // We should have no fixed content in the SVG rendering tree.
    ASSERT(useTransforms); // Mapping a point through SVG w/o respecting transforms is useless.
    transformState.applyTransform(object->localToParentTransform());
    object->parent()->mapLocalToContainer(repaintContainer, fixed, useTransforms, transformState);
}

bool SVGRenderBase::prepareToRenderSVGContent(RenderObject* object, RenderObject::PaintInfo& paintInfo, const FloatRect& repaintRect, SVGResourceFilter*& filter, SVGResourceFilter* rootFilter)
{
#if !ENABLE(FILTERS)
    UNUSED_PARAM(filter);
    UNUSED_PARAM(rootFilter);
#endif

    ASSERT(object);
    SVGElement* svgElement = static_cast<SVGElement*>(object->node());
    ASSERT(svgElement && svgElement->document() && svgElement->isStyled());

    SVGStyledElement* styledElement = static_cast<SVGStyledElement*>(svgElement);
    const RenderStyle* style = object->style();
    ASSERT(style);

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

    // Setup transparency layers before setting up filters!
    float opacity = style->opacity(); 
    if (opacity < 1.0f) {
        paintInfo.context->clip(repaintRect);
        paintInfo.context->beginTransparencyLayer(opacity);
    }

    if (const ShadowData* shadow = svgStyle->shadow()) {
        paintInfo.context->clip(repaintRect);
        paintInfo.context->setShadow(IntSize(shadow->x(), shadow->y()), shadow->blur(), shadow->color(), style->colorSpace());
        paintInfo.context->beginTransparencyLayer(1.0f);
    }

#if ENABLE(FILTERS)
    AtomicString filterId(svgStyle->filterResource());
#endif

    AtomicString clipperId(svgStyle->clipperResource());
    AtomicString maskerId(svgStyle->maskerResource());

    Document* document = object->document();

#if ENABLE(FILTERS)
    SVGResourceFilter* newFilter = getFilterById(document, filterId, object);
    if (newFilter == rootFilter) {
        // Catch <text filter="url(#foo)">Test<tspan filter="url(#foo)">123</tspan></text>.
        // The filter is NOT meant to be applied twice in that case!
        filter = 0;
        filterId = String();
    } else
        filter = newFilter;
#endif

    if (RenderSVGResourceMasker* masker = getRenderSVGResourceById<RenderSVGResourceMasker>(document, maskerId)) {
        if (!masker->applyResource(object, paintInfo.context))
            return false;
    } else if (!maskerId.isEmpty())
        svgElement->document()->accessSVGExtensions()->addPendingResource(maskerId, styledElement);

    if (RenderSVGResourceClipper* clipper = getRenderSVGResourceById<RenderSVGResourceClipper>(document, clipperId))
        clipper->applyResource(object, paintInfo.context);
    else if (!clipperId.isEmpty())
        svgElement->document()->accessSVGExtensions()->addPendingResource(clipperId, styledElement);

#if ENABLE(FILTERS)
    if (filter) {
        filter->addClient(styledElement);
        if (!filter->prepareFilter(paintInfo.context, object))
            return false;
    } else if (!filterId.isEmpty())
        svgElement->document()->accessSVGExtensions()->addPendingResource(filterId, styledElement);
#endif

    return true;
}

void SVGRenderBase::finishRenderSVGContent(RenderObject* object, RenderObject::PaintInfo& paintInfo, SVGResourceFilter*& filter, GraphicsContext* savedContext)
{
#if !ENABLE(FILTERS)
    UNUSED_PARAM(filter);
    UNUSED_PARAM(savedContext);
#endif

    ASSERT(object);

    const RenderStyle* style = object->style();
    ASSERT(style);

#if ENABLE(FILTERS)
    if (filter) {
        filter->applyFilter(paintInfo.context, object);
        paintInfo.context = savedContext;
    }
#endif

    float opacity = style->opacity();    
    if (opacity < 1.0f)
        paintInfo.context->endTransparencyLayer();

    // This needs to be done separately from opacity, because if both properties are set,
    // then the transparency layers are nested. 
    if (style->svgStyle()->shadow())
        paintInfo.context->endTransparencyLayer();
}

void renderSubtreeToImage(ImageBuffer* image, RenderObject* item)
{
    ASSERT(item);
    ASSERT(image);
    ASSERT(image->context());

    // FIXME: This sets the rect to the viewable area of the current frame. This
    // is used to support text drawings to the ImageBuffer. See bug 30399.
    IntRect rect;
    FrameView* frameView = item->document()->view();
    if (frameView)
        rect = IntRect(0, 0, frameView->visibleWidth(), frameView->visibleHeight());
    RenderObject::PaintInfo info(image->context(), rect, PaintPhaseForeground, 0, 0, 0);

    // FIXME: isSVGContainer returns true for RenderSVGViewportContainer, so if this is ever
    // called with one of those, we will read from the wrong offset in an object due to a bad cast.
    RenderSVGContainer* svgContainer = 0;
    if (item && item->isSVGContainer())
        svgContainer = toRenderSVGContainer(item);

    bool drawsContents = svgContainer ? svgContainer->drawsContents() : false;
    if (svgContainer && !drawsContents)
        svgContainer->setDrawsContents(true);

    item->layoutIfNeeded();
    item->paint(info, 0, 0);

    if (svgContainer && !drawsContents)
        svgContainer->setDrawsContents(false);
}

void clampImageBufferSizeToViewport(FrameView* frameView, IntSize& size)
{
    if (!frameView)
        return;

    int viewWidth = frameView->visibleWidth();
    int viewHeight = frameView->visibleHeight();

    if (size.width() > viewWidth)
        size.setWidth(viewWidth);

    if (size.height() > viewHeight)
        size.setHeight(viewHeight);
}

FloatRect SVGRenderBase::computeContainerBoundingBox(const RenderObject* container, bool includeAllPaintedContent)
{
    FloatRect boundingBox;

    RenderObject* current = container->firstChild();
    for (; current != 0; current = current->nextSibling()) {
        FloatRect childBBox = includeAllPaintedContent ? current->repaintRectInLocalCoordinates() : current->objectBoundingBox();
        FloatRect childBBoxInLocalCoords = current->localToParentTransform().mapRect(childBBox);
        boundingBox.unite(childBBoxInLocalCoords);
    }

    return boundingBox;
}

void SVGRenderBase::layoutChildren(RenderObject* start, bool selfNeedsLayout)
{
    for (RenderObject* child = start->firstChild(); child; child = child->nextSibling()) {
        // Only force our kids to layout if we're being asked to relayout as a result of a parent changing
        // FIXME: We should be able to skip relayout of non-relative kids when only bounds size has changed
        // that's a possible future optimization using LayoutState
        // http://bugs.webkit.org/show_bug.cgi?id=15391
        bool needsLayout = selfNeedsLayout;
        if (!needsLayout) {
            if (SVGElement* element = child->node()->isSVGElement() ? static_cast<SVGElement*>(child->node()) : 0) {
                if (element->isStyled())
                    needsLayout = static_cast<SVGStyledElement*>(element)->hasRelativeValues();
            }
        }

        if (needsLayout)
            child->setNeedsLayout(true, false);

        child->layoutIfNeeded();
        ASSERT(!child->needsLayout());
    }
}

bool SVGRenderBase::isOverflowHidden(const RenderObject* object)
{
    // SVG doesn't support independent x/y overflow
    ASSERT(object->style()->overflowX() == object->style()->overflowY());

    // OSCROLL is never set for SVG - see CSSStyleSelector::adjustRenderStyle
    ASSERT(object->style()->overflowX() != OSCROLL);

    // RenderSVGRoot should never query for overflow state - it should always clip itself to the initial viewport size.
    ASSERT(!object->isRoot());

    return object->style()->overflowX() == OHIDDEN;
}

FloatRect SVGRenderBase::filterBoundingBoxForRenderer(const RenderObject* object) const
{
#if ENABLE(FILTERS)
    SVGResourceFilter* filter = getFilterById(object->document(), object->style()->svgStyle()->filterResource(), object);
    if (filter)
        return filter->filterBoundingBox(object->objectBoundingBox());
#else
    UNUSED_PARAM(object);
#endif
    return FloatRect();
}

FloatRect SVGRenderBase::clipperBoundingBoxForRenderer(const RenderObject* object) const
{
    if (RenderSVGResourceClipper* clipper = getRenderSVGResourceById<RenderSVGResourceClipper>(object->document(), object->style()->svgStyle()->clipperResource()))
        return clipper->resourceBoundingBox(object->objectBoundingBox());

    return FloatRect();
}

FloatRect SVGRenderBase::maskerBoundingBoxForRenderer(const RenderObject* object) const
{
    if (RenderSVGResourceMasker* masker = getRenderSVGResourceById<RenderSVGResourceMasker>(object->document(), object->style()->svgStyle()->maskerResource()))
        return masker->resourceBoundingBox(object->objectBoundingBox());

    return FloatRect();
}

void deregisterFromResources(RenderObject* object)
{
    // We only have the renderer for masker and clipper at the moment.
    if (RenderSVGResourceMasker* masker = getRenderSVGResourceById<RenderSVGResourceMasker>(object->document(), object->style()->svgStyle()->maskerResource()))
        masker->invalidateClient(object);
    if (RenderSVGResourceClipper* clipper = getRenderSVGResourceById<RenderSVGResourceClipper>(object->document(), object->style()->svgStyle()->clipperResource()))
        clipper->invalidateClient(object);
    if (RenderSVGResourceMarker* startMarker = getRenderSVGResourceById<RenderSVGResourceMarker>(object->document(), object->style()->svgStyle()->markerStartResource()))
        startMarker->invalidateClient(object);
    if (RenderSVGResourceMarker* midMarker = getRenderSVGResourceById<RenderSVGResourceMarker>(object->document(), object->style()->svgStyle()->markerMidResource()))
        midMarker->invalidateClient(object);
    if (RenderSVGResourceMarker* endMarker = getRenderSVGResourceById<RenderSVGResourceMarker>(object->document(), object->style()->svgStyle()->markerEndResource()))
        endMarker->invalidateClient(object);
}

void applyTransformToPaintInfo(RenderObject::PaintInfo& paintInfo, const AffineTransform& localToAncestorTransform)
{
    if (localToAncestorTransform.isIdentity())
        return;

    paintInfo.context->concatCTM(localToAncestorTransform);
    paintInfo.rect = localToAncestorTransform.inverse().mapRect(paintInfo.rect);
}

const RenderObject* findTextRootObject(const RenderObject* start)
{
    while (start && !start->isSVGText())
        start = start->parent();
    ASSERT(start);
    ASSERT(start->isSVGText());

    return start;
}

}

#endif
