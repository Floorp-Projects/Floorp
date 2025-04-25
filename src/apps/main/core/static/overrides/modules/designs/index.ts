/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const overrides = [
  () => {
    const progressListener = {
      onLocationChange(
        webProgress: nsIWebProgress,
        request: nsIRequest,
        location: nsIURI,
        flags: number,
      ) {
        window.setTimeout(() => {
          const IdentityIconLabel = document?.getElementById(
            "identity-icon-label",
          ) as XULElement;

          if (IdentityIconLabel) {
            IdentityIconLabel.setAttribute("value", "Floorp");
            IdentityIconLabel.value = "Floorp";
            IdentityIconLabel.textContent = "Floorp";
            IdentityIconLabel.setAttribute("collapsed", "false");
          } else {
            console.log("Floorp: identity-icon label not found");
          }
        }, 20);
      },
    };

    const filter = Cc[
      "@mozilla.org/appshell/component/browser-status-filter;1"
    ].createInstance(Ci.nsIWebProgress);

    filter.addProgressListener(progressListener, Ci.nsIWebProgress.NOTIFY_ALL);
    const webProgress =
      globalThis?.docShell?.QueryInterface?.(Ci.nsIInterfaceRequestor)
        ?.getInterface?.(Ci.nsIWebProgress) || null;
    webProgress?.addProgressListener(filter, Ci.nsIWebProgress.NOTIFY_ALL);
  },
];
