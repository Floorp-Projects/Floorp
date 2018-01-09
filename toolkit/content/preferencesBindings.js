/* - This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// We attach Preferences to the window object so other contexts (tests, JSMs)
// have access to it.
const Preferences = window.Preferences = (function() {
  const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

  Cu.import("resource://gre/modules/EventEmitter.jsm");
  Cu.import("resource://gre/modules/Services.jsm");
  Cu.import("resource://gre/modules/XPCOMUtils.jsm");

  XPCOMUtils.defineLazyModuleGetter(this, "DeferredTask",
                                    "resource://gre/modules/DeferredTask.jsm");

  function getElementsByAttribute(name, value) {
    // If we needed to defend against arbitrary values, we would escape
    // double quotes (") and escape characters (\) in them, i.e.:
    //   ${value.replace(/["\\]/g, '\\$&')}
    return value ? document.querySelectorAll(`[${name}="${value}"]`)
                 : document.querySelectorAll(`[${name}]`);
  }

  const domContentLoadedPromise = new Promise(resolve => {
    window.addEventListener("DOMContentLoaded", resolve, { capture: true, once: true });
  });

  const Preferences = {
    _all: {},

    _add(prefInfo) {
      if (this._all[prefInfo.id]) {
        throw new Error(`preference with id '${prefInfo.id}' already added`);
      }
      const pref = new Preference(prefInfo);
      this._all[pref.id] = pref;
      domContentLoadedPromise.then(() => { pref.updateElements(); });
      return pref;
    },

    add(prefInfo) {
      const pref = this._add(prefInfo);
      return pref;
    },

    addAll(prefInfos) {
      prefInfos.map(prefInfo => this._add(prefInfo));
    },

    get(id) {
      return this._all[id] || null;
    },

    getAll() {
      return Object.values(this._all);
    },

    defaultBranch: Services.prefs.getDefaultBranch(""),

    get type() {
      return document.documentElement.getAttribute("type") || "";
    },

    get instantApply() {
      // The about:preferences page forces instantApply.
      if (this._instantApplyForceEnabled) {
        return true;
      }

      // Dialogs of type="child" are never instantApply.
      if (this.type === "child") {
        return false;
      }

      // All other pref windows observe the value of the instantApply
      // preference.  Note that, as of this writing, the only such windows
      // are in tests, so it should be possible to remove the pref
      // (and forceEnableInstantApply) in favor of always applying in a parent
      // and never applying in a child.
      return Services.prefs.getBoolPref("browser.preferences.instantApply");
    },

    _instantApplyForceEnabled: false,

    // Override the computed value of instantApply for this window.
    forceEnableInstantApply() {
      this._instantApplyForceEnabled = true;
    },

    observe(subject, topic, data) {
      const pref = this._all[data];
      if (pref) {
        pref.value = pref.valueFromPreferences;
      }
    },

    onDOMContentLoaded() {
      // Iterate elements with a "preference" attribute and log an error
      // if there isn't a corresponding Preference object in order to catch
      // any cases of elements referencing preferences that haven't (yet?)
      // been registered.
      //
      // TODO: remove this code once we determine that there are no such
      // elements (or resolve any bugs that cause this behavior).
      //
      const elements = getElementsByAttribute("preference");
      for (const element of elements) {
        const id = element.getAttribute("preference");
        const pref = this.get(id);
        if (!pref) {
          console.error(`Missing preference for ID ${id}`);
        }
      }
    },

    onUnload() {
      Services.prefs.removeObserver("", this);
    },

    QueryInterface: XPCOMUtils.generateQI([
      Ci.nsITimerCallback,
      Ci.nsIObserver,
    ]),

    _deferredValueUpdateElements: new Set(),

    writePreferences(aFlushToDisk) {
      // Write all values to preferences.
      if (this._deferredValueUpdateElements.size) {
        this._finalizeDeferredElements();
      }

      const preferences = Preferences.getAll();
      for (const preference of preferences) {
        preference.batching = true;
        preference.valueFromPreferences = preference.value;
        preference.batching = false;
      }
      if (aFlushToDisk) {
        Services.prefs.savePrefFile(null);
      }
    },

    getPreferenceElement(aStartElement) {
      let temp = aStartElement;
      while (temp && temp.nodeType == Node.ELEMENT_NODE &&
             !temp.hasAttribute("preference"))
        temp = temp.parentNode;
      return temp && temp.nodeType == Node.ELEMENT_NODE ?
             temp : aStartElement;
    },

    _deferredValueUpdate(aElement) {
      delete aElement._deferredValueUpdateTask;
      const prefID = aElement.getAttribute("preference");
      const preference = Preferences.get(prefID);
      const prefVal = preference.getElementValue(aElement);
      preference.value = prefVal;
      this._deferredValueUpdateElements.delete(aElement);
    },

    _finalizeDeferredElements() {
      for (const el of this._deferredValueUpdateElements) {
        if (el._deferredValueUpdateTask) {
          el._deferredValueUpdateTask.finalize();
        }
      }
    },

    userChangedValue(aElement) {
      const element = this.getPreferenceElement(aElement);
      if (element.hasAttribute("preference")) {
        if (element.getAttribute("delayprefsave") != "true") {
          const preference = Preferences.get(element.getAttribute("preference"));
          const prefVal = preference.getElementValue(element);
          preference.value = prefVal;
        } else {
          if (!element._deferredValueUpdateTask) {
            element._deferredValueUpdateTask =
              new DeferredTask(this._deferredValueUpdate.bind(this, element), 1000);
            this._deferredValueUpdateElements.add(element);
          } else {
            // Each time the preference is changed, restart the delay.
            element._deferredValueUpdateTask.disarm();
          }
          element._deferredValueUpdateTask.arm();
        }
      }
    },

    onCommand(event) {
      // This "command" event handler tracks changes made to preferences by
      // the user in this window.
      if (event.sourceEvent)
        event = event.sourceEvent;
      this.userChangedValue(event.target);
    },

    onSelect(event) {
      // This "select" event handler tracks changes made to colorpicker
      // preferences by the user in this window.
      if (event.target.localName == "colorpicker")
        this.userChangedValue(event.target);
    },

    onChange(event) {
      // This "change" event handler tracks changes made to preferences by
      // the user in this window.
      this.userChangedValue(event.target);
    },

    onInput(event) {
      // This "input" event handler tracks changes made to preferences by
      // the user in this window.
      this.userChangedValue(event.target);
    },

    _fireEvent(aEventName, aTarget) {
      // Panel loaded, synthesize a load event.
      try {
        const event = document.createEvent("Events");
        event.initEvent(aEventName, true, true);
        let cancel = !aTarget.dispatchEvent(event);
        if (aTarget.hasAttribute("on" + aEventName)) {
          const fn = new Function("event", aTarget.getAttribute("on" + aEventName));
          const rv = fn.call(aTarget, event);
          if (rv == false)
            cancel = true;
        }
        return !cancel;
      } catch (e) {
        Cu.reportError(e);
      }
      return false;
    },

    onDialogAccept(event) {
      if (!this._fireEvent("beforeaccept", document.documentElement)) {
        event.preventDefault();
        return false;
      }
      this.writePreferences(true);
      return true;
    },

    close(event) {
      if (Preferences.instantApply)
        window.close();
      event.stopPropagation();
      event.preventDefault();
    },

    handleEvent(event) {
      switch (event.type) {
        case "change": return this.onChange(event);
        case "command": return this.onCommand(event);
        case "dialogaccept": return this.onDialogAccept(event);
        case "input": return this.onInput(event);
        case "select": return this.onSelect(event);
        case "unload": return this.onUnload(event);
        default: return undefined;
      }
    },
  };

  Services.prefs.addObserver("", Preferences);
  domContentLoadedPromise.then(result => Preferences.onDOMContentLoaded(result));
  window.addEventListener("change", Preferences);
  window.addEventListener("command", Preferences);
  window.addEventListener("dialogaccept", Preferences);
  window.addEventListener("input", Preferences);
  window.addEventListener("select", Preferences);
  window.addEventListener("unload", Preferences, { once: true });

  class Preference extends EventEmitter {
    constructor({ id, name, type, inverted, disabled }) {
      super();
      this.on("change", this.onChange.bind(this));

      this._value = null;
      this.readonly = false;
      this._useDefault = false;
      this.batching = false;

      this.id = id;
      this._name = name || this.id;
      this.type = type;
      this.inverted = !!inverted;
      this._disabled = !!disabled;

      // if the element has been inserted without the name attribute set,
      // we have nothing to do here
      if (!this.name) {
        throw new Error(`preference with id '${id}' doesn't have name`);
      }

      // In non-instant apply mode, we must try and use the last saved state
      // from any previous opens of a child dialog instead of the value from
      // preferences, to pick up any edits a user may have made.

      if (Preferences.type == "child" && window.opener &&
          window.opener.Preferences &&
          Services.scriptSecurityManager.isSystemPrincipal(window.opener.document.nodePrincipal)) {
        // Try to find the preference in the parent window.
        const preference = window.opener.Preferences.get(this.name);

        // Don't use the value setter here, we don't want updateElements to be
        // prematurely fired.
        this._value = preference ? preference.value : this.valueFromPreferences;
      } else
        this._value = this.valueFromPreferences;
    }

    reset() {
      // defer reset until preference update
      this.value = undefined;
    }

    _reportUnknownType() {
      const msg = `Preference with id=${this.id} and name=${this.name} has unknown type ${this.type}.`;
      Services.console.logStringMessage(msg);
    }

    setElementValue(aElement) {
      if (this.locked)
        aElement.disabled = true;

      if (!this.isElementEditable(aElement))
        return;

      let rv = undefined;
      if (aElement.hasAttribute("onsyncfrompreference")) {
        // Value changed, synthesize an event
        try {
          const event = document.createEvent("Events");
          event.initEvent("syncfrompreference", true, true);
          const f = new Function("event",
                               aElement.getAttribute("onsyncfrompreference"));
          rv = f.call(aElement, event);
        } catch (e) {
          Cu.reportError(e);
        }
      }
      let val = rv;
      if (val === undefined)
        val = Preferences.instantApply ? this.valueFromPreferences : this.value;
      // if the preference is marked for reset, show default value in UI
      if (val === undefined)
        val = this.defaultValue;

      /**
       * Initialize a UI element property with a value. Handles the case
       * where an element has not yet had a XBL binding attached for it and
       * the property setter does not yet exist by setting the same attribute
       * on the XUL element using DOM apis and assuming the element's
       * constructor or property getters appropriately handle this state.
       */
      function setValue(element, attribute, value) {
        if (attribute in element)
          element[attribute] = value;
        else
          element.setAttribute(attribute, value);
      }
      if (aElement.localName == "checkbox" ||
          aElement.localName == "listitem")
        setValue(aElement, "checked", val);
      else if (aElement.localName == "colorpicker")
        setValue(aElement, "color", val);
      else if (aElement.localName == "textbox") {
        // XXXmano Bug 303998: Avoid a caret placement issue if either the
        // preference observer or its setter calls updateElements as a result
        // of the input event handler.
        if (aElement.value !== val)
          setValue(aElement, "value", val);
      } else
        setValue(aElement, "value", val);
    }

    getElementValue(aElement) {
      if (aElement.hasAttribute("onsynctopreference")) {
        // Value changed, synthesize an event
        try {
          const event = document.createEvent("Events");
          event.initEvent("synctopreference", true, true);
          const f = new Function("event",
                               aElement.getAttribute("onsynctopreference"));
          const rv = f.call(aElement, event);
          if (rv !== undefined)
            return rv;
        } catch (e) {
          Cu.reportError(e);
        }
      }

      /**
       * Read the value of an attribute from an element, assuming the
       * attribute is a property on the element's node API. If the property
       * is not present in the API, then assume its value is contained in
       * an attribute, as is the case before a binding has been attached.
       */
      function getValue(element, attribute) {
        if (attribute in element)
          return element[attribute];
        return element.getAttribute(attribute);
      }
      let value;
      if (aElement.localName == "checkbox" ||
          aElement.localName == "listitem")
        value = getValue(aElement, "checked");
      else if (aElement.localName == "colorpicker")
        value = getValue(aElement, "color");
      else
        value = getValue(aElement, "value");

      switch (this.type) {
      case "int":
        return parseInt(value, 10) || 0;
      case "bool":
        return typeof(value) == "boolean" ? value : value == "true";
      }
      return value;
    }

    isElementEditable(aElement) {
      switch (aElement.localName) {
      case "checkbox":
      case "colorpicker":
      case "radiogroup":
      case "textbox":
      case "listitem":
      case "listbox":
      case "menulist":
        return true;
      }
      return aElement.getAttribute("preference-editable") == "true";
    }

    updateElements() {
      if (!this.id)
        return;

      // This "change" event handler tracks changes made to preferences by
      // sources other than the user in this window.
      const elements = getElementsByAttribute("preference", this.id);
      for (const element of elements)
        this.setElementValue(element);
    }

    onChange() {
      this.updateElements();
    }

    get name() {
      return this._name;
    }

    set name(val) {
      if (val == this.name)
        return val;

      this._name = val;

      return val;
    }

    get value() {
      return this._value;
    }

    set value(val) {
      if (this.value !== val) {
        this._value = val;
        if (Preferences.instantApply)
          this.valueFromPreferences = val;
        this.emit("change");
      }
      return val;
    }

    get locked() {
      return Services.prefs.prefIsLocked(this.name);
    }

    get disabled() {
      return this._disabled;
    }

    set disabled(val) {
      this._disabled = !!val;

      if (!this.id)
        return val;

      const elements = getElementsByAttribute("preference", this.id);
      for (const element of elements) {
        element.disabled = val;

        const labels = getElementsByAttribute("control", element.id);
        for (const label of labels)
          label.disabled = val;
      }

      return val;
    }

    get hasUserValue() {
      return Services.prefs.prefHasUserValue(this.name) &&
             this.value !== undefined;
    }

    get defaultValue() {
      this._useDefault = true;
      const val = this.valueFromPreferences;
      this._useDefault = false;
      return val;
    }

    get _branch() {
      return this._useDefault ? Preferences.defaultBranch : Services.prefs;
    }

    get valueFromPreferences() {
      try {
        // Force a resync of value with preferences.
        switch (this.type) {
        case "int":
          return this._branch.getIntPref(this.name);
        case "bool": {
          const val = this._branch.getBoolPref(this.name);
          return this.inverted ? !val : val;
        }
        case "wstring":
          return this._branch
                     .getComplexValue(this.name, Ci.nsIPrefLocalizedString)
                     .data;
        case "string":
        case "unichar":
          return this._branch.getStringPref(this.name);
        case "fontname": {
          const family = this._branch.getStringPref(this.name);
          const fontEnumerator = Cc["@mozilla.org/gfx/fontenumerator;1"]
                               .createInstance(Ci.nsIFontEnumerator);
          return fontEnumerator.getStandardFamilyName(family);
        }
        case "file": {
          const f = this._branch
                      .getComplexValue(this.name, Ci.nsIFile);
          return f;
        }
        default:
          this._reportUnknownType();
        }
      } catch (e) { }
      return null;
    }

    set valueFromPreferences(val) {
      // Exit early if nothing to do.
      if (this.readonly || this.valueFromPreferences == val)
        return val;

      // The special value undefined means 'reset preference to default'.
      if (val === undefined) {
        Services.prefs.clearUserPref(this.name);
        return val;
      }

      // Force a resync of preferences with value.
      switch (this.type) {
      case "int":
        Services.prefs.setIntPref(this.name, val);
        break;
      case "bool":
        Services.prefs.setBoolPref(this.name, this.inverted ? !val : val);
        break;
      case "wstring": {
        const pls = Cc["@mozilla.org/pref-localizedstring;1"]
                  .createInstance(Ci.nsIPrefLocalizedString);
        pls.data = val;
        Services.prefs
            .setComplexValue(this.name, Ci.nsIPrefLocalizedString, pls);
        break;
      }
      case "string":
      case "unichar":
      case "fontname":
        Services.prefs.setStringPref(this.name, val);
        break;
      case "file": {
        let lf;
        if (typeof(val) == "string") {
          lf = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
          lf.persistentDescriptor = val;
          if (!lf.exists())
            lf.initWithPath(val);
        } else
          lf = val.QueryInterface(Ci.nsIFile);
        Services.prefs
            .setComplexValue(this.name, Ci.nsIFile, lf);
        break;
      }
      default:
        this._reportUnknownType();
      }
      if (!this.batching) {
        Services.prefs.savePrefFile(null);
      }
      return val;
    }
  }

  return Preferences;
}.bind({})());
