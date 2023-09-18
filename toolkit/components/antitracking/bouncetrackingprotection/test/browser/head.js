/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SITE_A = "example.com";
const ORIGIN_A = `https://${SITE_A}`;

const SITE_B = "example.org";
const ORIGIN_B = `https://${SITE_B}`;

const SITE_C = "example.net";
const ORIGIN_C = `https://${SITE_C}`;

const SITE_TRACKER = "itisatracker.org";
const ORIGIN_TRACKER = `https://${SITE_TRACKER}`;

const SITE_TRACKER_B = "trackertest.org";
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const ORIGIN_TRACKER_B = `http://${SITE_TRACKER_B}`;

// Test message used for observing when the record-bounces method in
// BounceTrackingProtection.cpp has finished.
const OBSERVER_MSG_RECORD_BOUNCES_FINISHED = "test-record-bounces-finished";

const ROOT_DIR = getRootDirectory(gTestPath);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "bounceTrackingProtection",
  "@mozilla.org/bounce-tracking-protection;1",
  "nsIBounceTrackingProtection"
);

/**
 * Get the base url for the current test directory using the given origin.
 * @param {string} origin - Origin to use in URL.
 * @returns {string} - Generated URL as a string.
 */
function getBaseUrl(origin) {
  return ROOT_DIR.replace("chrome://mochitests/content", origin);
}

/**
 * Constructs a url for an intermediate "bounce" hop which represents a tracker.
 * @param {*} options - URL generation options.
 * @param {('server'|'client')} options.bounceType - Redirect type to use for
 * the bounce.
 * @param {string} [options.bounceOrigin] - The origin of the bounce URL.
 * @param {string} [options.targetURL] - URL to redirect to after the bounce.
 * @param {("cookie"|null)} [options.setState] - What type of state should be set during
 * the bounce. No state by default.
 * @param {number} [options.statusCode] - HTTP status code to use for server
 * side redirect. Only applies to bounceType == "server".
 * @param {number} [options.redirectDelayMS] - How long to wait before
 * redirecting. Only applies to bounceType == "client".
 * @returns {URL} Generated URL which points to an endpoint performing the
 * redirect.
 */
function getBounceURL({
  bounceType,
  bounceOrigin = ORIGIN_TRACKER,
  targetURL = new URL(getBaseUrl(ORIGIN_B) + "file_start.html"),
  setState = null,
  statusCode = 302,
  redirectDelayMS = 50,
}) {
  if (!["server", "client"].includes(bounceType)) {
    throw new Error("Invalid bounceType");
  }

  let bounceFile =
    bounceType == "client" ? "file_bounce.html" : "file_bounce.sjs";

  let bounceUrl = new URL(getBaseUrl(bounceOrigin) + bounceFile);

  let { searchParams } = bounceUrl;
  searchParams.set("target", targetURL.href);
  if (setState) {
    searchParams.set("setState", setState);
  }

  if (bounceType == "server") {
    searchParams.set("statusCode", statusCode);
  } else if (bounceType == "client") {
    searchParams.set("redirectDelay", redirectDelayMS);
  }

  return bounceUrl;
}

/**
 * Insert an <a href/> element with the given target and perform a synthesized
 * click on it.
 * @param {MozBrowser} browser - Browser to insert the link in.
 * @param {URL} targetURL - Destination for navigation.
 * @returns {Promise} Resolves once the click is done. Does not wait for
 * navigation or load.
 */
async function navigateLinkClick(browser, targetURL) {
  await SpecialPowers.spawn(browser, [targetURL.href], targetURL => {
    let link = content.document.createElement("a");

    link.href = targetURL;
    link.textContent = targetURL;
    // The link needs display: block, otherwise synthesizeMouseAtCenter doesn't
    // hit it.
    link.style.display = "block";

    content.document.body.appendChild(link);
  });

  await BrowserTestUtils.synthesizeMouseAtCenter("a[href]", {}, browser);
}

/**
 * Helper to wait for an observer message to be dispatched.
 * @param {string} message - Message to listen for.
 * @param {function} [conditionFn] - Optional condition function to filter
 * messages. Return false to keep listening for messages.
 * @returns {Promise} A promise which resolves once the given message has been
 * observed.
 */
