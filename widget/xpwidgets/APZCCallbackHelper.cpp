/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCCallbackHelper.h"
#include "mozilla/Preferences.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsIDOMElement.h"
#include "nsIInterfaceRequestorUtils.h"
#include "TiledLayerBuffer.h" // For TILEDLAYERBUFFER_TILE_SIZE

namespace mozilla {
namespace widget {

bool
APZCCallbackHelper::HasValidPresShellId(nsIDOMWindowUtils* aUtils,
                                        const FrameMetrics& aMetrics)
{
    MOZ_ASSERT(aUtils);

    uint32_t presShellId;
    nsresult rv = aUtils->GetPresShellId(&presShellId);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    return NS_SUCCEEDED(rv) && aMetrics.mPresShellId == presShellId;
}

/**
 * Expands a given rectangle to the next tile boundary. Note, this will
 * expand the rectangle if it is already on tile boundaries.
 */
static CSSRect ExpandDisplayPortToTileBoundaries(
  const CSSRect& aDisplayPort,
  const CSSToLayerScale& aLayerPixelsPerCSSPixel)
{
  // Convert the given rect to layer coordinates so we can inflate to tile
  // boundaries (layer space corresponds to texture pixel space here).
  LayerRect displayPortInLayerSpace = aDisplayPort * aLayerPixelsPerCSSPixel;

  // Inflate the rectangle by 1 so that we always push to the next tile
  // boundary. This is desirable to stop from having a rectangle with a
  // moving origin occasionally being smaller when it coincidentally lines
  // up to tile boundaries.
  displayPortInLayerSpace.Inflate(1);

  // Now nudge the rectangle to the nearest equal or larger tile boundary.
  gfxFloat left = TILEDLAYERBUFFER_TILE_SIZE
    * floor(displayPortInLayerSpace.x / TILEDLAYERBUFFER_TILE_SIZE);
  gfxFloat top = TILEDLAYERBUFFER_TILE_SIZE
    * floor(displayPortInLayerSpace.y / TILEDLAYERBUFFER_TILE_SIZE);
  gfxFloat right = TILEDLAYERBUFFER_TILE_SIZE
    * ceil(displayPortInLayerSpace.XMost() / TILEDLAYERBUFFER_TILE_SIZE);
  gfxFloat bottom = TILEDLAYERBUFFER_TILE_SIZE
    * ceil(displayPortInLayerSpace.YMost() / TILEDLAYERBUFFER_TILE_SIZE);

  displayPortInLayerSpace = LayerRect(left, top, right - left, bottom - top);
  CSSRect displayPort = displayPortInLayerSpace / aLayerPixelsPerCSSPixel;

  return displayPort;
}

static void
MaybeAlignAndClampDisplayPort(mozilla::layers::FrameMetrics& aFrameMetrics,
                              const CSSPoint& aActualScrollOffset)
{
  // Correct the display-port by the difference between the requested scroll
  // offset and the resulting scroll offset after setting the requested value.
  CSSRect& displayPort = aFrameMetrics.mDisplayPort;
  displayPort += aActualScrollOffset - aFrameMetrics.mScrollOffset;

  // Expand the display port to the next tile boundaries, if tiled thebes layers
  // are enabled.
  if (Preferences::GetBool("layers.force-tiles")) {
    displayPort =
      ExpandDisplayPortToTileBoundaries(displayPort + aActualScrollOffset,
                                        aFrameMetrics.LayersPixelsPerCSSPixel())
      - aActualScrollOffset;
  }

  // Finally, clamp the display port to the scrollable rect.
  CSSRect scrollableRect = aFrameMetrics.mScrollableRect;
  displayPort = scrollableRect.Intersect(displayPort + aActualScrollOffset)
    - aActualScrollOffset;
}

void
APZCCallbackHelper::UpdateRootFrame(nsIDOMWindowUtils* aUtils,
                                    FrameMetrics& aMetrics)
{
    // Precondition checks
    MOZ_ASSERT(aUtils);
    if (aMetrics.mScrollId == FrameMetrics::NULL_SCROLL_ID) {
        return;
    }

    // Set the scroll port size, which determines the scroll range. For example if
    // a 500-pixel document is shown in a 100-pixel frame, the scroll port length would
    // be 100, and gecko would limit the maximum scroll offset to 400 (so as to prevent
    // overscroll). Note that if the content here was zoomed to 2x, the document would
    // be 1000 pixels long but the frame would still be 100 pixels, and so the maximum
    // scroll range would be 900. Therefore this calculation depends on the zoom applied
    // to the content relative to the container.
    CSSSize scrollPort = aMetrics.CalculateCompositedRectInCssPixels().Size();
    aUtils->SetScrollPositionClampingScrollPortSize(scrollPort.width, scrollPort.height);

    // Scroll the window to the desired spot
    aUtils->ScrollToCSSPixelsApproximate(aMetrics.mScrollOffset.x, aMetrics.mScrollOffset.y, nullptr);

    // Re-query the scroll position after setting it so that anything that relies on it
    // can have an accurate value.
    CSSPoint actualScrollOffset;
    aUtils->GetScrollXYFloat(false, &actualScrollOffset.x, &actualScrollOffset.y);

    // Correct the display port due to the difference between mScrollOffset and the
    // actual scroll offset, possibly align it to tile boundaries (if tiled layers are
    // enabled), and clamp it to the scrollable rect.
    MaybeAlignAndClampDisplayPort(aMetrics, actualScrollOffset);
    aMetrics.mScrollOffset = actualScrollOffset;

    // The mZoom variable on the frame metrics stores the CSS-to-screen scale for this
    // frame. This scale includes all of the (cumulative) resolutions set on the presShells
    // from the root down to this frame. However, when setting the resolution, we only
    // want the piece of the resolution that corresponds to this presShell, rather than
    // all of the cumulative stuff, so we need to divide out the parent resolutions.
    // Finally, we multiply by a ScreenToLayerScale of 1.0f because the goal here is to
    // take the async zoom calculated by the APZC and tell gecko about it (turning it into
    // a "sync" zoom) which will update the resolution at which the layer is painted.
    mozilla::layers::ParentLayerToLayerScale presShellResolution =
        aMetrics.mZoom
        / aMetrics.mDevPixelsPerCSSPixel
        / aMetrics.GetParentResolution()
        * ScreenToLayerScale(1.0f);
    aUtils->SetResolution(presShellResolution.scale, presShellResolution.scale);

    // Finally, we set the displayport.
    nsCOMPtr<nsIContent> content = nsLayoutUtils::FindContentFor(aMetrics.mScrollId);
    if (!content) {
        return;
    }
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(content);
    if (!element) {
        return;
    }
    aUtils->SetDisplayPortForElement(aMetrics.mDisplayPort.x,
                                     aMetrics.mDisplayPort.y,
                                     aMetrics.mDisplayPort.width,
                                     aMetrics.mDisplayPort.height,
                                     element);
}

void
APZCCallbackHelper::UpdateSubFrame(nsIContent* aContent,
                                   FrameMetrics& aMetrics)
{
    // Precondition checks
    MOZ_ASSERT(aContent);
    if (aMetrics.mScrollId == FrameMetrics::NULL_SCROLL_ID) {
        return;
    }

    nsCOMPtr<nsIDOMWindowUtils> utils = GetDOMWindowUtils(aContent);
    if (!utils) {
        return;
    }

    // We currently do not support zooming arbitrary subframes. They can only
    // be scrolled, so here we only have to set the scroll position and displayport.

    CSSPoint actualScrollOffset;
    nsIScrollableFrame* sf = nsLayoutUtils::FindScrollableFrameFor(aMetrics.mScrollId);
    if (sf) {
        sf->ScrollToCSSPixelsApproximate(aMetrics.mScrollOffset);
        actualScrollOffset = CSSPoint::FromAppUnits(sf->GetScrollPosition());
    }

    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aContent);
    if (element) {
        MaybeAlignAndClampDisplayPort(aMetrics, actualScrollOffset);
        utils->SetDisplayPortForElement(aMetrics.mDisplayPort.x,
                                        aMetrics.mDisplayPort.y,
                                        aMetrics.mDisplayPort.width,
                                        aMetrics.mDisplayPort.height,
                                        element);
    }

