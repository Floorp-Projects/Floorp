"use strict";

/**
 * ebay.com - Ebay jumps to the top once you scroll to the bottom on a mobile device
 * Bug #1522755 - https://bugzilla.mozilla.org/show_bug.cgi?id=1522755
 *
 * If scroll anchoring is enabled, scrolling all the way to the bottom causes the
 * site to scroll back up about half the page. See the bug linked above for a
 * detailed explanation.
 * Overriding the window.infinity.Page.prototype.hasVacancy with a version that
 * uses a cached clientHeight resolves this issue.
 *
 * NOTE: This script runs at document_idle, and this is important in order for us
 * to be able to override the method we want to override.
 */

/* globals exportFunction */

if (window.wrappedJSObject.infinity && window.wrappedJSObject.infinity.Page) {
  console.info("window.infinity.Page.prototype.hasVacancy has been overriden for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1520666 for details.");

  // Assign the current clientHeight initially, then only update the global
  // variable on a resize event.
  window.mozCompatCachedClientHeight = document.documentElement.clientHeight;
  window.wrappedJSObject.addEventListener("resize", (exportFunction(function() {
    window.mozCompatCachedClientHeight = document.documentElement.clientHeight;
  }, window)));

  // Replace the original window.infinity.Page.prototype.hasVacancy method with
  // a copied implementation using our cached client height.
  window.wrappedJSObject.infinity.Page.prototype.hasVacancy = (exportFunction(function() {
    return this.height < window.mozCompatCachedClientHeight * 3;
  }, window));
}
