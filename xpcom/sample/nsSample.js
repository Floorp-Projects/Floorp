/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

/**
 * We set up a sample component. The constructor is empty, all the interesting
 * stuff goes in the prototype.
 */
function mySample() { }

mySample.prototype = {
    /**
     * .classID is required for generateNSGetFactory to work correctly.
     * Make sure this CID matches the "component" in your .manifest file.
     */
    classID: Components.ID("{dea98e50-1dd1-11b2-9344-8902b4805a2e}"),

    /**
     * .classDescription and .contractID are only used for
     * backwards compatibility with Gecko 1.9.2 and
     * XPCOMUtils.generateNSGetModule.
     */
    classDescription: "nsSample: JS version", // any human-readable string
    contractID: "@mozilla.org/jssample;1",

    /**
     * List all the interfaces your component supports.
     * @note nsISupports is generated automatically; you don't need to list it.
     */
    QueryInterface: XPCOMUtils.generateQI([Components.interfaces.nsISample]),

    /*
     * get and set are new Magic in JS1.5, borrowing the intent -- if not
     * the exact syntax -- from the JS2 design.  They define accessors for
     * properties on the JS object, follow the expected rules for prototype
     * delegation, and make a mean cup of coffee.
     */
    get value()       { return this.val; },
    set value(newval) { return this.val = newval; },

    writeValue: function (aPrefix) {
        debug("mySample::writeValue => " + aPrefix + this.val + "\n");
    },
    poke: function (aValue) { this.val = aValue; },

    val: "<default value>"
};

/**
 * XPCOMUtils.generateNSGetFactory was introduced in Mozilla 2 (Firefox 4).
 * XPCOMUtils.generateNSGetModule is for Mozilla 1.9.2 (Firefox 3.6).
 */
if (XPCOMUtils.generateNSGetFactory)
    this.NSGetFactory = XPCOMUtils.generateNSGetFactory([mySample]);
else
    var NSGetModule = XPCOMUtils.generateNSGetModule([mySample]);
