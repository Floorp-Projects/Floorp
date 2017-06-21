/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict"

const { classes: Cc, interfaces: Ci, manager: Cm, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Prompt",
                                  "resource://gre/modules/Prompt.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "EventDispatcher",
                                  "resource://gre/modules/Messaging.jsm");

function TabSource() {
}

TabSource.prototype = {
  classID: Components.ID("{5850c76e-b916-4218-b99a-31f004e0a7e7}"),
  classDescription: "Fennec Tab Source",
  contractID: "@mozilla.org/tab-source-service;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITabSource]),

  getTabToStream: function() {
    let app = Services.wm.getMostRecentWindow("navigator:browser").BrowserApp;
    let tabs = app.tabs;
    if (tabs == null || tabs.length == 0) {
      Services.console.logStringMessage("ERROR: No tabs");
      return null;
    }

    let bundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
    let title = bundle.GetStringFromName("tabshare.title")

    let prompt = new Prompt({
      title: title,
      window: null
    }).setSingleChoiceItems(tabs.map(function(tab) {
      let label;
      if (tab.browser.contentTitle)
        label = tab.browser.contentTitle;
      else if (tab.browser.contentURI)
        label = tab.browser.contentURI.spec;
      else
        label = tab.originalURI.spec;
      return { label: label,
               icon: "thumbnail:" + tab.id }
    }));

    let result = null;
    prompt.show(function(data) {
      result = data.button;
    });

    // Spin this thread while we wait for a result.
    Services.tm.spinEventLoopUntil(() => result != null);

    if (result == -1) {
      return null;
    }
    return tabs[result].browser.contentWindow;
  },

  notifyStreamStart: function(window) {
    let app = Services.wm.getMostRecentWindow("navigator:browser").BrowserApp;
    let tabs = app.tabs;
    for (var i in tabs) {
      if (tabs[i].browser.contentWindow == window) {
        EventDispatcher.instance.sendRequest({
          type: "Tab:RecordingChange",
          recording: true,
          tabID: tabs[i].id,
        });
      }
    }
  },

  notifyStreamStop: function(window) {
    let app = Services.wm.getMostRecentWindow("navigator:browser").BrowserApp;
    let tabs = app.tabs;
    for (let i in tabs) {
      if (tabs[i].browser.contentWindow == window) {
        EventDispatcher.instance.sendRequest({
          type: "Tab:RecordingChange",
          recording: false,
          tabID: tabs[i].id,
        });
      }
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TabSource]);
