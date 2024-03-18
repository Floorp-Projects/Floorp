/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1799980 - Healow gets stuck in an infinite loop while pages load
 *
 * This patch keeps Healow's localization scripts from getting stuck in
 * an infinite loop while their pages are loading.
 *
 * This happens because they use synchronous XMLHttpRequests to fetch a
 * JSON file with their localized text on the first call to their i18n
 * function, and then force subsequent calls to wait for it by waiting
 * in an infinite loop.
 *
 * But since they're in an infinite loop, the code after the syncXHR will
 * never be able to run, so this ultimately triggers a slow script warning.
 *
 * We can improve this by just preventing the infinite loop from happening,
 * though since they disable caching on their JSON files it means that more
 * XHRs may happen. But since those files are small, this seems like a
 * reasonable compromise until they migrate to a better i18n solution.
 *
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1799980 for details.
 */

/* globals exportFunction */

Object.defineProperty(window.wrappedJSObject, "ajaxRequestProcessing", {
  get: exportFunction(function () {
    return false;
  }, window),

  set: exportFunction(function () {}, window),
});
