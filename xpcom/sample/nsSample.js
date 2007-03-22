/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 * No magic constructor behaviour, as is de rigeur for XPCOM.
 * If you must perform some initialization, and it could possibly fail (even
 * due to an out-of-memory condition), you should use an Init method, which
 * can convey failure appropriately (thrown exception in JS,
 * NS_FAILED(nsresult) return in C++).
 *
 * In JS, you can actually cheat, because a thrown exception will cause the
 * CreateInstance call to fail in turn, but not all languages are so lucky.
 * (Though ANSI C++ provides exceptions, they are verboten in Mozilla code
 * for portability reasons -- and even when you're building completely
 * platform-specific code, you can't throw across an XPCOM method boundary.)
 */
function mySample() { /* big comment for no code, eh? */ }

/* decorate prototype to provide ``class'' methods and property accessors */
mySample.prototype = {
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

    QueryInterface: function (iid) {
        if (iid.equals(Components.interfaces.nsISample) ||
            iid.equals(Components.interfaces.nsISupports))
            return this;

        Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
        return null;
    },

    val: "<default value>"
}

var myModule = {
    firstTime: true,

    /*
     * RegisterSelf is called at registration time (component installation
     * or the only-until-release startup autoregistration) and is responsible
     * for notifying the component manager of all components implemented in
     * this module.  The fileSpec, location and type parameters are mostly
     * opaque, and should be passed on to the registerComponent call
     * unmolested.
     */
    registerSelf: function (compMgr, fileSpec, location, type) {
        if (this.firstTime) {
            debug("*** Deferring registration of sample JS components\n");
            this.firstTime = false;
            throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
        }
        debug("*** Registering sample JS components\n");
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        compMgr.registerFactoryLocation(this.myCID,
                                        "Sample JS Component",
                                        this.myProgID,
                                        fileSpec,
                                        location,
                                        type);
    },

    /*
     * The GetClassObject method is responsible for producing Factory objects
     */
    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.myCID))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        return this.myFactory;
    },

    /* CID for this class */
    myCID: Components.ID("{dea98e50-1dd1-11b2-9344-8902b4805a2e}"),

    /* ProgID for this class */
    myProgID: "@mozilla.org/jssample;1",

    /* factory object */
    myFactory: {
        /*
         * Construct an instance of the interface specified by iid, possibly
         * aggregating it with the provided outer.  (If you don't know what
         * aggregation is all about, you don't need to.  It reduces even the
         * mightiest of XPCOM warriors to snivelling cowards.)
         */
        createInstance: function (outer, iid) {
            debug("CI: " + iid + "\n");
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;

            return (new mySample()).QueryInterface(iid);
        }
    },

    /*
     * The canUnload method signals that the component is about to be unloaded.
     * C++ components can return false to indicate that they don't wish to be
     * unloaded, but the return value from JS components' canUnload is ignored:
     * mark-and-sweep will keep everything around until it's no longer in use,
     * making unconditional ``unload'' safe.
     *
     * You still need to provide a (likely useless) canUnload method, though:
     * it's part of the nsIModule interface contract, and the JS loader _will_
     * call it.
     */
    canUnload: function(compMgr) {
        debug("*** Unloading sample JS components\n");
        return true;
    }
};

function NSGetModule(compMgr, fileSpec) {
    return myModule;
}


