/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["MockColorPicker"];

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "WrapPrivileged",
  "resource://specialpowers/WrapPrivileged.jsm"
);

const Cm = Components.manager;

const CONTRACT_ID = "@mozilla.org/colorpicker;1";

Cu.crashIfNotInAutomation();

var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
var oldClassID = "";
var newClassID = Services.uuid.generateUUID();
var newFactory = function(window) {
  return {
    createInstance(aIID) {
      return new MockColorPickerInstance(window).QueryInterface(aIID);
    },
    QueryInterface: ChromeUtils.generateQI(["nsIFactory"]),
  };
};

var MockColorPicker = {
  init(window) {
    this.reset();
    this.factory = newFactory(window);
    if (!registrar.isCIDRegistered(newClassID)) {
      try {
        oldClassID = registrar.contractIDToCID(CONTRACT_ID);
      } catch (ex) {
        oldClassID = "";
        dump(
          "TEST-INFO | can't get colorpicker registered component, " +
            "assuming there is none"
        );
      }
      registrar.registerFactory(newClassID, "", CONTRACT_ID, this.factory);
    }
  },

  reset() {
    this.returnColor = "";
    this.showCallback = null;
    this.shown = false;
    this.showing = false;
  },

  cleanup() {
    var previousFactory = this.factory;
    this.reset();
    this.factory = null;

    registrar.unregisterFactory(newClassID, previousFactory);
    if (oldClassID != "") {
      registrar.registerFactory(oldClassID, "", CONTRACT_ID, null);
    }
  },
};

function MockColorPickerInstance(window) {
  this.window = window;
  this.showCallback = null;
  this.showCallbackWrapped = null;
}
MockColorPickerInstance.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIColorPicker"]),
  init(aParent, aTitle, aInitialColor) {
    this.parent = aParent;
    this.initialColor = aInitialColor;
  },
  initialColor: "",
  parent: null,
  open(aColorPickerShownCallback) {
    MockColorPicker.showing = true;
    MockColorPicker.shown = true;

    this.window.setTimeout(() => {
      let result = "";
      try {
        if (typeof MockColorPicker.showCallback == "function") {
          if (MockColorPicker.showCallback != this.showCallback) {
            this.showCallback = MockColorPicker.showCallback;
            if (Cu.isXrayWrapper(this.window)) {
              this.showCallbackWrapped = lazy.WrapPrivileged.wrapCallback(
                MockColorPicker.showCallback,
                this.window
              );
            } else {
              this.showCallbackWrapped = this.showCallback;
            }
          }
          var updateCb = function(color) {
            result = color;
            aColorPickerShownCallback.update(color);
          };
          let returnColor = this.showCallbackWrapped(this, updateCb);
          if (typeof returnColor === "string") {
            result = returnColor;
          }
        } else if (typeof MockColorPicker.returnColor === "string") {
          result = MockColorPicker.returnColor;
        }
      } catch (ex) {
        dump(
          "TEST-UNEXPECTED-FAIL | Exception in MockColorPicker.jsm open() " +
            "method: " +
            ex +
            "\n"
        );
      }
      if (aColorPickerShownCallback) {
        aColorPickerShownCallback.done(result);
      }
    }, 0);
  },
};
