/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AboutReader", "resource://gre/modules/AboutReader.jsm");
ChromeUtils.defineModuleGetter(this, "ReaderMode", "resource://gre/modules/ReaderMode.jsm");
ChromeUtils.defineModuleGetter(this, "LoginManagerContent", "resource://gre/modules/LoginManagerContent.jsm");

XPCOMUtils.defineLazyGetter(this, "gPipNSSBundle", function() {
  return Services.strings.createBundle("chrome://pipnss/locale/pipnss.properties");
});
XPCOMUtils.defineLazyGetter(this, "gNSSErrorsBundle", function() {
  return Services.strings.createBundle("chrome://pipnss/locale/nsserrors.properties");
});

var dump = ChromeUtils.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.d.bind(null, "Content");

var global = this;

var AboutBlockedSiteListener = {
  init(chromeGlobal) {
    addEventListener("AboutBlockedLoaded", this, false, true);
  },

  get isBlockedSite() {
    return content.document.documentURI.startsWith("about:blocked");
  },

  handleEvent(aEvent) {
    if (!this.isBlockedSite) {
      return;
    }

    if (aEvent.type != "AboutBlockedLoaded") {
      return;
    }

    let provider = "";
    if (docShell.failedChannel) {
      let classifiedChannel = docShell.failedChannel.
                              QueryInterface(Ci.nsIClassifiedChannel);
      if (classifiedChannel) {
        provider = classifiedChannel.matchedProvider;
      }
    }

    let advisoryUrl = Services.prefs.getCharPref(
      "browser.safebrowsing.provider." + provider + ".advisoryURL", "");
    if (!advisoryUrl) {
      let el = content.document.getElementById("advisoryDesc");
      el.remove();
      return;
    }

    let advisoryLinkText = Services.prefs.getCharPref(
      "browser.safebrowsing.provider." + provider + ".advisoryName", "");
    if (!advisoryLinkText) {
      let el = content.document.getElementById("advisoryDesc");
      el.remove();
      return;
    }

    let anchorEl = content.document.getElementById("advisory_provider");
    anchorEl.setAttribute("href", advisoryUrl);
    anchorEl.textContent = advisoryLinkText;
  },
};
AboutBlockedSiteListener.init();

/* The following code, in particular AboutCertErrorListener and
 * AboutNetErrorListener, is mostly copied from content browser.js and content.js.
 * Certificate error handling should be unified to remove this duplicated code.
 */

const SEC_ERROR_BASE          = Ci.nsINSSErrorsService.NSS_SEC_ERROR_BASE;
const MOZILLA_PKIX_ERROR_BASE = Ci.nsINSSErrorsService.MOZILLA_PKIX_ERROR_BASE;

const SEC_ERROR_EXPIRED_CERTIFICATE                = SEC_ERROR_BASE + 11;
const SEC_ERROR_UNKNOWN_ISSUER                     = SEC_ERROR_BASE + 13;
const SEC_ERROR_UNTRUSTED_ISSUER                   = SEC_ERROR_BASE + 20;
const SEC_ERROR_UNTRUSTED_CERT                     = SEC_ERROR_BASE + 21;
const SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE         = SEC_ERROR_BASE + 30;
const SEC_ERROR_CA_CERT_INVALID                    = SEC_ERROR_BASE + 36;
const SEC_ERROR_OCSP_FUTURE_RESPONSE               = SEC_ERROR_BASE + 131;
const SEC_ERROR_OCSP_OLD_RESPONSE                  = SEC_ERROR_BASE + 132;
const SEC_ERROR_REUSED_ISSUER_AND_SERIAL           = SEC_ERROR_BASE + 138;
const SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED  = SEC_ERROR_BASE + 176;
const MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE = MOZILLA_PKIX_ERROR_BASE + 5;
const MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE = MOZILLA_PKIX_ERROR_BASE + 6;


