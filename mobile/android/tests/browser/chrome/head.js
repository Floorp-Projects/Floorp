/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function promiseBrowserEvent(browser, eventType) {
  return new Promise((resolve) => {
    function handle(event) {
      // Since we'll be redirecting, don't make assumptions about the given URL and the loaded URL
      if (event.target != browser.contentDocument || event.target.location.href == "about:blank") {
        info("Skipping spurious '" + eventType + "' event" + " for " + event.target.location.href);
        return;
      }
      info("Received event " + eventType + " from browser");
      browser.removeEventListener(eventType, handle, true);
      resolve(event);
    }

    browser.addEventListener(eventType, handle, true);
    info("Now waiting for " + eventType + " event from browser");
  });
}
