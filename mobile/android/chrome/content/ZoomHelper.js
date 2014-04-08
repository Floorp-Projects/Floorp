/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var ZoomHelper = {
  zoomInAndSnapToRange: function(aRange) {
    // aRange is always non-null here, since a check happened previously.
    let viewport = BrowserApp.selectedTab.getViewport();
    let fudge = 15; // Add a bit of fudge.
    let boundingElement = aRange.offsetNode;
    while (!boundingElement.getBoundingClientRect && boundingElement.parentNode) {
      boundingElement = boundingElement.parentNode;
    }

    let rect = ElementTouchHelper.getBoundingContentRect(boundingElement);
    let drRect = aRange.getClientRect();
    let scrollTop =
      BrowserApp.selectedBrowser.contentDocument.documentElement.scrollTop ||
      BrowserApp.selectedBrowser.contentDocument.body.scrollTop;

    // We subtract half the height of the viewport so that we can (ideally)
    // center the area of interest on the screen.
    let topPos = scrollTop + drRect.top - (viewport.cssHeight / 2.0);

    // Factor in the border and padding
    let boundingStyle = window.getComputedStyle(boundingElement);
    let leftAdjustment = parseInt(boundingStyle.paddingLeft) +
                         parseInt(boundingStyle.borderLeftWidth);

    BrowserApp.selectedTab._mReflozPositioned = true;

    rect.type = "Browser:ZoomToRect";
    rect.x = Math.max(viewport.cssPageLeft, rect.x  - fudge + leftAdjustment);
    rect.y = Math.max(topPos, viewport.cssPageTop);
    rect.w = viewport.cssWidth;
    rect.h = viewport.cssHeight;
    rect.animate = false;

    sendMessageToJava(rect);
    BrowserApp.selectedTab._mReflozPoint = null;
  },

  zoomOut: function() {
    BrowserEventHandler.resetMaxLineBoxWidth();
    sendMessageToJava({ type: "Browser:ZoomToPageWidth" });
  },

  isRectZoomedIn: function(aRect, aViewport) {
    // This function checks to see if the area of the rect visible in the
    // viewport (i.e. the "overlapArea" variable below) is approximately
    // the max area of the rect we can show. It also checks that the rect
    // is actually on-screen by testing the left and right edges of the rect.
    // In effect, this tells us whether or not zooming in to this rect
    // will significantly change what the user is seeing.
    const minDifference = -20;
    const maxDifference = 20;
    const maxZoomAllowed = 4; // keep this in sync with mobile/android/base/ui/PanZoomController.MAX_ZOOM

    let vRect = new Rect(aViewport.cssX, aViewport.cssY, aViewport.cssWidth, aViewport.cssHeight);
    let overlap = vRect.intersect(aRect);
    let overlapArea = overlap.width * overlap.height;
    let availHeight = Math.min(aRect.width * vRect.height / vRect.width, aRect.height);
    let showing = overlapArea / (aRect.width * availHeight);
    let dw = (aRect.width - vRect.width);
    let dx = (aRect.x - vRect.x);

    if (fuzzyEquals(aViewport.zoom, maxZoomAllowed) && overlap.width / aRect.width > 0.9) {
      // we're already at the max zoom and the block is not spilling off the side of the screen so that even
      // if the block isn't taking up most of the viewport we can't pan/zoom in any more. return true so that we zoom out
      return true;
    }

    return (showing > 0.9 &&
            dx > minDifference && dx < maxDifference &&
            dw > minDifference && dw < maxDifference);
  },

  /* Zoom to an element, optionally keeping a particular part of it
   * in view if it is really tall.
   */
  zoomToElement: function(aElement, aClickY = -1, aCanZoomOut = true, aCanScrollHorizontally = true) {
    let rect = ElementTouchHelper.getBoundingContentRect(aElement);

    const margin = 15;

    if(!rect.h || !rect.w) {
      rect.h = rect.height;
      rect.w = rect.width;
    }

    let viewport = BrowserApp.selectedTab.getViewport();
    let bRect = new Rect(aCanScrollHorizontally ? Math.max(viewport.cssPageLeft, rect.x - margin) : viewport.cssX,
                         rect.y,
                         aCanScrollHorizontally ? rect.w + 2 * margin : viewport.cssWidth,
                         rect.h);
    // constrict the rect to the screen's right edge
    bRect.width = Math.min(bRect.width, viewport.cssPageRight - bRect.x);

    // if the rect is already taking up most of the visible area and is stretching the
    // width of the page, then we want to zoom out instead.
    if (BrowserEventHandler.mReflozPref) {
      let zoomFactor = BrowserApp.selectedTab.getZoomToMinFontSize(aElement);

      bRect.width = zoomFactor <= 1.0 ? bRect.width : gScreenWidth / zoomFactor;
      bRect.height = zoomFactor <= 1.0 ? bRect.height : bRect.height / zoomFactor;
      if (zoomFactor == 1.0 || ZoomHelper.isRectZoomedIn(bRect, viewport)) {
        if (aCanZoomOut) {
          ZoomHelper.zoomOut();
        }
        return;
      }
    } else if (ZoomHelper.isRectZoomedIn(bRect, viewport)) {
      if (aCanZoomOut) {
        ZoomHelper.zoomOut();
      }
      return;
    }

    ZoomHelper.zoomToRect(bRect, aClickY);
  },

  zoomToRect: function(aRect, aClickY = -1) {
    let viewport = BrowserApp.selectedTab.getViewport();
    let rect = {};

    rect.type = "Browser:ZoomToRect";
    rect.x = aRect.x;
    rect.y = aRect.y;
    rect.w = aRect.width;
    rect.h = Math.min(aRect.width * viewport.cssHeight / viewport.cssWidth, aRect.height);

    if (aClickY >= 0) {
      // if the block we're zooming to is really tall, and we want to keep a particular
      // part of it in view, then adjust the y-coordinate of the target rect accordingly.
      // the 1.2 multiplier is just a little fuzz to compensate for bRect including horizontal
      // margins but not vertical ones.
      let cssTapY = viewport.cssY + aClickY;
      if ((aRect.height > rect.h) && (cssTapY > rect.y + (rect.h * 1.2))) {
        rect.y = cssTapY - (rect.h / 2);
      }
    }

    if (rect.w > viewport.cssWidth || rect.h > viewport.cssHeight) {
      BrowserEventHandler.resetMaxLineBoxWidth();
    }

    sendMessageToJava(rect);
  },
};