const SSL_ERROR_BASE = Ci.nsINSSErrorsService.NSS_SSL_ERROR_BASE;
const SSL_ERROR_SSL_DISABLED  = SSL_ERROR_BASE + 20;
const SSL_ERROR_SSL2_DISABLED  = SSL_ERROR_BASE + 14;

var AboutNetErrorListener = {
  init(chromeGlobal) {
    addEventListener("AboutNetErrorLoad", this, false, true);
  },

  get isNetErrorSite() {
    return content.document.documentURI.startsWith("about:neterror");
  },

  _getErrorMessageFromCode(securityInfo, doc) {
    let uri = Services.io.newURI(doc.location);
    let hostString = uri.host;
    if (uri.port != 443 && uri.port != -1) {
      hostString += ":" + uri.port;
    }

    let id_str = "";
    switch (securityInfo.errorCode) {
      case SSL_ERROR_SSL_DISABLED:
        id_str = "PSMERR_SSL_Disabled";
        break;
      case SSL_ERROR_SSL2_DISABLED:
        id_str = "PSMERR_SSL2_Disabled";
        break;
      case SEC_ERROR_REUSED_ISSUER_AND_SERIAL:
        id_str = "PSMERR_HostReusedIssuerSerial";
        break;
    }
    let nss_error_id_str = securityInfo.errorCodeString;
    let msg2 = "";
    if (id_str) {
      msg2 = gPipNSSBundle.GetStringFromName(id_str) + "\n";
    } else if (nss_error_id_str) {
      msg2 = gNSSErrorsBundle.GetStringFromName(nss_error_id_str) + "\n";
    }

    if (!msg2) {
      // We couldn't get an error message. Use the error string.
      // Note that this is different from before where we used PR_ErrorToString.
      msg2 = nss_error_id_str;
    }
    let msg = gPipNSSBundle.formatStringFromName("SSLConnectionErrorPrefix2",
                                                 [hostString, msg2], 2);

    if (nss_error_id_str) {
      msg += gPipNSSBundle.formatStringFromName("certErrorCodePrefix3",
                                                [nss_error_id_str], 1);
    }
    return msg;
  },

  handleEvent(aEvent) {
    if (!this.isNetErrorSite) {
      return;
    }

    if (aEvent.type != "AboutNetErrorLoad") {
      return;
    }

    let {securityInfo} = docShell.failedChannel;
    // We don't have a securityInfo when this is for example a DNS error.
    if (securityInfo) {
      securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
      let msg = this._getErrorMessageFromCode(securityInfo,
                                              aEvent.originalTarget.ownerGlobal);
      let id = content.document.getElementById("errorShortDescText");
      id.textContent = msg;
    }
  },
};
AboutNetErrorListener.init();

