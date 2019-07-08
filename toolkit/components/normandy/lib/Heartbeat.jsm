/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TelemetryController } = ChromeUtils.import(
  "resource://gre/modules/TelemetryController.jsm"
);
const { clearTimeout, setTimeout } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);
const { CleanupManager } = ChromeUtils.import(
  "resource://normandy/lib/CleanupManager.jsm"
);
const { EventEmitter } = ChromeUtils.import(
  "resource://normandy/lib/EventEmitter.jsm"
);
const { LogManager } = ChromeUtils.import(
  "resource://normandy/lib/LogManager.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]); /* globals URL */

var EXPORTED_SYMBOLS = ["Heartbeat"];

const PREF_SURVEY_DURATION = "browser.uitour.surveyDuration";
const NOTIFICATION_TIME = 3000;
const HEARTBEAT_CSS_URI = Services.io.newURI(
  "resource://normandy/skin/shared/Heartbeat.css"
);
const HEARTBEAT_CSS_URI_OSX = Services.io.newURI(
  "resource://normandy/skin/osx/Heartbeat.css"
);

const log = LogManager.getLogger("heartbeat");
const windowsWithInjectedCss = new WeakSet();
let anyWindowsWithInjectedCss = false;

// Add cleanup handler for CSS injected into windows by Heartbeat
CleanupManager.addCleanupHandler(() => {
  if (anyWindowsWithInjectedCss) {
    for (let window of Services.wm.getEnumerator("navigator:browser")) {
      if (windowsWithInjectedCss.has(window)) {
        const utils = window.windowUtils;
        utils.removeSheet(HEARTBEAT_CSS_URI, window.AGENT_SHEET);
        if (AppConstants.platform === "macosx") {
          utils.removeSheet(HEARTBEAT_CSS_URI_OSX, window.AGENT_SHEET);
        }
        windowsWithInjectedCss.delete(window);
      }
    }
  }
});

/**
 * Show the Heartbeat UI to request user feedback.
 *
 * @param chromeWindow
 *        The chrome window that the heartbeat notification is displayed in.
 * @param {Object} options Options object.
 * @param {String} options.message
 *        The message, or question, to display on the notification.
 * @param {String} options.thanksMessage
 *        The thank you message to display after user votes.
 * @param {String} options.flowId
 *        An identifier for this rating flow. Please note that this is only used to
 *        identify the notification box.
 * @param {String} [options.engagementButtonLabel=null]
 *        The text of the engagement button to use instead of stars. If this is null
 *        or invalid, rating stars are used.
 * @param {String} [options.learnMoreMessage=null]
 *        The label of the learn more link. No link will be shown if this is null.
 * @param {String} [options.learnMoreUrl=null]
 *        The learn more URL to open when clicking on the learn more link. No learn more
 *        will be shown if this is an invalid URL.
 * @param {String} [options.surveyId]
 *        An ID for the survey, reflected in the Telemetry ping.
 * @param {Number} [options.surveyVersion]
 *        Survey's version number, reflected in the Telemetry ping.
 * @param {boolean} [options.testing]
 *        Whether this is a test survey, reflected in the Telemetry ping.
 * @param {String} [options.postAnswerURL=null]
 *        The url to visit after the user answers the question.
 */
var Heartbeat = class {
  constructor(chromeWindow, options) {
    if (typeof options.flowId !== "string") {
      throw new Error(
        `flowId must be a string, but got ${JSON.stringify(
          options.flowId
        )}, a ${typeof options.flowId}`
      );
    }

    if (!options.flowId) {
      throw new Error("flowId must not be an empty string");
    }

    if (typeof options.message !== "string") {
      throw new Error(
        `message must be a string, but got ${JSON.stringify(
          options.message
        )}, a ${typeof options.message}`
      );
    }

    if (!options.message) {
      throw new Error("message must not be an empty string");
    }

    if (options.postAnswerUrl) {
      options.postAnswerUrl = new URL(options.postAnswerUrl);
    } else {
      options.postAnswerUrl = null;
    }

    if (options.learnMoreUrl) {
      try {
        options.learnMoreUrl = new URL(options.learnMoreUrl);
      } catch (e) {
        options.learnMoreUrl = null;
      }
    }

    this.chromeWindow = chromeWindow;
    this.eventEmitter = new EventEmitter();
    this.options = options;
    this.surveyResults = {};
    this.buttons = null;

    if (!windowsWithInjectedCss.has(chromeWindow)) {
      windowsWithInjectedCss.add(chromeWindow);
      const utils = chromeWindow.windowUtils;
      utils.loadSheet(HEARTBEAT_CSS_URI, chromeWindow.AGENT_SHEET);
      if (AppConstants.platform === "macosx") {
        utils.loadSheet(HEARTBEAT_CSS_URI_OSX, chromeWindow.AGENT_SHEET);
      }
      anyWindowsWithInjectedCss = true;
    }

    // so event handlers are consistent
    this.handleWindowClosed = this.handleWindowClosed.bind(this);
    this.close = this.close.bind(this);

    if (this.options.engagementButtonLabel) {
      this.buttons = [
        {
          label: this.options.engagementButtonLabel,
          callback: () => {
            // Let the consumer know user engaged.
            this.maybeNotifyHeartbeat("Engaged");

            this.userEngaged({
              type: "button",
              flowId: this.options.flowId,
            });

            // Return true so that the notification bar doesn't close itself since
            // we have a thank you message to show.
            return true;
          },
        },
      ];
    }

    this.notificationBox = this.chromeWindow.gHighPriorityNotificationBox;
    this.notice = this.notificationBox.appendNotification(
      this.options.message,
      "heartbeat-" + this.options.flowId,
      "resource://normandy/skin/shared/heartbeat-icon.svg",
      this.notificationBox.PRIORITY_INFO_HIGH,
      this.buttons,
      eventType => {
        if (eventType !== "removed") {
          return;
        }
        this.maybeNotifyHeartbeat("NotificationClosed");
      }
    );

    // Holds the rating UI
    const frag = this.chromeWindow.document.createDocumentFragment();

    // Build the heartbeat stars
    if (!this.options.engagementButtonLabel) {
      const numStars = this.options.engagementButtonLabel ? 0 : 5;
      this.ratingContainer = this.chromeWindow.document.createXULElement(
        "hbox"
      );
      this.ratingContainer.id = "star-rating-container";

      for (let i = 0; i < numStars; i++) {
        // create a star rating element
        const ratingElement = this.chromeWindow.document.createXULElement(
          "toolbarbutton"
        );

        // style it
        const starIndex = numStars - i;
        ratingElement.className = "plain star-x";
        ratingElement.id = "star" + starIndex;
        ratingElement.setAttribute("data-score", starIndex);

        // Add the click handler
        ratingElement.addEventListener("click", ev => {
          const rating = parseInt(ev.target.getAttribute("data-score"));
          this.maybeNotifyHeartbeat("Voted", { score: rating });
          this.userEngaged({
            type: "stars",
            score: rating,
            flowId: this.options.flowId,
          });
        });

        this.ratingContainer.appendChild(ratingElement);
      }

      frag.appendChild(this.ratingContainer);
    }

    this.notice.messageDetails.style.overflow = "hidden";
    this.notice.messageImage.classList.add("heartbeat", "pulse-onshow");
    this.notice.messageText.classList.add("heartbeat");

    // Make sure the stars are not pushed to the right by the spacer.
    this.rightSpacer = this.chromeWindow.document.createXULElement("spacer");
    this.rightSpacer.flex = 20;
    frag.appendChild(this.rightSpacer);

    // collapse the space before the stars
    this.notice.messageText.flex = 0;
    this.notice.spacer.flex = 0;

    // Add Learn More Link
    if (this.options.learnMoreMessage && this.options.learnMoreUrl) {
      this.learnMore = this.chromeWindow.document.createXULElement("label", {
        is: "text-link",
      });
      this.learnMore.href = this.options.learnMoreUrl.toString();
      this.learnMore.setAttribute("value", this.options.learnMoreMessage);
      this.learnMore.addEventListener("click", () =>
        this.maybeNotifyHeartbeat("LearnMore")
      );
      frag.appendChild(this.learnMore);
    }

    // Append the fragment and apply the styling
    this.notice.messageDetails.appendChild(frag);
    this.notice.classList.add("heartbeat");

    // Let the consumer know the notification was shown.
    this.maybeNotifyHeartbeat("NotificationOffered");
    this.chromeWindow.addEventListener(
      "SSWindowClosing",
      this.handleWindowClosed
    );

    const surveyDuration = Preferences.get(PREF_SURVEY_DURATION, 300) * 1000;
    this.surveyEndTimer = setTimeout(() => {
      this.maybeNotifyHeartbeat("SurveyExpired");
      this.close();
    }, surveyDuration);

    CleanupManager.addCleanupHandler(this.close);
  }

  maybeNotifyHeartbeat(name, data = {}) {
    if (this.pingSent) {
      log.warn(
        "Heartbeat event received after Telemetry ping sent. name:",
        name,
        "data:",
        data
      );
      return;
    }

    const timestamp = Date.now();
    let sendPing = false;
    let cleanup = false;

    const phases = {
      NotificationOffered: () => {
        this.surveyResults.flowId = this.options.flowId;
        this.surveyResults.offeredTS = timestamp;
      },
      LearnMore: () => {
        if (!this.surveyResults.learnMoreTS) {
          this.surveyResults.learnMoreTS = timestamp;
        }
      },
      Engaged: () => {
        this.surveyResults.engagedTS = timestamp;
      },
      Voted: () => {
        this.surveyResults.votedTS = timestamp;
        this.surveyResults.score = data.score;
      },
      SurveyExpired: () => {
        this.surveyResults.expiredTS = timestamp;
      },
      NotificationClosed: () => {
        this.surveyResults.closedTS = timestamp;
        cleanup = true;
        sendPing = true;
      },
      WindowClosed: () => {
        this.surveyResults.windowClosedTS = timestamp;
        cleanup = true;
        sendPing = true;
      },
      default: () => {
        log.error("Unrecognized Heartbeat event:", name);
      },
    };

    (phases[name] || phases.default)();

    data.timestamp = timestamp;
    data.flowId = this.options.flowId;
    this.eventEmitter.emit(name, data);

    if (sendPing) {
      // Send the ping to Telemetry
      const payload = Object.assign({ version: 1 }, this.surveyResults);
      for (const meta of ["surveyId", "surveyVersion", "testing"]) {
        if (this.options.hasOwnProperty(meta)) {
          payload[meta] = this.options[meta];
        }
      }

      log.debug("Sending telemetry");
      TelemetryController.submitExternalPing("heartbeat", payload, {
        addClientId: true,
        addEnvironment: true,
      });

      // only for testing
      this.eventEmitter.emit("TelemetrySent", payload);

      // Survey is complete, clear out the expiry timer & survey configuration
      this.endTimerIfPresent("surveyEndTimer");

      this.pingSent = true;
      this.surveyResults = null;
    }

    if (cleanup) {
      this.cleanup();
    }
  }

  userEngaged(engagementParams) {
    // Make the heartbeat icon pulse twice
    this.notice.label = this.options.thanksMessage;
    this.notice.messageImage.classList.remove("pulse-onshow");
    this.notice.messageImage.classList.add("pulse-twice");

    // Remove the custom contents of the notice and the buttons
    if (this.ratingContainer) {
      this.ratingContainer.remove();
    }
    this.rightSpacer.remove();
    this.learnMore.remove();
    for (let button of this.notice.querySelectorAll("button")) {
      button.remove();
    }

    // Open the engagement tab if we have a valid engagement URL.
    if (this.options.postAnswerUrl) {
      for (const key in engagementParams) {
        this.options.postAnswerUrl.searchParams.append(
          key,
          engagementParams[key]
        );
      }
      // Open the engagement URL in a new tab.
      let { gBrowser } = this.chromeWindow;
      gBrowser.selectedTab = gBrowser.addWebTab(
        this.options.postAnswerUrl.toString(),
        {
          triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
            {}
          ),
        }
      );
    }

    this.endTimerIfPresent("surveyEndTimer");

    this.engagementCloseTimer = setTimeout(
      () => this.close(),
      NOTIFICATION_TIME
    );
  }

  endTimerIfPresent(timerName) {
    if (this[timerName]) {
      clearTimeout(this[timerName]);
      this[timerName] = null;
    }
  }

  handleWindowClosed() {
    this.maybeNotifyHeartbeat("WindowClosed");
  }

  close() {
    this.notificationBox.removeNotification(this.notice);
  }

  cleanup() {
    // Kill the timers which might call things after we've cleaned up:
    this.endTimerIfPresent("surveyEndTimer");
    this.endTimerIfPresent("engagementCloseTimer");
    // remove listeners
    this.chromeWindow.removeEventListener(
      "SSWindowClosing",
      this.handleWindowClosed
    );
    // remove references for garbage collection
    this.chromeWindow = null;
    this.notificationBox = null;
    this.notice = null;
    this.ratingContainer = null;
    this.rightSpacer = null;
    this.learnMore = null;
    this.eventEmitter = null;
    // Ensure we don't re-enter and release the CleanupManager's reference to us:
    CleanupManager.removeCleanupHandler(this.close);
  }
};
