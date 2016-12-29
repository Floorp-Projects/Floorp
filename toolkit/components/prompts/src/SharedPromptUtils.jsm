this.EXPORTED_SYMBOLS = [ "PromptUtils", "EnableDelayHelper" ];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

this.PromptUtils = {
    // Fire a dialog open/close event. Used by tabbrowser to focus the
    // tab which is triggering a prompt.
    // For remote dialogs, we pass in a different DOM window and a separate
    // target. If the caller doesn't pass in the target, then we'll simply use
    // the passed-in DOM window.
    // The detail may contain information about the principal on which the
    // prompt is triggered, as well as whether or not this is a tabprompt
    // (ie tabmodal alert/prompt/confirm and friends)
    fireDialogEvent(domWin, eventName, maybeTarget, detail) {
        let target = maybeTarget || domWin;
        let eventOptions = {cancelable: true, bubbles: true};
        if (detail) {
          eventOptions.detail = detail;
        }
        let event = new domWin.CustomEvent(eventName, eventOptions);
        let winUtils = domWin.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIDOMWindowUtils);
        winUtils.dispatchEventToChromeOnly(target, event);
    },

    objectToPropBag(obj) {
        let bag = Cc["@mozilla.org/hash-property-bag;1"].
                  createInstance(Ci.nsIWritablePropertyBag2);
        bag.QueryInterface(Ci.nsIWritablePropertyBag);

        for (let propName in obj)
            bag.setProperty(propName, obj[propName]);

        return bag;
    },

    propBagToObject(propBag, obj) {
        // Here we iterate over the object's original properties, not the bag
        // (ie, the prompt can't return more/different properties than were
        // passed in). This just helps ensure that the caller provides default
        // values, lest the prompt forget to set them.
        for (let propName in obj)
            obj[propName] = propBag.getProperty(propName);
    },
};

/**
 * This helper handles the enabling/disabling of dialogs that might
 * be subject to fast-clicking attacks. It handles the initial delayed
 * enabling of the dialog, as well as disabling it on blur and reapplying
 * the delay when the dialog regains focus.
 *
 * @param enableDialog   A custom function to be called when the dialog
 *                       is to be enabled.
 * @param diableDialog   A custom function to be called when the dialog
 *                       is to be disabled.
 * @param focusTarget    The window used to watch focus/blur events.
 */
this.EnableDelayHelper = function({enableDialog, disableDialog, focusTarget}) {
    this.enableDialog = makeSafe(enableDialog);
    this.disableDialog = makeSafe(disableDialog);
    this.focusTarget = focusTarget;

    this.disableDialog();

    this.focusTarget.addEventListener("blur", this, false);
    this.focusTarget.addEventListener("focus", this, false);
    this.focusTarget.document.addEventListener("unload", this, false);

    this.startOnFocusDelay();
};

this.EnableDelayHelper.prototype = {
    get delayTime() {
        return Services.prefs.getIntPref("security.dialog_enable_delay");
    },

    handleEvent(event) {
        if (event.target != this.focusTarget &&
            event.target != this.focusTarget.document)
            return;

        switch (event.type) {
            case "blur":
                this.onBlur();
                break;

            case "focus":
                this.onFocus();
                break;

            case "unload":
                this.onUnload();
                break;
        }
    },

    onBlur() {
        this.disableDialog();
        // If we blur while waiting to enable the buttons, just cancel the
        // timer to ensure the delay doesn't fire while not focused.
        if (this._focusTimer) {
            this._focusTimer.cancel();
            this._focusTimer = null;
        }
    },

    onFocus() {
        this.startOnFocusDelay();
    },

    onUnload() {
        this.focusTarget.removeEventListener("blur", this, false);
        this.focusTarget.removeEventListener("focus", this, false);
        this.focusTarget.document.removeEventListener("unload", this, false);

        if (this._focusTimer) {
            this._focusTimer.cancel();
            this._focusTimer = null;
        }

        this.focusTarget = this.enableDialog = this.disableDialog = null;
    },

    startOnFocusDelay() {
        if (this._focusTimer)
            return;

        this._focusTimer = Cc["@mozilla.org/timer;1"]
                             .createInstance(Ci.nsITimer);
        this._focusTimer.initWithCallback(
            () => { this.onFocusTimeout(); },
            this.delayTime,
            Ci.nsITimer.TYPE_ONE_SHOT
        );
    },

    onFocusTimeout() {
        this._focusTimer = null;
        this.enableDialog();
    },
};

function makeSafe(fn) {
    return function() {
        // The dialog could be gone by now (if the user closed it),
        // which makes it likely that the given fn might throw.
        try {
            fn();
        } catch (e) { }
    };
}