var AboutCertErrorListener = {
  init(chromeGlobal) {
    addEventListener("AboutCertErrorLoad", this, false, true);
  },

  get isCertErrorSite() {
    return content.document.documentURI.startsWith("about:certerror");
  },

  _setTechDetailsMsgPart1(hostString, sslStatus, technicalInfo, doc) {
    let msg = gPipNSSBundle.formatStringFromName("certErrorIntro",
                                                 [hostString], 1);
    msg += "\n\n";

    if (sslStatus.isUntrusted && !sslStatus.serverCert.isSelfSigned) {
      switch (securityInfo.errorCode) {
        case SEC_ERROR_UNKNOWN_ISSUER:
          msg += gPipNSSBundle.GetStringFromName("certErrorTrust_UnknownIssuer") + "\n";
          msg += gPipNSSBundle.GetStringFromName("certErrorTrust_UnknownIssuer2") + "\n";
          msg += gPipNSSBundle.GetStringFromName("certErrorTrust_UnknownIssuer3") + "\n";
          break;
        case SEC_ERROR_CA_CERT_INVALID:
          msg += gPipNSSBundle.GetStringFromName("certErrorTrust_CaInvalid") + "\n";
          break;
        case SEC_ERROR_UNTRUSTED_ISSUER:
          msg += gPipNSSBundle.GetStringFromName("certErrorTrust_Issuer") + "\n";
          break;
        case SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED:
          msg += gPipNSSBundle.GetStringFromName("certErrorTrust_SignatureAlgorithmDisabled") + "\n";
          break;
        case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
          msg += gPipNSSBundle.GetStringFromName("certErrorTrust_ExpiredIssuer") + "\n";
          break;
        case SEC_ERROR_UNTRUSTED_CERT:
        default:
          msg += gPipNSSBundle.GetStringFromName("certErrorTrust_Untrusted") + "\n";
      }
    }
    if (sslStatus.isUntrusted && sslStatus.serverCert.isSelfSigned) {
      msg += gPipNSSBundle.GetStringFromName("certErrorTrust_SelfSigned") + "\n";
    }

    technicalInfo.appendChild(doc.createTextNode(msg));
  },

  _setTechDetails(sslStatus, securityInfo, location) {
    if (!securityInfo || !sslStatus || !location) {
      return;
    }
    let validity = sslStatus.serverCert.validity;

    let doc = content.document;
    // CSS class and error code are set from nsDocShell.
    let searchParams = new URLSearchParams(doc.documentURI.split("?")[1]);
    let cssClass = searchParams.get("s");
    let error = searchParams.get("e");
    let technicalInfo = doc.getElementById("technicalContentText");

    let uri = Services.io.newURI(location);
    let hostString = uri.host;
    if (uri.port != 443 && uri.port != -1) {
      hostString += ":" + uri.port;
    }

    this._setTechDetailsMsgPart1(hostString, sslStatus, technicalInfo, doc);

    if (sslStatus.isDomainMismatch) {
      let subjectAltNamesList = sslStatus.serverCert.subjectAltNames;
      let subjectAltNames = subjectAltNamesList.split(",");
      let numSubjectAltNames = subjectAltNames.length;
      let msgPrefix = "";
      if (numSubjectAltNames != 0) {
        if (numSubjectAltNames == 1) {
          msgPrefix = gPipNSSBundle.GetStringFromName("certErrorMismatchSinglePrefix");

          // Let's check if we want to make this a link.
          let okHost = subjectAltNamesList;
          let href = "";
          let thisHost = doc.location.hostname;
          let proto = doc.location.protocol + "//";
          // If okHost is a wildcard domain ("*.example.com") let's
          // use "www" instead.  "*.example.com" isn't going to
          // get anyone anywhere useful. bug 432491
          okHost = okHost.replace(/^\*\./, "www.");
          /* case #1:
           * example.com uses an invalid security certificate.
           *
           * The certificate is only valid for www.example.com
           *
           * Make sure to include the "." ahead of thisHost so that
           * a MitM attack on paypal.com doesn't hyperlink to "notpaypal.com"
           *
           * We'd normally just use a RegExp here except that we lack a
           * library function to escape them properly (bug 248062), and
           * domain names are famous for having '.' characters in them,
           * which would allow spurious and possibly hostile matches.
           */
          if (okHost.endsWith("." + thisHost)) {
            href = proto + okHost;
          }
          /* case #2:
           * browser.garage.maemo.org uses an invalid security certificate.
           *
           * The certificate is only valid for garage.maemo.org
           */
          if (thisHost.endsWith("." + okHost)) {
            href = proto + okHost;
          }

          // If we set a link, meaning there's something helpful for
          // the user here, expand the section by default
          if (href && cssClass != "expertBadCert") {
            doc.getElementById("technicalContentText").style.display = "block";
          }

          // Set the link if we want it.
          if (href) {
            let referrerlink = doc.createElement("a");
            referrerlink.append(subjectAltNamesList + "\n");
            referrerlink.title = subjectAltNamesList;
            referrerlink.id = "cert_domain_link";
            referrerlink.href = href;
            let fragment = BrowserUtils.getLocalizedFragment(doc, msgPrefix,
                                                             referrerlink);
            technicalInfo.appendChild(fragment);
          } else {
            let fragment = BrowserUtils.getLocalizedFragment(doc,
                                                             msgPrefix,
                                                             subjectAltNamesList);
            technicalInfo.appendChild(fragment);
          }
          technicalInfo.append("\n");
        } else {
          let msg = gPipNSSBundle.GetStringFromName("certErrorMismatchMultiple") + "\n";
          for (let i = 0; i < numSubjectAltNames; i++) {
            msg += subjectAltNames[i];
            if (i != (numSubjectAltNames - 1)) {
              msg += ", ";
            }
          }
          technicalInfo.append(msg + "\n");
        }
      } else {
        let msg = gPipNSSBundle.formatStringFromName("certErrorMismatch",
                                                    [hostString], 1);
        technicalInfo.append(msg + "\n");
      }
    }

    if (sslStatus.isNotValidAtThisTime) {
      let nowTime = new Date().getTime() * 1000;
      let dateOptions = { year: "numeric", month: "long", day: "numeric", hour: "numeric", minute: "numeric" };
      let now = new Services.intl.DateTimeFormat(undefined, dateOptions).format(new Date());
      let msg = "";
      if (validity.notBefore) {
        if (nowTime > validity.notAfter) {
          msg += gPipNSSBundle.formatStringFromName("certErrorExpiredNow",
                                                   [validity.notAfterLocalTime, now], 2) + "\n";
        } else {
          msg += gPipNSSBundle.formatStringFromName("certErrorNotYetValidNow",
                                                   [validity.notBeforeLocalTime, now], 2) + "\n";
        }
      } else {
        // If something goes wrong, we assume the cert expired.
        msg += gPipNSSBundle.formatStringFromName("certErrorExpiredNow",
                                                 ["", now], 2) + "\n";
      }
      technicalInfo.append(msg);
    }
    technicalInfo.append("\n");

    // Add link to certificate and error message.
    let errorCodeMsg = gPipNSSBundle.formatStringFromName("certErrorCodePrefix3",
                                                          [securityInfo.errorCodeString], 1);
    technicalInfo.append(errorCodeMsg);
  },

  handleEvent(aEvent) {
    if (!this.isCertErrorSite) {
      return;
    }

    if (aEvent.type != "AboutCertErrorLoad") {
      return;
    }

    let ownerDoc = aEvent.originalTarget.ownerGlobal;
    let securityInfo = docShell.failedChannel && docShell.failedChannel.securityInfo;
    securityInfo.QueryInterface(Ci.nsITransportSecurityInfo)
                .QueryInterface(Ci.nsISerializable);
    let sslStatus = securityInfo.QueryInterface(Ci.nsISSLStatusProvider)
                                                .SSLStatus;
    this._setTechDetails(sslStatus, securityInfo, ownerDoc.location.href);
  },
};
AboutCertErrorListener.init();

