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
  startScale: null,

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

    this.start();

    // Check if zooming animations were occuring before.
    if (!this.zoomRect) {
      this.updateTo(this.zoomFrom);

      mozRequestAnimationFrame(this);

      let event = document.createEvent("Events");
      event.initEvent("AnimatedZoomBegin", true, true);
      window.dispatchEvent(event);
    }
  },

  start: function start() {
    this.tab = Browser.selectedTab;
    this.browser = this.tab.browser;
    this.bcr = this.browser.getBoundingClientRect();
    this.zoomFrom = this.zoomRect || this.getStartRect();
    this.startScale = this.browser.scale;
    this.beginTime = mozAnimationStartTime;
  },

  /** Get the visible rect, in device pixels relative to the content origin. */
  getStartRect: function getStartRect() {
    let browser = this.browser;
    let scroll = browser.getRootView().getPosition();
    return new Rect(scroll.x, scroll.y, this.bcr.width, this.bcr.height);
  },

  /** Update the visible rect, in device pixels relative to the content origin. */
  updateTo: function(nextRect) {
    let zoomRatio = this.bcr.width / nextRect.width;
    let scale = this.startScale * zoomRatio;
    let scrollX = nextRect.left * zoomRatio;
    let scrollY = nextRect.top * zoomRatio;

    this.browser.fuzzyZoom(scale, scrollX, scrollY);

    this.zoomRect = nextRect;
  },

  /** Stop animation, zoom to point, and clean up. */
  finish: function() {
    this.updateTo(this.zoomTo || this.zoomRect);

    // Check whether the zoom limits have changed since the animation started.
    let browser = this.browser;
    let finalScale = this.tab.clampZoomLevel(browser.scale);
    if (browser.scale != finalScale)
      browser.scale = finalScale; // scale= calls finishFuzzyZoom.
    else
      browser.finishFuzzyZoom();

    Browser.hideSidebars();
    Browser.hideTitlebar();

    this.beginTime = null;
    this.zoomTo = null;
    this.zoomFrom = null;
    this.zoomRect = null;
    this.startScale = null;

    let event = document.createEvent("Events");
    event.initEvent("AnimatedZoomEnd", true, true);
    window.dispatchEvent(event);
    browser._updateCSSViewport();
  },

  isZooming: function isZooming() {
    return this.beginTime != null;
  },

  sample: function(aTimeStamp) {
    try {
      let tdiff = aTimeStamp - this.beginTime;
      let counter = tdiff / this.animationDuration;
      if (counter < 1) {
        // update browser to interpolated rectangle
        let rect = this.zoomFrom.blend(this.zoomTo, counter);
        this.updateTo(rect);
        mozRequestAnimationFrame(this);
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
