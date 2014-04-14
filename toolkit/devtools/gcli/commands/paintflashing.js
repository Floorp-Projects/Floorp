/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
const TargetFactory = require("resource://gre/modules/devtools/Loader.jsm").devtools.TargetFactory;

const Telemetry = require("devtools/shared/telemetry");
const telemetry = new Telemetry();

const EventEmitter = require("devtools/toolkit/event-emitter");
const eventEmitter = new EventEmitter();

const gcli = require("gcli/index");

function onPaintFlashingChanged(context) {
  let tab = context.environment.chromeWindow.gBrowser.selectedTab;
  eventEmitter.emit("changed", tab);
  function fireChange() {
    eventEmitter.emit("changed", tab);
  }
  let target = TargetFactory.forTab(tab);
  target.off("navigate", fireChange);
  target.once("navigate", fireChange);

  let window = context.environment.window;
  let wUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils);
  if (wUtils.paintFlashing) {
    telemetry.toolOpened("paintflashing");
  } else {
    telemetry.toolClosed("paintflashing");
  }
}

exports.items = [
  {
    name: "paintflashing",
    description: gcli.lookup("paintflashingDesc")
  },
  {
    name: "paintflashing on",
    description: gcli.lookup("paintflashingOnDesc"),
    manual: gcli.lookup("paintflashingManual"),
    params: [{
      group: "options",
      params: [
        {
          type: "boolean",
          name: "chrome",
          get hidden() gcli.hiddenByChromePref(),
          description: gcli.lookup("paintflashingChromeDesc"),
        }
      ]
    }],
    exec: function(args, context) {
      let window = args.chrome ?
                  context.environment.chromeWindow :
                  context.environment.window;

      window.QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIDOMWindowUtils)
            .paintFlashing = true;
      onPaintFlashingChanged(context);
    }
  },
  {
    name: "paintflashing off",
    description: gcli.lookup("paintflashingOffDesc"),
    manual: gcli.lookup("paintflashingManual"),
    params: [{
      group: "options",
      params: [
        {
          type: "boolean",
          name: "chrome",
          get hidden() gcli.hiddenByChromePref(),
          description: gcli.lookup("paintflashingChromeDesc"),
        }
      ]
    }],
    exec: function(args, context) {
      let window = args.chrome ?
                  context.environment.chromeWindow :
                  context.environment.window;

      window.QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIDOMWindowUtils)
            .paintFlashing = false;
      onPaintFlashingChanged(context);
    }
  },
  {
    name: "paintflashing toggle",
    hidden: true,
    buttonId: "command-button-paintflashing",
    buttonClass: "command-button command-button-invertable",
    state: {
      isChecked: function(aTarget) {
        if (aTarget.isLocalTab) {
          let window = aTarget.tab.linkedBrowser.contentWindow;
          let wUtils = window.QueryInterface(Ci.nsIInterfaceRequestor).
                              getInterface(Ci.nsIDOMWindowUtils);
          return wUtils.paintFlashing;
        } else {
          throw new Error("Unsupported target");
        }
      },
      onChange: function(aTarget, aChangeHandler) {
        eventEmitter.on("changed", aChangeHandler);
      },
      offChange: function(aTarget, aChangeHandler) {
        eventEmitter.off("changed", aChangeHandler);
      },
    },
    tooltipText: gcli.lookup("paintflashingTooltip"),
    description: gcli.lookup("paintflashingToggleDesc"),
    manual: gcli.lookup("paintflashingManual"),
    exec: function(args, context) {
      let window = context.environment.window;
      let wUtils = window.QueryInterface(Ci.nsIInterfaceRequestor).
                   getInterface(Ci.nsIDOMWindowUtils);
      wUtils.paintFlashing = !wUtils.paintFlashing;
      onPaintFlashingChanged(context);
    }
  }
];