// This is copied from desktop's tab-content.js. See bug 1153485 about sharing this code somehow.
var AboutReaderListener = {

  _articlePromise: null,

  _isLeavingReaderableReaderMode: false,

  init: function() {
    addEventListener("AboutReaderContentLoaded", this, false, true);
    addEventListener("DOMContentLoaded", this, false);
    addEventListener("pageshow", this, false);
    addEventListener("pagehide", this, false);
    addMessageListener("Reader:ToggleReaderMode", this);
    addMessageListener("Reader:PushState", this);
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "Reader:ToggleReaderMode":
        let url = content.document.location.href;
        if (!this.isAboutReader) {
          this._articlePromise = ReaderMode.parseDocument(content.document).catch(Cu.reportError);
          ReaderMode.enterReaderMode(docShell, content);
        } else {
          this._isLeavingReaderableReaderMode = this.isReaderableAboutReader;
          ReaderMode.leaveReaderMode(docShell, content);
        }
        break;

      case "Reader:PushState":
        this.updateReaderButton(!!(message.data && message.data.isArticle));
        break;
    }
  },

  get isAboutReader() {
    return content.document.documentURI.startsWith("about:reader");
  },

  get isReaderableAboutReader() {
    return this.isAboutReader &&
      !content.document.documentElement.dataset.isError;
  },

  get isErrorPage() {
    return content.document.documentURI.startsWith("about:neterror") ||
        content.document.documentURI.startsWith("about:certerror") ||
        content.document.documentURI.startsWith("about:blocked");
  },

  handleEvent: function(aEvent) {
    if (aEvent.originalTarget.defaultView != content) {
      return;
    }

    switch (aEvent.type) {
      case "AboutReaderContentLoaded":
        if (!this.isAboutReader) {
          return;
        }

        // If we are restoring multiple reader mode tabs during session restore, duplicate "DOMContentLoaded"
        // events may be fired for the visible tab. The inital "DOMContentLoaded" may be received before the
        // document body is available, so we avoid instantiating an AboutReader object, expecting that a
        // valid message will follow. See bug 925983.
        if (content.document.body) {
          new AboutReader(global, content, this._articlePromise);
          this._articlePromise = null;
        }
        break;

      case "pagehide":
        // this._isLeavingReaderableReaderMode is used here to keep the Reader Mode icon
        // visible in the location bar when transitioning from reader-mode page
        // back to the source page.
        sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: this._isLeavingReaderableReaderMode });
        if (this._isLeavingReaderableReaderMode) {
          this._isLeavingReaderableReaderMode = false;
        }
        break;

      case "pageshow":
        // If a page is loaded from the bfcache, we won't get a "DOMContentLoaded"
        // event, so we need to rely on "pageshow" in this case.
        if (aEvent.persisted) {
          this.updateReaderButton();
        }
        break;
      case "DOMContentLoaded":
        this.updateReaderButton();
        break;
    }
  },
  updateReaderButton: function(forceNonArticle) {
    // Do not show Reader View icon on error pages (bug 1320900)
    if (this.isErrorPage) {
        sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: false });
    } else if (!ReaderMode.isEnabledForParseOnLoad || this.isAboutReader ||
        !(content.document instanceof content.HTMLDocument) ||
        content.document.mozSyntheticDocument) {

    } else {
        this.scheduleReadabilityCheckPostPaint(forceNonArticle);
    }
  },

  cancelPotentialPendingReadabilityCheck: function() {
    if (this._pendingReadabilityCheck) {
      removeEventListener("MozAfterPaint", this._pendingReadabilityCheck);
      delete this._pendingReadabilityCheck;
    }
  },

  scheduleReadabilityCheckPostPaint: function(forceNonArticle) {
    if (this._pendingReadabilityCheck) {
      // We need to stop this check before we re-add one because we don't know
      // if forceNonArticle was true or false last time.
      this.cancelPotentialPendingReadabilityCheck();
    }
    this._pendingReadabilityCheck = this.onPaintWhenWaitedFor.bind(this, forceNonArticle);
    addEventListener("MozAfterPaint", this._pendingReadabilityCheck);
  },

  onPaintWhenWaitedFor: function(forceNonArticle, event) {
    // In non-e10s, we'll get called for paints other than ours, and so it's
    // possible that this page hasn't been laid out yet, in which case we
    // should wait until we get an event that does relate to our layout. We
    // determine whether any of our content got painted by checking if there
    // are any painted rects.
    if (!event.clientRects.length) {
      return;
    }

    this.cancelPotentialPendingReadabilityCheck();

    // Only send updates when there are articles; there's no point updating with
    // |false| all the time.
    if (ReaderMode.isProbablyReaderable(content.document)) {
      sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: true });
    } else if (forceNonArticle) {
      sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: false });
    }
  },
};
AboutReaderListener.init();

addMessageListener("RemoteLogins:fillForm", function(message) {
  LoginManagerContent.receiveMessage(message, content);
});

Services.obs.notifyObservers(this, "tab-content-frameloader-created");
