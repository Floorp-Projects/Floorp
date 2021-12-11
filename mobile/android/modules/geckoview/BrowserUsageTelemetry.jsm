/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["getUniqueDomainsVisitedInPast24Hours"];

// Used by nsIBrowserUsage
function getUniqueDomainsVisitedInPast24Hours() {
  // The prompting heuristic for the storage access API looks at 1% of the
  // number of the domains visited in the past 24 hours, with a minimum cap of
  // 5 domains, in order to prevent prompts from showing up before a tracker is
  // about to obtain tracking power over a significant portion of the user's
  // cross-site browsing activity (that is, we do not want to allow automatic
  // access grants over 1% of the domains). We have the
  // dom.storage_access.max_concurrent_auto_grants which establishes the
  // minimum cap here (set to 5 by default) so if we return 0 here the minimum
  // cap would always take effect. That would only become inaccurate if the
  // user has browsed more than 500 top-level eTLD's in the past 24 hours,
  // which should be a very unlikely scenario on mobile anyway.

  return 0;
}
