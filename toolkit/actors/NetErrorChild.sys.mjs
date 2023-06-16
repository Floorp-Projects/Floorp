/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AppInfo: "chrome://remote/content/shared/AppInfo.sys.mjs",
});

import { RemotePageChild } from "resource://gre/actors/RemotePageChild.sys.mjs";

export class NetErrorChild extends RemotePageChild {
  actorCreated() {
    super.actorCreated();

    // If you add a new function, remember to add it to RemotePageAccessManager.sys.mjs
    // to allow content-privileged about:neterror or about:certerror to use it.
    const exportableFunctions = [
      "RPMGetAppBuildID",
      "RPMGetInnerMostURI",
      "RPMAddToHistogram",
      "RPMRecordTelemetryEvent",
      "RPMCheckAlternateHostAvailable",
      "RPMGetHttpResponseHeader",
      "RPMIsTRROnlyFailure",
      "RPMIsFirefox",
      "RPMIsNativeFallbackFailure",
      "RPMOpenPreferences",
      "RPMGetTRRSkipReason",
      "RPMGetTRRDomain",
      "RPMIsSiteSpecificTRRError",
      "RPMSetTRRDisabledLoadFlags",
      "RPMGetCurrentTRRMode",
    ];
    this.exportFunctions(exportableFunctions);
  }

  getFailedCertChain(docShell) {
    let securityInfo =
      docShell.failedChannel && docShell.failedChannel.securityInfo;
    if (!securityInfo) {
      return [];
    }
    return securityInfo.failedCertChain.map(cert => cert.getBase64DERString());
  }

  handleEvent(aEvent) {
    // Documents have a null ownerDocument.
    let doc = aEvent.originalTarget.ownerDocument || aEvent.originalTarget;

    switch (aEvent.type) {
      case "click":
        let elem = aEvent.originalTarget;
        if (elem.id == "viewCertificate") {
          // Call through the superclass to avoid the security check.
          this.sendAsyncMessage("Browser:CertExceptionError", {
            location: doc.location.href,
            elementId: elem.id,
            failedCertChain: this.getFailedCertChain(doc.defaultView.docShell),
          });
        }
        break;
    }
  }

  RPMGetInnerMostURI(uriString) {
    let uri = Services.io.newURI(uriString);
    if (uri instanceof Ci.nsINestedURI) {
      uri = uri.QueryInterface(Ci.nsINestedURI).innermostURI;
    }

    return uri.spec;
  }

  RPMGetAppBuildID() {
    return Services.appinfo.appBuildID;
  }

  RPMAddToHistogram(histID, bin) {
    Services.telemetry.getHistogramById(histID).add(bin);
  }

  RPMRecordTelemetryEvent(category, event, object, value, extra) {
    Services.telemetry.recordEvent(category, event, object, value, extra);
  }

