/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

function _contentHeroHandler(isload) {
  var obs = null;
  var el = content.window.document.querySelector("[elementtiming]");
  if (el) {
    function callback(entries, observer) {
      entries.forEach(entry => {
        sendAsyncMessage("PageLoader:LoadEvent", {
          time: content.window.performance.now(),
          name: "tphero",
        });
        obs.disconnect();
      });
    }
    // we want the element 100% visible on the viewport
    var options = { root: null, rootMargin: "0px", threshold: [1] };
    try {
      obs = new content.window.IntersectionObserver(callback, options);
      obs.observe(el);
    } catch (err) {
      sendAsyncMessage("PageLoader:Error", { msg: err.message });
    }
  } else if (isload) {
    // If the hero element is added from a settimeout handler, it might not run before 'load'
    content.setTimeout(function() {
      _contentHeroHandler(false);
    }, 5000);
  } else {
    var err = "Could not find a tag with an elmenttiming attr on the page";
    sendAsyncMessage("PageLoader:Error", { msg: err });
  }
  return obs;
}

function _contentHeroLoadHandler() {
  _contentHeroHandler(true);
}

addEventListener(
  "load",
  // eslint-disable-next-line no-undef
  contentLoadHandlerCallback(_contentHeroLoadHandler),
  true
);
