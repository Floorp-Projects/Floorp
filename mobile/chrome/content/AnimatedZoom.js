// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benjamin Stover <bstover@mozilla.com>
 *   Matt Brubeck <mbrubeck@mozilla.com>
 *   Jaakko Kiviluoto <jaakko.kiviluoto@digia.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Responsible for zooming in to a given view rectangle
 */
const AnimatedZoom = {
  /** Starts an animated zoom to zoomRect. */
  animateTo: function(aZoomRect) {
    if (!aZoomRect)
      return;

    this.zoomTo = aZoomRect.clone();

    if (this.animationDuration === undefined)
      this.animationDuration = Services.prefs.getIntPref("browser.ui.zoom.animationDuration");

    Browser.hideSidebars();
    Browser.hideTitlebar();
    Browser.forceChromeReflow();

    this.beginTime = mozAnimationStartTime;

    // Check if zooming animations were occuring before.
    if (this.zoomRect) {
      this.zoomFrom = this.zoomRect;
    } else {
      this.zoomFrom = this.getStartRect();
      this.updateTo(this.zoomFrom);

      window.addEventListener("MozBeforePaint", this, false);
      mozRequestAnimationFrame();

      let event = document.createEvent("Events");
      event.initEvent("AnimatedZoomBegin", true, true);
      window.dispatchEvent(event);
    }
  },

  /** Get the visible rect, in device pixels relative to the content origin. */
  getStartRect: function getStartRect() {
    let browser = getBrowser();
    let bcr = browser.getBoundingClientRect();
    let scroll = browser.getRootView().getPosition();
    return new Rect(scroll.x, scroll.y, bcr.width, bcr.height);
  },

  /** Update the visible rect, in device pixels relative to the content origin. */
  updateTo: function(nextRect) {
    let browser = getBrowser();
    let zoomRatio = window.innerWidth / nextRect.width;
    let zoomLevel = browser.scale * zoomRatio;

    let contentView = browser.getRootView();
    contentView.setScale(zoomLevel);
    contentView.scrollTo(nextRect.left * zoomRatio, nextRect.top * zoomRatio);

    this.zoomRect = nextRect;
  },

  /** Stop animation, zoom to point, and clean up. */
  finish: function() {
    window.removeEventListener("MozBeforePaint", this, false);
    Browser.setVisibleRect(this.zoomTo || this.zoomRect);
    this.beginTime = null;
    this.zoomTo = null;
    this.zoomFrom = null;
    this.zoomRect = null;

    let event = document.createEvent("Events");
    event.initEvent("AnimatedZoomEnd", true, true);
    window.dispatchEvent(event);
  },

  isZooming: function isZooming() {
    return this.beginTime != null;
  },

  handleEvent: function(aEvent) {
    try {
      let tdiff = aEvent.timeStamp - this.beginTime;
      let counter = tdiff / this.animationDuration;
      if (counter < 1) {
        // update browser to interpolated rectangle
        let rect = this.zoomFrom.blend(this.zoomTo, counter);
        this.updateTo(rect);
        mozRequestAnimationFrame();
      } else {
        // last cycle already rendered final scaled image, now clean up
        this.finish();
      }
    } catch(e) {
      this.finish();
      throw e;
    }
  }
};
