/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

class MozTextLink extends MozElements.MozElementMixin(XULTextElement) {
  constructor() {
    super();

    this.addEventListener("click", (event) => {
      if (event.button == 0 || event.button == 1) {
        this.open(event);
      }
    }, true);

    this.addEventListener("keypress", (event) => {
      if (event.keyCode != KeyEvent.DOM_VK_RETURN) {
        return;
      }
      this.click();
    });
  }

  connectedCallback() {
    this.classList.add("text-link");
  }

  set href(val) {
    this.setAttribute("href", val);
    return val;
  }

  get href() {
    return this.getAttribute("href");
  }

  open(aEvent) {
    var href = this.href;
    if (!href || this.disabled || aEvent.defaultPrevented)
      return;

    var uri = null;
    try {
      const nsISSM = Ci.nsIScriptSecurityManager;
      const secMan =
        Cc["@mozilla.org/scriptsecuritymanager;1"]
        .getService(nsISSM);

      uri = Services.io.newURI(href);

      let principal;
      if (this.getAttribute("useoriginprincipal") == "true") {
        principal = this.nodePrincipal;
      } else {
        principal = secMan.createNullPrincipal({});
      }
      try {
        secMan.checkLoadURIWithPrincipal(principal, uri,
          nsISSM.DISALLOW_INHERIT_PRINCIPAL);
      } catch (ex) {
        var msg = "Error: Cannot open a " + uri.scheme + ": link using \
                         the text-link binding.";
        Cu.reportError(msg);
        return;
      }

      const cID = "@mozilla.org/uriloader/external-protocol-service;1";
      const nsIEPS = Ci.nsIExternalProtocolService;
      var protocolSvc = Cc[cID].getService(nsIEPS);

      // if the scheme is not an exposed protocol, then opening this link
      // should be deferred to the system's external protocol handler
      if (!protocolSvc.isExposedProtocol(uri.scheme)) {
        protocolSvc.loadURI(uri);
        aEvent.preventDefault();
        return;
      }
    } catch (ex) {
      Cu.reportError(ex);
    }

    aEvent.preventDefault();
    href = uri ? uri.spec : href;

    // Try handing off the link to the host application, e.g. for
    // opening it in a tabbed browser.
    var linkHandled = Cc["@mozilla.org/supports-PRBool;1"]
      .createInstance(Ci.nsISupportsPRBool);
    linkHandled.data = false;
    let { shiftKey, ctrlKey, metaKey, altKey, button } = aEvent;
    let data = { shiftKey, ctrlKey, metaKey, altKey, button, href };
    Services.obs.notifyObservers(linkHandled,
                                 "handle-xul-text-link",
                                 JSON.stringify(data));
    if (linkHandled.data)
      return;

    // otherwise, fall back to opening the anchor directly
    var win = window;
    if (window.isChromeWindow) {
      while (win.opener && !win.opener.closed)
        win = win.opener;
    }
    win.open(href);
  }
}

customElements.define("text-link", MozTextLink, { extends: "label" });
}
