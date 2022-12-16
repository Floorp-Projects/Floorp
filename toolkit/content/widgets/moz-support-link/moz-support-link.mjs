/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

MozXULElement.insertFTLIfNeeded("browser/components/mozSupportLink.ftl");

export default class MozSupportLink extends HTMLAnchorElement {
  static SUPPORT_URL = "https://www.mozilla.org/";
  static get observedAttributes() {
    return ["support-page", "utm-content", "data-l10n-id"];
  }

  /**
   * Handles setting up the SUPPORT_URL preference getter.
   * Without this, the tests for this component may not behave
   * as expected.
   * @private
   * @memberof MozSupportLink
   */
  #register() {
    if (!window.IS_STORYBOOK) {
      let { XPCOMUtils } = window.XPCOMUtils
        ? window
        : ChromeUtils.importESModule(
            "resource://gre/modules/XPCOMUtils.sys.mjs"
          );
      XPCOMUtils.defineLazyPreferenceGetter(
        MozSupportLink,
        "SUPPORT_URL",
        "app.support.baseURL",
        "",
        null,
        val => Services.urlFormatter.formatURL(val)
      );
    }
  }

  connectedCallback() {
    this.#register();
    this.#setHref();
    this.setAttribute("target", "_blank");
    if (!this.getAttribute("data-l10n-id")) {
      document.l10n.setAttributes(this, "moz-support-link-text");
      document.l10n.translateFragment(this);
    }
  }

  attributeChangedCallback(name, oldVal, newVal) {
    if (name === "support-page" || name === "utm-content") {
      this.#setHref();
    }
  }

  #setHref() {
    let supportPage = this.getAttribute("support-page") ?? "";
    let base = MozSupportLink.SUPPORT_URL + supportPage;
    this.href = this.hasAttribute("utm-content")
      ? formatUTMParams(this.getAttribute("utm-content"), base)
      : base;
  }
}
customElements.define("moz-support-link", MozSupportLink, { extends: "a" });

/**
 * Adds UTM parameters to a given URL, if it is an AMO URL.
 *
 * @param {string} contentAttribute
 *        Identifies the part of the UI with which the link is associated.
 * @param {string} url
 * @returns {string}
 *          The url with UTM parameters if it is an AMO URL.
 *          Otherwise the url in unmodified form.
 */
export function formatUTMParams(contentAttribute, url) {
  if (!contentAttribute) {
    return url;
  }
  let parsedUrl = new URL(url);
  let domain = `.${parsedUrl.hostname}`;
  if (
    !domain.endsWith(".mozilla.org") &&
    // For testing: addons-dev.allizom.org and addons.allizom.org
    !domain.endsWith(".allizom.org")
  ) {
    return url;
  }

  parsedUrl.searchParams.set("utm_source", "firefox-browser");
  parsedUrl.searchParams.set("utm_medium", "firefox-browser");
  parsedUrl.searchParams.set("utm_content", contentAttribute);
  return parsedUrl.href;
}
