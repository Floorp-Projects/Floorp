/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1713726 - Shim Ads by Google
 *
 * Sites relying on window.adsbygoogle may encounter breakage if it is blocked.
 * This shim provides a stub for that API to mitigate that breakage.
 */

if (window.adsbygoogle?.loaded === undefined) {
  window.adsbygoogle = {
    loaded: true,
    push() {},
  };
}

if (window.gapi?._pl === undefined) {
  const stub = {
    go() {},
    render: () => "",
  };
  window.gapi = {
    _pl: true,
    additnow: stub,
    autocomplete: stub,
    backdrop: stub,
    blogger: stub,
    commentcount: stub,
    comments: stub,
    community: stub,
    donation: stub,
    family_creation: stub,
    follow: stub,
    hangout: stub,
    health: stub,
    interactivepost: stub,
    load() {},
    logutil: {
      enableDebugLogging() {},
    },
    page: stub,
    partnersbadge: stub,
    person: stub,
    platform: {
      go() {},
    },
    playemm: stub,
    playreview: stub,
    plus: stub,
    plusone: stub,
    post: stub,
    profile: stub,
    ratingbadge: stub,
    recobar: stub,
    savetoandroidpay: stub,
    savetodrive: stub,
    savetowallet: stub,
    share: stub,
    sharetoclassroom: stub,
    shortlists: stub,
    signin: stub,
    signin2: stub,
    surveyoptin: stub,
    visibility: stub,
    youtube: stub,
    ytsubscribe: stub,
    zoomableimage: stub,
  };
}

for (const e of document.querySelectorAll("ins.adsbygoogle")) {
  e.style.maxWidth = "0px";
}
