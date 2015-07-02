// -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "DebuggerServer",
                                  "resource://gre/modules/devtools/dbg-server.jsm");

var RemoteDebugger = {
  init() {
    Services.prefs.addObserver("devtools.debugger.", this, false);

    if (this._isEnabled())
      this._start();
  },

  observe(aSubject, aTopic, aData) {
    if (aTopic != "nsPref:changed")
      return;

    switch (aData) {
      case "devtools.debugger.remote-enabled":
        if (this._isEnabled())
          this._start();
        else
          this._stop();
        break;

      case "devtools.debugger.remote-port":
      case "devtools.debugger.unix-domain-socket":
        if (this._isEnabled())
          this._restart();
        break;
    }
  },

  _getPort() {
    return Services.prefs.getIntPref("devtools.debugger.remote-port");
  },

  _getPath() {
    return Services.prefs.getCharPref("devtools.debugger.unix-domain-socket");
  },

  _isEnabled() {
    return Services.prefs.getBoolPref("devtools.debugger.remote-enabled");
  },

  /**
   * Prompt the user to accept or decline the incoming connection.
   * This is passed to DebuggerService.init as a callback.
   *
   * @return An AuthenticationResult value.
   *         A promise that will be resolved to the above is also allowed.
   */
  _showConnectionPrompt() {
    let title = Strings.browser.GetStringFromName("remoteIncomingPromptTitle");
    let msg = Strings.browser.GetStringFromName("remoteIncomingPromptMessage");
    let disable = Strings.browser.GetStringFromName("remoteIncomingPromptDisable");
    let cancel = Strings.browser.GetStringFromName("remoteIncomingPromptCancel");
    let agree = Strings.browser.GetStringFromName("remoteIncomingPromptAccept");

    // Make prompt. Note: button order is in reverse.
    let prompt = new Prompt({
      window: null,
      hint: "remotedebug",
      title: title,
      message: msg,
      buttons: [ agree, cancel, disable ],
      priority: 1
    });

    // The debugger server expects a synchronous response, so spin on result since Prompt is async.
    let result = null;

    prompt.show(function(data) {
      result = data.button;
    });

    // Spin this thread while we wait for a result.
    let thread = Services.tm.currentThread;
    while (result == null)
      thread.processNextEvent(true);

    if (result === 0)
      return DebuggerServer.AuthenticationResult.ALLOW;
    if (result === 2) {
      return DebuggerServer.AuthenticationResult.DISABLE_ALL;
    }
    return DebuggerServer.AuthenticationResult.DENY;
  },

  _restart() {
    this._stop();
    this._start();
  },

  _start() {
    try {
      if (!DebuggerServer.initialized) {
        DebuggerServer.init();
        DebuggerServer.addBrowserActors();
        DebuggerServer.registerModule("resource://gre/modules/dbg-browser-actors.js");
        DebuggerServer.allowChromeProcess = true;
      }

      let pathOrPort = this._getPath();
      if (!pathOrPort)
        pathOrPort = this._getPort();
      let AuthenticatorType = DebuggerServer.Authenticators.get("PROMPT");
      let authenticator = new AuthenticatorType.Server();
      authenticator.allowConnection = this._showConnectionPrompt.bind(this);
      let listener = DebuggerServer.createListener();
      listener.portOrPath = pathOrPort;
      listener.authenticator = authenticator;
      listener.open();
      dump("Remote debugger listening at path " + pathOrPort);
    } catch(e) {
      dump("Remote debugger didn't start: " + e);
    }
  },

  _stop() {
    DebuggerServer.closeAllListeners();
    dump("Remote debugger stopped");
  }
};