async function waitForObserverMessage(message, conditionFn = () => true) {
  return new Promise(resolve => {
    let observer = (subject, topic, data) => {
      if (topic != message) {
        return;
      }

      if (conditionFn(subject, topic, data)) {
        Services.obs.removeObserver(observer, message);
        resolve();
      }
    };
    Services.obs.addObserver(observer, message);
  });
}

/**
 * Wait for the record-bounces method to run for the given tab / browser.
 * @param {browser} browser - Browser element which represents the tab we want
 * to observe.
 * @returns {Promise} Promise which resolves once the record-bounces method has
 * run for the given browser.
 */
async function waitForRecordBounces(browser) {
  return waitForObserverMessage(
    OBSERVER_MSG_RECORD_BOUNCES_FINISHED,
    subject => {
      // Ensure the message was dispatched for the browser we're interested in.
      let propBag = subject.QueryInterface(Ci.nsIPropertyBag2);
      let browserId = propBag.getProperty("browserId");
      return browser.browsingContext.browserId == browserId;
    }
  );
}

/**
 * Test helper which loads an initial blank page, then navigates to a url which
 * performs a bounce. Checks that the bounce hosts are properly identified as
 * trackers.
 * @param {object} options - Test Options.
 * @param {('server'|'client')} options.bounceType - Whether to perform a client
 * or server side redirect.
 * @param {('cookie-server'|'cookie-client'|'localStorage')} [options.setState]
 * Type of state to set during the redirect. Defaults to non stateful redirect.
 * @param {boolean} [options.expectCandidate=true] - Expect the redirecting site to be
 * identified as a bounce tracker (candidate).
 * @param {boolean} [options.expectPurge=true] - Expect the redirecting site to have
 * its storage purged.
 */
async function runTestBounce(options = {}) {
  let {
    bounceType,
    setState = null,
    expectCandidate = true,
    expectPurge = true,
  } = options;
  info(`runTestBounce ${JSON.stringify(options)}`);

  Assert.equal(
    bounceTrackingProtection.bounceTrackerCandidateHosts.length,
    0,
    "No bounce tracker hosts initially."
  );
  Assert.equal(
    bounceTrackingProtection.userActivationHosts.length,
    0,
    "No user activation hosts initially."
  );

  await BrowserTestUtils.withNewTab(
    getBaseUrl(ORIGIN_A) + "file_start.html",
    async browser => {
      let promiseRecordBounces = waitForRecordBounces(browser);

      // The final destination after the bounce.
      let targetURL = new URL(getBaseUrl(ORIGIN_B) + "file_start.html");

      // Navigate through the bounce chain.
      await navigateLinkClick(
        browser,
        getBounceURL({ bounceType, targetURL, setState })
      );

      // Wait for the final site to be loaded which complete the BounceTrackingRecord.
      await BrowserTestUtils.browserLoaded(browser, false, targetURL);

      // Navigate again with user gesture which triggers
      // BounceTrackingProtection::RecordStatefulBounces. We could rely on the
      // timeout (mClientBounceDetectionTimeout) here but that can cause races
      // in debug where the load is quite slow.
      await navigateLinkClick(
        browser,
        new URL(getBaseUrl(ORIGIN_C) + "file_start.html")
      );

      await promiseRecordBounces;

      Assert.deepEqual(
        bounceTrackingProtection.bounceTrackerCandidateHosts,
        expectCandidate ? [SITE_TRACKER] : [],
        `Should ${
          expectCandidate ? "" : "not "
        }have identified ${SITE_TRACKER} as a bounce tracker.`
      );
      Assert.deepEqual(
        bounceTrackingProtection.userActivationHosts.sort(),
        [SITE_A, SITE_B].sort(),
        "Should only have user activation for sites where we clicked links."
      );

      Assert.deepEqual(
        await bounceTrackingProtection.testRunPurgeBounceTrackers(),
        expectPurge ? [SITE_TRACKER] : [],
        `Should ${expectPurge ? "" : "not "}purge state for ${SITE_TRACKER}.`
      );

      bounceTrackingProtection.reset();
    }
  );
}
