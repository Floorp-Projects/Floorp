/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

ChromeUtils.import("resource://gre/modules/BrowserUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");
ChromeUtils.import("resource://gre/modules/WebNavigationChild.jsm");
ChromeUtils.import("resource://gre/modules/WebProgressChild.jsm");

ChromeUtils.defineModuleGetter(this, "PageThumbUtils",
  "resource://gre/modules/PageThumbUtils.jsm");

this.WebProgress = new WebProgressChild(this);
this.WebNavigation = new WebNavigationChild(this);


var ControllerCommands = {
  init() {
    addMessageListener("ControllerCommands:Do", this);
    addMessageListener("ControllerCommands:DoWithParams", this);
    this.init = null;
  },

  receiveMessage(message) {
    switch (message.name) {
      case "ControllerCommands:Do":
        if (docShell.isCommandEnabled(message.data))
          docShell.doCommand(message.data);
        break;

      case "ControllerCommands:DoWithParams":
        var data = message.data;
        if (docShell.isCommandEnabled(data.cmd)) {
          var params = Cc["@mozilla.org/embedcomp/command-params;1"].
                       createInstance(Ci.nsICommandParams);
          for (var name in data.params) {
            var value = data.params[name];
            if (value.type == "long") {
              params.setLongValue(name, parseInt(value.value));
            } else {
              throw Cr.NS_ERROR_NOT_IMPLEMENTED;
            }
          }
          docShell.doCommandWithParams(data.cmd, params);
        }
        break;
    }
  }
};

ControllerCommands.init();

addEventListener("DOMTitleChanged", function(aEvent) {
  if (!aEvent.isTrusted || aEvent.target.defaultView != content)
    return;
  sendAsyncMessage("DOMTitleChanged", { title: content.document.title });
}, false);

addEventListener("DOMWindowClose", function(aEvent) {
  if (!aEvent.isTrusted)
    return;
  sendAsyncMessage("DOMWindowClose");
}, false);

addEventListener("ImageContentLoaded", function(aEvent) {
  if (content.document instanceof Ci.nsIImageDocument) {
    let req = content.document.imageRequest;
    if (!req.image)
      return;
    sendAsyncMessage("ImageDocumentLoaded", { width: req.image.width,
                                              height: req.image.height });
  }
}, false);

const ZoomManager = {
  get fullZoom() {
    return this._cache.fullZoom;
  },

  get textZoom() {
    return this._cache.textZoom;
  },

  set fullZoom(value) {
    this._cache.fullZoom = value;
    this._markupViewer.fullZoom = value;
  },

  set textZoom(value) {
    this._cache.textZoom = value;
    this._markupViewer.textZoom = value;
  },

  refreshFullZoom() {
    return this._refreshZoomValue("fullZoom");
  },

  refreshTextZoom() {
    return this._refreshZoomValue("textZoom");
  },

  /**
   * Retrieves specified zoom property value from markupViewer and refreshes
   * cache if needed.
   * @param valueName Either 'fullZoom' or 'textZoom'.
   * @returns Returns true if cached value was actually refreshed.
   * @private
   */
  _refreshZoomValue(valueName) {
    let actualZoomValue = this._markupViewer[valueName];
    // Round to remove any floating-point error.
    actualZoomValue = Number(actualZoomValue.toFixed(2));
    if (actualZoomValue != this._cache[valueName]) {
      this._cache[valueName] = actualZoomValue;
      return true;
    }
    return false;
  },

  get _markupViewer() {
    return docShell.contentViewer;
  },

  _cache: {
    fullZoom: NaN,
    textZoom: NaN
  }
};

addMessageListener("FullZoom", function(aMessage) {
  ZoomManager.fullZoom = aMessage.data.value;
});

addMessageListener("TextZoom", function(aMessage) {
  ZoomManager.textZoom = aMessage.data.value;
});

