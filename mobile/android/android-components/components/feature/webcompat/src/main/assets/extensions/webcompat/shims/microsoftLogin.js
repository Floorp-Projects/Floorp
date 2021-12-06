/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const SANDBOX_ATTR = "allow-storage-access-by-user-activation";

console.warn(
  "Firefox calls the Storage Access API on behalf of the site. See https://bugzilla.mozilla.org/show_bug.cgi?id=1638383 for details."
);

// Watches for MS auth iframes and adds missing sandbox attribute. The attribute
// is required so the third-party iframe can gain access to its first party
// storage via the Storage Access API.
function init() {
  const observer = new MutationObserver(() => {
    document.body
      .querySelectorAll("iframe[id^='msalRenewFrame'][sandbox]")
      .forEach(frame => {
        frame.sandbox.add(SANDBOX_ATTR);
      });
  });

  observer.observe(document.body, {
    attributes: true,
    subtree: false,
    childList: true,
  });
}
window.addEventListener("DOMContentLoaded", init);
