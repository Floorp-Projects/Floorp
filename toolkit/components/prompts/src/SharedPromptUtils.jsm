this.EXPORTED_SYMBOLS = [ "PromptUtils" ];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

this.PromptUtils = {
    // Fire a dialog open/close event. Used by tabbrowser to focus the
    // tab which is triggering a prompt.
    // For remote dialogs, we pass in a different DOM window and a separate
    // target. If the caller doesn't pass in the target, then we'll simply use
    // the passed-in DOM window.
    fireDialogEvent : function (domWin, eventName, maybeTarget) {
        let target = maybeTarget || domWin;
        let event = domWin.document.createEvent("Events");
        event.initEvent(eventName, true, true);
        let winUtils = domWin.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIDOMWindowUtils);
        winUtils.dispatchEventToChromeOnly(target, event);
    },

    objectToPropBag : function (obj) {
        let bag = Cc["@mozilla.org/hash-property-bag;1"].
                  createInstance(Ci.nsIWritablePropertyBag2);
        bag.QueryInterface(Ci.nsIWritablePropertyBag);

        for (let propName in obj)
            bag.setProperty(propName, obj[propName]);

        return bag;
    },

    propBagToObject : function (propBag, obj) {
        // Here we iterate over the object's original properties, not the bag
        // (ie, the prompt can't return more/different properties than were
        // passed in). This just helps ensure that the caller provides default
        // values, lest the prompt forget to set them.
        for (let propName in obj)
            obj[propName] = propBag.getProperty(propName);
    },
};
