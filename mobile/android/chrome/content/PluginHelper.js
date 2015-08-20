/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var PluginHelper = {
  showDoorHanger: function(aTab) {
    if (!aTab.browser)
      return;

    // Even though we may not end up showing a doorhanger, this flag
    // lets us know that we've tried to show a doorhanger.
    aTab.shouldShowPluginDoorhanger = false;

    let uri = aTab.browser.currentURI;

    // If the user has previously set a plugins permission for this website,
    // either play or don't play the plugins instead of showing a doorhanger.
    let permValue = Services.perms.testPermission(uri, "plugins");
    if (permValue != Services.perms.UNKNOWN_ACTION) {
      if (permValue == Services.perms.ALLOW_ACTION)
        PluginHelper.playAllPlugins(aTab.browser.contentWindow);

      return;
    }

    let message = Strings.browser.formatStringFromName("clickToPlayPlugins.message2",
                                                       [uri.host], 1);
    let buttons = [
      {
        label: Strings.browser.GetStringFromName("clickToPlayPlugins.dontActivate"),
        callback: function(aChecked) {
          // If the user checked "Don't ask again", make a permanent exception
          if (aChecked)
            Services.perms.add(uri, "plugins", Ci.nsIPermissionManager.DENY_ACTION);

          // Other than that, do nothing
        }
      },
      {
        label: Strings.browser.GetStringFromName("clickToPlayPlugins.activate"),
        callback: function(aChecked) {
          // If the user checked "Don't ask again", make a permanent exception
          if (aChecked)
            Services.perms.add(uri, "plugins", Ci.nsIPermissionManager.ALLOW_ACTION);

          PluginHelper.playAllPlugins(aTab.browser.contentWindow);
        },
        positive: true
      }
    ];

    // Add a checkbox with a "Don't ask again" message if the uri contains a
    // host. Adding a permanent exception will fail if host is not present.
    let options = uri.host ? { checkbox: Strings.browser.GetStringFromName("clickToPlayPlugins.dontAskAgain") } : {};

    NativeWindow.doorhanger.show(message, "ask-to-play-plugins", buttons, aTab.id, options);
  },

  delayAndShowDoorHanger: function(aTab) {
    // To avoid showing the doorhanger if there are also visible plugin
    // overlays on the page, delay showing the doorhanger to check if
    // visible plugins get added in the near future.
    if (!aTab.pluginDoorhangerTimeout) {
      aTab.pluginDoorhangerTimeout = setTimeout(function() {
        if (this.shouldShowPluginDoorhanger) {
          PluginHelper.showDoorHanger(this);
        }
      }.bind(aTab), 500);
    }
  },

  playAllPlugins: function(aContentWindow) {
    let cwu = aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindowUtils);
    // XXX not sure if we should enable plugins for the parent documents...
    let plugins = cwu.plugins;
    if (!plugins || !plugins.length)
      return;

    plugins.forEach(this.playPlugin);
  },

  playPlugin: function(plugin) {
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    if (!objLoadingContent.activated)
      objLoadingContent.playPlugin();
  },

  getPluginPreference: function getPluginPreference() {
    let pluginDisable = Services.prefs.getBoolPref("plugin.disable");
    if (pluginDisable)
      return "0";

    let state = Services.prefs.getIntPref("plugin.default.state");
    return state == Ci.nsIPluginTag.STATE_CLICKTOPLAY ? "2" : "1";
  },

  setPluginPreference: function setPluginPreference(aValue) {
    switch (aValue) {
      case "0": // Enable Plugins = No
        Services.prefs.setBoolPref("plugin.disable", true);
        Services.prefs.clearUserPref("plugin.default.state");
        break;
      case "1": // Enable Plugins = Yes
        Services.prefs.clearUserPref("plugin.disable");
        Services.prefs.setIntPref("plugin.default.state", Ci.nsIPluginTag.STATE_ENABLED);
        break;
      case "2": // Enable Plugins = Tap to Play (default)
        Services.prefs.clearUserPref("plugin.disable");
        Services.prefs.clearUserPref("plugin.default.state");
        break;
    }
  },

  // Copied from /browser/base/content/browser.js
  isTooSmall : function (plugin, overlay) {
    // Is the <object>'s size too small to hold what we want to show?
    let pluginRect = plugin.getBoundingClientRect();
    // XXX bug 446693. The text-shadow on the submitted-report text at
    //     the bottom causes scrollHeight to be larger than it should be.
    let overflows = (overlay.scrollWidth > pluginRect.width) ||
                    (overlay.scrollHeight - 5 > pluginRect.height);

    return overflows;
  },

  getPluginMimeType: function (plugin) {
    var tagMimetype = plugin.actualType;

    if (tagMimetype == "") {
      tagMimetype = plugin.type;
    }

    return tagMimetype;
  },

  handlePluginBindingAttached: function (aTab, aEvent) {
    let plugin = aEvent.target;
    let doc = plugin.ownerDocument;
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    if (!overlay || overlay._bindingHandled) {
      return;
    }
    overlay._bindingHandled = true;

    let eventType = PluginHelper._getBindingType(plugin);
    if (!eventType) {
      // Not all bindings have handlers
      return;
    }

    switch  (eventType) {
      case "PluginClickToPlay": {
        // Check if plugins have already been activated for this page, or if
        // the user has set a permission to always play plugins on the site
        if (aTab.clickToPlayPluginsActivated ||
            Services.perms.testPermission(aTab.browser.currentURI, "plugins") ==
            Services.perms.ALLOW_ACTION) {
          PluginHelper.playPlugin(plugin);
          return;
        }

        // If the plugin is hidden, or if the overlay is too small, show a 
        // doorhanger notification
        if (PluginHelper.isTooSmall(plugin, overlay)) {
          PluginHelper.delayAndShowDoorHanger(aTab);
        } else {
          // There's a large enough visible overlay that we don't need to show
          // the doorhanger.
          aTab.shouldShowPluginDoorhanger = false;
          overlay.classList.add("visible");
        }

        // Add click to play listener to the overlay
        overlay.addEventListener("click", function(e) {
          if (!e.isTrusted)
            return;
          e.preventDefault();
          let win = e.target.ownerDocument.defaultView.top;
          let tab = BrowserApp.getTabForWindow(win);
          tab.clickToPlayPluginsActivated = true;
          PluginHelper.playAllPlugins(win);

          NativeWindow.doorhanger.hide("ask-to-play-plugins", tab.id);
        }, true);

        // Add handlers for over- and underflow in case the plugin gets resized
        plugin.addEventListener("overflow", function(event) {
          overlay.classList.remove("visible");
          PluginHelper.delayAndShowDoorHanger(aTab);
        });
        plugin.addEventListener("underflow", function(event) {
          // This is also triggered if only one dimension underflows,
          // the other dimension might still overflow
          if (!PluginHelper.isTooSmall(plugin, overlay)) {
            overlay.classList.add("visible");
          }
        });

        break;
      }

      case "PluginNotFound": {
        // On devices where we don't support Flash, there will be a
        // "Learn More..." link in the missing plugin error message.
        let learnMoreLink = doc.getAnonymousElementByAttribute(plugin, "class", "unsupportedLearnMoreLink");
        let learnMoreUrl = Services.urlFormatter.formatURLPref("app.support.baseURL");
        learnMoreUrl += "mobile-flash-unsupported";
        learnMoreLink.href = learnMoreUrl;
        overlay.classList.add("visible");
        break;
      }
    }
  },

  // Helper to get the binding handler type from a plugin object
  _getBindingType: function(plugin) {
    if (!(plugin instanceof Ci.nsIObjectLoadingContent))
      return null;

    switch (plugin.pluginFallbackType) {
      case Ci.nsIObjectLoadingContent.PLUGIN_UNSUPPORTED:
        return "PluginNotFound";
      case Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY:
        return "PluginClickToPlay";
      default:
        // Not all states map to a handler
        return null;
    }
  }
};
