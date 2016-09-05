/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionContext"];

/*
 * This file handles addon logic that is independent of the chrome process.
 * When addons run out-of-process, this is the main entry point.
 * Its primary function is managing addon globals.
 *
 * Don't put contentscript logic here, use ExtensionContent.jsm instead.
 */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  BaseContext,
  Messenger,
} = ExtensionUtils;

// There is a circular dependency between Extension.jsm and us.
// Long-term this file should not reference Extension.jsm (because they would
// live in different processes), but for now use lazy getters.
XPCOMUtils.defineLazyGetter(this, "Management",
  () => Cu.import("resource://gre/modules/Extension.jsm", {}).Management);

// An extension page is an execution context for any extension content
// that runs in the chrome process. It's used for background pages
// (type="background"), popups (type="popup"), and any extension
// content loaded into browser tabs (type="tab").
//
// |params| is an object with the following properties:
// |type| is one of "background", "popup", or "tab".
// |contentWindow| is the DOM window the content runs in.
// |uri| is the URI of the content (optional).
// |docShell| is the docshell the content runs in (optional).
this.ExtensionContext = class extends BaseContext {
  constructor(extension, params) {
    // TODO(robwu): This should be addon_child once all ext- files are split.
    // There should be a new ProxyContext instance with the "addon_parent" type.
    super("addon_parent", extension);

    let {type, uri} = params;
    this.type = type;
    this.uri = uri || extension.baseURI;

    this.setContentWindow(params.contentWindow);

    // This is the MessageSender property passed to extension.
    // It can be augmented by the "page-open" hook.
    let sender = {id: extension.uuid};
    if (uri) {
      sender.url = uri.spec;
    }
    Management.emit("page-load", this, params, sender);

    let filter = {extensionId: extension.id};
    let optionalFilter = {};
    // Addon-generated messages (not necessarily from the same process as the
    // addon itself) are sent to the main process, which forwards them via the
    // parent process message manager. Specific replies can be sent to the frame
    // message manager.
    this.messenger = new Messenger(this, [Services.cpmm, this.messageManager], sender, filter, optionalFilter);

    if (this.externallyVisible) {
      this.extension.views.add(this);
    }
  }

  get cloneScope() {
    return this.contentWindow;
  }

  get principal() {
    return this.contentWindow.document.nodePrincipal;
  }

  get externallyVisible() {
    return true;
  }

  // Called when the extension shuts down.
  shutdown() {
    Management.emit("page-shutdown", this);
    this.unload();
  }

  // This method is called when an extension page navigates away or
  // its tab is closed.
  unload() {
    // Note that without this guard, we end up running unload code
    // multiple times for tab pages closed by the "page-unload" handlers
    // triggered below.
    if (this.unloaded) {
      return;
    }

    super.unload();

    Management.emit("page-unload", this);

    if (this.externallyVisible) {
      this.extension.views.delete(this);
    }
  }
};


