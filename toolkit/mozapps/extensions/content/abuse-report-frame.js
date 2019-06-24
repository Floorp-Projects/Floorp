"use strict";

/* globals MozXULElement, Services, useHtmlViews, getHtmlBrowser, htmlBrowserLoaded */

{
  const ABUSE_REPORT_ENABLED = Services.prefs.getBoolPref("extensions.abuseReport.enabled", false);
  const ABUSE_REPORT_FRAME_URL = "chrome://mozapps/content/extensions/abuse-report-frame.html";
  const fm = Services.focus;
  const {AbuseReporter} = ChromeUtils.import("resource://gre/modules/AbuseReporter.jsm");

  class AddonAbuseReportsXULFrame extends MozXULElement {
    constructor() {
      super();
      this.report = null;
      // Keep track if the loadURI has already been called on the
      // browser element.
      this.browserLoadURI = false;
    }

    connectedCallback() {
      this.textContent = "";

      const content = MozXULElement.parseXULToFragment(`
        <browser id="abuse-report-xulframe-overlay-inner"
          type="content"
          disablehistory="true"
          transparent="true"
          flex="1">
        </browser>
      `);

      this.appendChild(content);

      const browser = this.querySelector("browser");
      this.promiseBrowserLoaded = new Promise(resolve => {
        browser.addEventListener("load", () => resolve(browser), {once: true});
      });

      document.addEventListener("focus", this);

      this.promiseHtmlAboutAddons.then(win => {
        win.document.addEventListener("abuse-report:new", this);
      });

      this.update();
    }

    disconnectedCallback() {
      this.textContent = "";
      this.browserLoadURI = false;
      this.promiseBrowserLoaded = null;
      this.report = null;
      document.removeEventListener("focus", this);

      this.promiseHtmlAboutAddons.then(win => {
        win.document.removeEventListener("abuse-report:new", this);
      });
    }

    handleEvent(evt) {
      // The "abuse-report:new" events are dispatched from the html about:addons sub-frame
      // (on the html about:addons document).
      // "abuse-report:cancel", "abuse-report:submit" and "abuse-report:updated" are
      // all dispatched from the AbuseReport webcomponent (on the AbuseReport element itself).
      // All the "abuse-report:*" events are also forwarded (dispatched on the frame
      // DOM element itself) to make it easier for the tests to wait for certain conditions
      // to be reached.
      switch (evt.type) {
        case "focus":
          this.focus();
          break;
        case "abuse-report:new":
          this.openReport(evt.detail);
          this.forwardEvent(evt);
          break;
        case "abuse-report:cancel":
          this.cancelReport();
          this.forwardEvent(evt);
          break;
        case "abuse-report:submit":
          this.onSubmitReport(evt);
          this.forwardEvent(evt);
          break;
        case "abuse-report:updated":
          this.forwardEvent(evt);
          break;
      }
    }

    forwardEvent(evt) {
      this.dispatchEvent(new CustomEvent(evt.type, {detail: evt.detail}));
    }

    async openReport({addonId, reportEntryPoint}) {
      if (this.report) {
        throw new Error("Ignoring new abuse report request. AbuseReport panel already open");
      } else {
        try {
          this.report = await AbuseReporter.createAbuseReport(addonId, {reportEntryPoint});
          this.update();
        } catch (err) {
          // Log the complete error in the console.
          console.error("Error creating abuse report for", addonId, err);
          // The report has an error on creation, and so instead of opening the report
          // panel an error message-bar is created on the HTML about:addons page.
          const win = await this.promiseHtmlAboutAddons;
          win.document.dispatchEvent(
            new CustomEvent("abuse-report:create-error", {detail: {
              addonId, addon: err.addon, errorType: err.errorType,
            }}));
        }
      }
    }

    cancelReport() {
      if (this.report) {
        this.report.abort();
        this.report = null;
        this.update();
      }
    }

    async onSubmitReport(evt) {
      if (this.report) {
        this.report = null;
        this.update();
        const win = await this.promiseHtmlAboutAddons;
        win.document.dispatchEvent(evt);
      }
    }

    focus() {
      // Trap the focus in the abuse report modal while it is enabled.
      if (this.hasAddonId) {
        this.promiseAbuseReport.then(abuseReport => {
          abuseReport.focus();
        });
      }
    }

    async update() {
      const {report} = this;
      if (report && report.addon && !report.errorType) {
        const {addon, reportEntryPoint} = this.report;
        this.addonId = addon.id;
        this.reportEntryPoint = reportEntryPoint;

        // Set the addon id on the addon-abuse-report webcomponent instance
        // embedded in the XUL browser.
        this.promiseAbuseReport.then(abuseReport => {
          this.hidden = false;
          abuseReport.addEventListener("abuse-report:updated", this, {once: true});
          abuseReport.addEventListener("abuse-report:submit", this, {once: true});
          abuseReport.addEventListener("abuse-report:cancel", this, {once: true});
          abuseReport.setAbuseReport(report);
          // Hide the content of the underlying about:addons page from
          // screen readers.
          this.aboutAddonsContent.setAttribute("aria-hidden", true);
          // Move the focus to the embedded window.
          this.focus();
          this.dispatchEvent(new CustomEvent("abuse-report:frame-shown"));
        });
      } else {
        this.hidden = true;
        this.removeAttribute("addon-id");
        this.removeAttribute("report-entry-point");

        // Make the content of the underlying about:addons page visible
        // to screen readers.
        this.aboutAddonsContent.setAttribute("aria-hidden", false);

        // Move the focus back to the top level window.
        fm.moveFocus(window, null, fm.MOVEFOCUS_ROOT, fm.FLAG_BYKEY);
        this.promiseAbuseReport.then(abuseReport => {
          abuseReport.removeEventListener("abuse-report:updated", this, {once: true});
          abuseReport.removeEventListener("abuse-report:submit", this, {once: true});
          abuseReport.removeEventListener("abuse-report:cancel", this, {once: true});
          abuseReport.setAbuseReport(null);
        }, err => {
          console.error("promiseAbuseReport rejected", err);
        }).then(() => {
          this.dispatchEvent(new CustomEvent("abuse-report:frame-hidden"));
        });
      }
    }

    get aboutAddonsContent() {
      return document.getElementById("main-page-content");
    }

    get promiseAbuseReport() {
      if (!this.browserLoadURI) {
        const browser = this.querySelector("browser");
        browser.loadURI(ABUSE_REPORT_FRAME_URL, {
          triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
        });
        this.browserLoadURI = true;
      }
      return this.promiseBrowserLoaded.then(browser => {
        return browser.contentDocument.querySelector("addon-abuse-report");
      });
    }

    get promiseHtmlAboutAddons() {
      const browser = getHtmlBrowser();
      return htmlBrowserLoaded.then(() => {
        return browser.contentWindow;
      });
    }

    get hasAddonId() {
      return !!this.addonId;
    }

    get addonId() {
      return this.getAttribute("addon-id");
    }

    set addonId(value) {
      this.setAttribute("addon-id", value);
    }

    get reportEntryPoint() {
      return this.getAttribute("report-entry-point");
    }

    set reportEntryPoint(value) {
      this.setAttribute("report-entry-point", value);
    }
  }

  // If the html about:addons and the abuse report are both enabled, register
  // the custom XUL WebComponent and append it to the XUL stack element
  // (if not registered the element will be just a dummy hidden box)
  if (useHtmlViews && ABUSE_REPORT_ENABLED) {
    customElements.define("addon-abuse-report-xulframe", AddonAbuseReportsXULFrame);
  }

  // Helper method exported into the about:addons global, used to open the
  // abuse report panel from outside of the about:addons page
  // (e.g. triggered from the browserAction context menu).
  window.openAbuseReport = ({addonId, reportEntryPoint}) => {
    const frame = document.querySelector("addon-abuse-report-xulframe");
    frame.openReport({addonId, reportEntryPoint});
  };
}