    aMetrics.mScrollOffset = actualScrollOffset;
}

already_AddRefed<nsIDOMWindowUtils>
APZCCallbackHelper::GetDOMWindowUtils(const nsIDocument* aDoc)
{
    nsCOMPtr<nsIDOMWindowUtils> utils;
    nsCOMPtr<nsIDOMWindow> window = aDoc->GetDefaultView();
    if (window) {
        utils = do_GetInterface(window);
    }
    return utils.forget();
}

already_AddRefed<nsIDOMWindowUtils>
APZCCallbackHelper::GetDOMWindowUtils(const nsIContent* aContent)
{
    nsCOMPtr<nsIDOMWindowUtils> utils;
    nsIDocument* doc = aContent->GetCurrentDoc();
    if (doc) {
        utils = GetDOMWindowUtils(doc);
    }
    return utils.forget();
}

bool
APZCCallbackHelper::GetScrollIdentifiers(const nsIContent* aContent,
                                         uint32_t* aPresShellIdOut,
                                         FrameMetrics::ViewID* aViewIdOut)
{
    if (!aContent || !nsLayoutUtils::FindIDFor(aContent, aViewIdOut)) {
        return false;
    }
    nsCOMPtr<nsIDOMWindowUtils> utils = GetDOMWindowUtils(aContent);
    return utils && (utils->GetPresShellId(aPresShellIdOut) == NS_OK);
}

}
}
