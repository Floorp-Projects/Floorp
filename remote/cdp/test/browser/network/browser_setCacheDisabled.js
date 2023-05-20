/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { INHIBIT_CACHING, LOAD_BYPASS_CACHE, LOAD_NORMAL } = Ci.nsIRequest;

const TEST_PAGE =
  "https://example.com/browser/remote/cdp/test/browser/network/doc_empty.html";

add_task(async function cacheEnabledAfterDisabled({ client }) {
  const { Network } = client;
  await Network.setCacheDisabled({ cacheDisabled: true });
  await Network.setCacheDisabled({ cacheDisabled: false });

  await watchLoadFlags(LOAD_NORMAL, TEST_PAGE);
  await loadURL(TEST_PAGE);
  await waitForLoadFlags();
});

add_task(async function cacheEnabledByDefault({ Network }) {
  await watchLoadFlags(LOAD_NORMAL, TEST_PAGE);
  await loadURL(TEST_PAGE);
  await waitForLoadFlags();
});

add_task(async function cacheDisabled({ client }) {
  const { Network } = client;
  await Network.setCacheDisabled({ cacheDisabled: true });

  await watchLoadFlags(LOAD_BYPASS_CACHE | INHIBIT_CACHING, TEST_PAGE);
  await loadURL(TEST_PAGE);
  await waitForLoadFlags();
});

// This helper will resolve when the content-process progressListener is started
// and ready to monitor requests.
// The promise itself will resolve when the progressListener will detect the
// expected flags for the provided url.
function watchLoadFlags(flags, url) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [{ flags, url }],
    async (options = {}) => {
      const { flags, url } = options;

      // an nsIWebProgressListener that checks all requests made by the docShell
      // have the flags we expect.
      var RequestWatcher = {
        init(docShell, expectedLoadFlags, url, callback) {
          this.callback = callback;
          this.docShell = docShell;
          this.expectedLoadFlags = expectedLoadFlags;
          this.url = url;

          this.requestCount = 0;

          const { NOTIFY_STATE_DOCUMENT, NOTIFY_STATE_REQUEST } =
            Ci.nsIWebProgress;

          this.docShell
            .QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIWebProgress)
            .addProgressListener(
              this,
              NOTIFY_STATE_DOCUMENT | NOTIFY_STATE_REQUEST
            );
        },

        onStateChange(webProgress, request, flags, status) {
          // We are checking requests - if there isn't one, ignore it.
          if (!request) {
            return;
          }

          // We will usually see requests for 'about:document-onload-blocker' not
          // have the flag, so we just ignore them.
          // We also see, eg, resource://gre-resources/loading-image.png, so
          // skip resource:// URLs too.
          // We may also see, eg, chrome://global/skin/icons/chevron.svg, so
          // skip chrome:// URLs too.
          if (
            request.name.startsWith("about:") ||
            request.name.startsWith("resource:") ||
            request.name.startsWith("chrome:")
          ) {
            return;
          }
          is(
            request.loadFlags & this.expectedLoadFlags,
            this.expectedLoadFlags,
            "request " + request.name + " has the expected flags"
          );
          this.requestCount += 1;

          var stopFlags =
            Ci.nsIWebProgressListener.STATE_STOP |
            Ci.nsIWebProgressListener.STATE_IS_DOCUMENT;

          if (request.name == this.url && (flags & stopFlags) == stopFlags) {
            this.docShell.removeProgressListener(this);
            ok(
              this.requestCount > 1,
              this.url + " saw " + this.requestCount + " requests"
            );
            this.callback();
          }
        },

        QueryInterface: ChromeUtils.generateQI([
          "nsIWebProgressListener",
          "nsISupportsWeakReference",
        ]),
      };

      // Store the promise that should be awaited for on the current window.
      content.resolveCheckLoadFlags = new Promise(resolve => {
        RequestWatcher.init(docShell, flags, url, resolve);
      });
    }
  );
}

// Wait for the latest promise created in-content by watchLoadFlags to resolve.
function waitForLoadFlags() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    await content.resolveCheckLoadFlags;
    delete content.resolveCheckLoadFlags;
  });
}
