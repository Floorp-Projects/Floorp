/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
const TargetFactory = require("resource://gre/modules/devtools/Loader.jsm").devtools.TargetFactory;

let telemetry;
try {
  const Telemetry = require("devtools/shared/telemetry");
  telemetry = new Telemetry();
} catch(e) {
  // DevTools Telemetry module only available in Firefox
}

const EventEmitter = require("devtools/toolkit/event-emitter");
const eventEmitter = new EventEmitter();

const gcli = require("gcli/index");
const l10n = require("gcli/l10n");

/**
 * Keep a store of the paintFlashing state here. This is a nasty hack but
 * the only other way to know is to ask the server, which is async and we need
 * the answer synchronously in "paintflashing toggle".state()
 */
let isContentPaintFlashing = false;

/**
 * Fire events and telemetry when paintFlashing happens
 */
function onPaintFlashingChanged(target, value) {
  eventEmitter.emit("changed", { target: target });
  function fireChange() {
    eventEmitter.emit("changed", { target: target });
  }

  target.off("navigate", fireChange);
  target.once("navigate", fireChange);

  if (!telemetry) {
    return;
  }
  if (value) {
    telemetry.toolOpened("paintflashing");
  } else {
    telemetry.toolClosed("paintflashing");
  }
}

/**
 * Alter the paintFlashing state of a window and report on the new value.
 * This works with chrome or content windows.
 *
 * This is a bizarre method that you could argue should be broken up into
 * separate getter and setter functions, however keeping it as one helps
 * to simplify the commands below.
 *
 * @param state {string} One of:
 * - "on" which does window.paintFlashing = true
 * - "off" which does window.paintFlashing = false
 * - "toggle" which does window.paintFlashing = !window.paintFlashing
 * - "query" which does nothing
 * @return The new value of the window.paintFlashing flag
 */
function setPaintFlashing(window, state) {
  const winUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils)

  if (["on", "off", "toggle", "query"].indexOf(state) === -1) {
    throw new Error("Unsupported state: " + state);
  }

  if (state === "on") {
    winUtils.paintFlashing = true;
  }
  else if (state === "off") {
    winUtils.paintFlashing = false;
  }
  else if (state === "toggle") {
    winUtils.paintFlashing = !winUtils.paintFlashing;
  }

  return winUtils.paintFlashing;
}

exports.items = [
  {
    name: "paintflashing",
    description: l10n.lookup("paintflashingDesc")
  },
  {
    item: "command",
    runAt: "client",
    name: "paintflashing on",
    description: l10n.lookup("paintflashingOnDesc"),
    manual: l10n.lookup("paintflashingManual"),
    params: [{
      group: "options",
      params: [
        {
          type: "boolean",
          name: "chrome",
          get hidden() gcli.hiddenByChromePref(),
          description: l10n.lookup("paintflashingChromeDesc"),
        }
      ]
    }],
    exec: function*(args, context) {
      if (!args.chrome) {
        const value = yield context.updateExec("paintflashing_server --state on");
        isContentPaintFlashing = value;
        onPaintFlashingChanged(context.environment.target, value);
      }
      else {
        setPaintFlashing(context.environment.chromeWindow, "on");
      }
    }
  },
  {
    item: "command",
    runAt: "client",
    name: "paintflashing off",
    description: l10n.lookup("paintflashingOffDesc"),
    manual: l10n.lookup("paintflashingManual"),
    params: [{
      group: "options",
      params: [
        {
          type: "boolean",
          name: "chrome",
          get hidden() gcli.hiddenByChromePref(),
          description: l10n.lookup("paintflashingChromeDesc"),
        }
      ]
    }],
    exec: function(args, context) {
      if (!args.chrome) {
        const value = yield context.updateExec("paintflashing_server --state off");
        isContentPaintFlashing = value;
        onPaintFlashingChanged(context.environment.target, value);
      }
      else {
        setPaintFlashing(context.environment.chromeWindow, "off");
      }
    }
  },
  {
    item: "command",
    runAt: "client",
    name: "paintflashing toggle",
    hidden: true,
    buttonId: "command-button-paintflashing",
    buttonClass: "command-button command-button-invertable",
    state: {
      isChecked: () => isContentPaintFlashing,
      onChange: (_, handler) => eventEmitter.on("changed", handler),
      offChange: (_, handler) => eventEmitter.off("changed", handler),
    },
    tooltipText: l10n.lookup("paintflashingTooltip"),
    description: l10n.lookup("paintflashingToggleDesc"),
    manual: l10n.lookup("paintflashingManual"),
    exec: function(args, context) {
      const value = yield context.updateExec("paintflashing_server --state toggle");
      isContentPaintFlashing = value;
      onPaintFlashingChanged(context.environment.target, value);
    }
  },
  {
    item: "command",
    runAt: "server",
    name: "paintflashing_server",
    hidden: true,
    params: [
      {
        name: "state",
        type: {
          name: "selection",
          data: [ "on", "off", "toggle", "query" ]
        }
      },
    ],
    returnType: "boolean",
    exec: function(args, context) {
      return setPaintFlashing(context.environment.window, args.state);
    }
  }
];