  RPMCheckAlternateHostAvailable() {
    const host = this.contentWindow.location.host.trim();

    // Adapted from UrlbarUtils::looksLikeSingleWordHost
    // https://searchfox.org/mozilla-central/rev/a26af613a476fafe6c3eba05a81bef63dff3c9f1/browser/components/urlbar/UrlbarUtils.sys.mjs#893
    const REGEXP_SINGLE_WORD = /^[^\s@:/?#]+(:\d+)?$/;
    if (!REGEXP_SINGLE_WORD.test(host)) {
      return;
    }

    let info = Services.uriFixup.forceHttpFixup(
      this.contentWindow.location.href
    );

    if (!info.fixupCreatedAlternateURI && !info.fixupChangedProtocol) {
      return;
    }

    let { displayHost, displaySpec, pathQueryRef } = info.fixedURI;

    if (pathQueryRef.endsWith("/")) {
      pathQueryRef = pathQueryRef.slice(0, pathQueryRef.length - 1);
    }

    let weakDoc = Cu.getWeakReference(this.contentWindow.document);
    let onLookupCompleteListener = {
      onLookupComplete(request, record, status) {
        let doc = weakDoc.get();
        if (!doc || !Components.isSuccessCode(status)) {
          return;
        }

        let link = doc.createElement("a");
        link.href = displaySpec;
        link.setAttribute("data-l10n-name", "website");

        let span = doc.createElement("span");
        span.appendChild(link);
        doc.l10n.setAttributes(span, "neterror-dns-not-found-with-suggestion", {
          hostAndPath: displayHost + pathQueryRef,
        });

        const shortDesc = doc.getElementById("errorShortDesc");
        shortDesc.textContent += " ";
        shortDesc.appendChild(span);
      },
    };

    Services.uriFixup.checkHost(
      info.fixedURI,
      onLookupCompleteListener,
      this.document.nodePrincipal.originAttributes
    );
  }

  // Get the header from the http response of the failed channel. This function
  // is used in the 'about:neterror' page.
  RPMGetHttpResponseHeader(responseHeader) {
    let channel = this.contentWindow.docShell.failedChannel;
    if (!channel) {
      return "";
    }

    let httpChannel = channel.QueryInterface(Ci.nsIHttpChannel);
    if (!httpChannel) {
      return "";
    }

    try {
      return httpChannel.getResponseHeader(responseHeader);
    } catch (e) {}

    return "";
  }

  RPMIsTRROnlyFailure() {
    // We will only show this in Firefox because the options may direct users to settings only available on Firefox Desktop
    let channel = this.contentWindow?.docShell?.failedChannel?.QueryInterface(
      Ci.nsIHttpChannelInternal
    );
    if (!channel) {
      return false;
    }
    return channel.effectiveTRRMode == Ci.nsIRequest.TRR_ONLY_MODE;
  }

  RPMIsFirefox() {
    return lazy.AppInfo.isFirefox;
  }

  _getTRRSkipReason() {
    let channel = this.contentWindow?.docShell?.failedChannel?.QueryInterface(
      Ci.nsIHttpChannelInternal
    );
    return channel?.trrSkipReason ?? Ci.nsITRRSkipReason.TRR_UNSET;
  }

  RPMIsNativeFallbackFailure() {
    if (!this.contentWindow?.navigator.onLine) {
      return false;
    }

    let skipReason = this._getTRRSkipReason();

    if (
      Services.dns.currentTrrMode === Ci.nsIDNSService.MODE_TRRFIRST &&
      skipReason === Ci.nsITRRSkipReason.TRR_NOT_CONFIRMED
    ) {
      return true;
    }

    const warningReasons = new Set([
      Ci.nsITRRSkipReason.TRR_HEURISTIC_TRIPPED_GOOGLE_SAFESEARCH,
      Ci.nsITRRSkipReason.TRR_HEURISTIC_TRIPPED_YOUTUBE_SAFESEARCH,
      Ci.nsITRRSkipReason.TRR_HEURISTIC_TRIPPED_ZSCALER_CANARY,
      Ci.nsITRRSkipReason.TRR_HEURISTIC_TRIPPED_CANARY,
      Ci.nsITRRSkipReason.TRR_HEURISTIC_TRIPPED_MODIFIED_ROOTS,
      Ci.nsITRRSkipReason.TRR_HEURISTIC_TRIPPED_PARENTAL_CONTROLS,
      Ci.nsITRRSkipReason.TRR_HEURISTIC_TRIPPED_THIRD_PARTY_ROOTS,
      Ci.nsITRRSkipReason.TRR_HEURISTIC_TRIPPED_ENTERPRISE_POLICY,
      Ci.nsITRRSkipReason.TRR_HEURISTIC_TRIPPED_VPN,
      Ci.nsITRRSkipReason.TRR_HEURISTIC_TRIPPED_PROXY,
      Ci.nsITRRSkipReason.TRR_HEURISTIC_TRIPPED_NRPT,
    ]);

    return (
      Services.dns.currentTrrMode === Ci.nsIDNSService.MODE_NATIVEONLY &&
      warningReasons.has(skipReason)
    );
  }

  RPMGetTRRSkipReason() {
    let skipReason = this._getTRRSkipReason();
    return Services.dns.getTRRSkipReasonName(skipReason);
  }

  RPMGetTRRDomain() {
    return Services.dns.trrDomain;
  }

  RPMIsSiteSpecificTRRError() {
    let skipReason = this._getTRRSkipReason();
    switch (skipReason) {
      case Ci.nsITRRSkipReason.TRR_NXDOMAIN:
      case Ci.nsITRRSkipReason.TRR_RCODE_FAIL:
      case Ci.nsITRRSkipReason.TRR_NO_ANSWERS:
        return true;
    }
    return false;
  }

  RPMSetTRRDisabledLoadFlags() {
    this.contentWindow.docShell.browsingContext.defaultLoadFlags |=
      Ci.nsIRequest.LOAD_TRR_DISABLED_MODE;
  }
}