addEventListener("FullZoomChange", function() {
  if (ZoomManager.refreshFullZoom()) {
    sendAsyncMessage("FullZoomChange", { value: ZoomManager.fullZoom });
  }
}, false);

addEventListener("TextZoomChange", function(aEvent) {
  if (ZoomManager.refreshTextZoom()) {
    sendAsyncMessage("TextZoomChange", { value: ZoomManager.textZoom });
  }
}, false);

addEventListener("ZoomChangeUsingMouseWheel", function() {
  sendAsyncMessage("ZoomChangeUsingMouseWheel", {});
}, false);

addMessageListener("UpdateCharacterSet", function(aMessage) {
  docShell.charset = aMessage.data.value;
  docShell.gatherCharsetMenuTelemetry();
});

/**
 * Remote thumbnail request handler for PageThumbs thumbnails.
 */
addMessageListener("Browser:Thumbnail:Request", function(aMessage) {
  let snapshot;
  let args = aMessage.data.additionalArgs;
  let fullScale = args ? args.fullScale : false;
  if (fullScale) {
    snapshot = PageThumbUtils.createSnapshotThumbnail(content, null, args);
  } else {
    let snapshotWidth = aMessage.data.canvasWidth;
    let snapshotHeight = aMessage.data.canvasHeight;
    snapshot =
      PageThumbUtils.createCanvas(content, snapshotWidth, snapshotHeight);
    PageThumbUtils.createSnapshotThumbnail(content, snapshot, args);
  }

  snapshot.toBlob(function(aBlob) {
    sendAsyncMessage("Browser:Thumbnail:Response", {
      thumbnail: aBlob,
      id: aMessage.data.id
    });
  });
});

/**
 * Remote isSafeForCapture request handler for PageThumbs.
 */
addMessageListener("Browser:Thumbnail:CheckState", function(aMessage) {
  Services.tm.idleDispatchToMainThread(() => {
    let result = PageThumbUtils.shouldStoreContentThumbnail(content, docShell);
    sendAsyncMessage("Browser:Thumbnail:CheckState:Response", {
      result
    });
  });
});

/**
 * Remote GetOriginalURL request handler for PageThumbs.
 */
addMessageListener("Browser:Thumbnail:GetOriginalURL", function(aMessage) {
  let channel = docShell.currentDocumentChannel;
  let channelError = PageThumbUtils.isChannelErrorResponse(channel);
  let originalURL;
  try {
    originalURL = channel.originalURI.spec;
  } catch (ex) {}
  sendAsyncMessage("Browser:Thumbnail:GetOriginalURL:Response", {
    channelError,
    originalURL,
  });
});

/**
 * Remote createAboutBlankContentViewer request handler.
 */
addMessageListener("Browser:CreateAboutBlank", function(aMessage) {
  if (!content.document || content.document.documentURI != "about:blank") {
    throw new Error("Can't create a content viewer unless on about:blank");
  }
  let principal = aMessage.data;
  principal = BrowserUtils.principalWithMatchingOA(principal, content.document.nodePrincipal);
  docShell.createAboutBlankContentViewer(principal);
});

addMessageListener("InPermitUnload", msg => {
  let inPermitUnload = docShell.contentViewer && docShell.contentViewer.inPermitUnload;
  sendAsyncMessage("InPermitUnload", {id: msg.data.id, inPermitUnload});
});

addMessageListener("PermitUnload", msg => {
  sendAsyncMessage("PermitUnload", {id: msg.data.id, kind: "start"});

  let permitUnload = true;
  if (docShell && docShell.contentViewer) {
    permitUnload = docShell.contentViewer.permitUnload(msg.data.aPermitUnloadFlags);
  }

  sendAsyncMessage("PermitUnload", {id: msg.data.id, kind: "end", permitUnload});
});

// We may not get any responses to Browser:Init if the browser element
// is torn down too quickly.
var outerWindowID = content.windowUtils.outerWindowID;
sendAsyncMessage("Browser:Init", {outerWindowID});
