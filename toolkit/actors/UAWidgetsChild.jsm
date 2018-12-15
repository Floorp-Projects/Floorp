/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["UAWidgetsChild"];

ChromeUtils.import("resource://gre/modules/ActorChild.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

class UAWidgetsChild extends ActorChild {
  constructor(dispatcher) {
    super(dispatcher);

    this.widgets = new WeakMap();
  }

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "UAWidgetSetupOrChange":
        this.setupOrNotifyWidget(aEvent.target);
        break;
      case "UAWidgetTeardown":
        this.teardownWidget(aEvent.target);
        break;
    }

    // In case we are a nested frame, prevent the message manager of the
    // parent frame from receving the event.
    aEvent.stopPropagation();
  }

  setupOrNotifyWidget(aElement) {
    let widget = this.widgets.get(aElement);
    if (!widget) {
      this.setupWidget(aElement);
      return;
    }
    if (typeof widget.wrappedJSObject.onchange == "function") {
      try {
        widget.wrappedJSObject.onchange();
      } catch (ex) {
        Cu.reportError(ex);
      }
    }
  }

  setupWidget(aElement) {
    let uri;
    let widgetName;
    switch (aElement.localName) {
      case "video":
      case "audio":
        uri = "chrome://global/content/elements/videocontrols.js";
        widgetName = "VideoControlsWidget";
        break;
      case "input":
        uri = "chrome://global/content/elements/datetimebox.js";
        widgetName = "DateTimeBoxWidget";
        break;
      case "embed":
      case "object":
        uri = "chrome://global/content/elements/pluginProblem.js";
        widgetName = "PluginProblemWidget";
        break;
      case "marquee":
        uri = "chrome://global/content/elements/marquee.js";
        widgetName = "MarqueeWidget";
        break;
    }

    if (!uri || !widgetName) {
      Cu.reportError("Getting a UAWidgetSetupOrChange event on undefined element.");
      return;
    }

    let shadowRoot = aElement.openOrClosedShadowRoot;
    if (!shadowRoot) {
      Cu.reportError("Getting a UAWidgetSetupOrChange event without the Shadow Root.");
      return;
    }

    let isSystemPrincipal = aElement.nodePrincipal.isSystemPrincipal;
    let sandbox = isSystemPrincipal ?
      Object.create(null) : Cu.getUAWidgetScope(aElement.nodePrincipal);

    if (!sandbox[widgetName]) {
      Services.scriptloader.loadSubScript(uri, sandbox, "UTF-8");
    }

    let widget = new sandbox[widgetName](shadowRoot);
    this.widgets.set(aElement, widget);
    try {
      if (!isSystemPrincipal) {
        widget.wrappedJSObject.onsetup();
      } else {
        widget.onsetup();
      }
    } catch (ex) {
      Cu.reportError(ex);
    }
  }

  teardownWidget(aElement) {
    let widget = this.widgets.get(aElement);
    if (!widget) {
      return;
    }
    if (typeof widget.wrappedJSObject.destructor == "function") {
      try {
        widget.wrappedJSObject.destructor();
      } catch (ex) {
        Cu.reportError(ex);
      }
    }
    this.widgets.delete(aElement);
  }
}
