/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

function mySample() {
    /* use this to execute code a la C++'s SetValue */
    this.value getter= function () {
        dump ("Getting value\n");
        return this.val;
    };

    this.value setter= function (newval) {
        dump("Setting value to " + newval + "\n");
        return this.val = newval;
    };
}

mySample.prototype.val = "<default value>";
mySample.prototype.writeValue = function (aPrefix) {
    dump("mySample::writeValue => " + aPrefix + this.val + "\n");
}
mySample.prototype.poke = function (aValue) {
    this.val = aValue;
}
/* until bug 14460 is fixed, must use QueryInterface, not queryInterface */
mySample.prototype.QueryInterface = function(iid) {
    if (iid.equals(Components.interfaces.nsISample) ||
        iid.equals(Components.interfaces.nsISupports))
        return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
}

var myModule = {
    RegisterSelf:function (compMgr, fileSpec, location) {
        dump("RegisterSelf(" + Array.prototype.join.apply(arguments, ",") + 
             ")\n");
        try {
            compMgr.registerComponentWithType(this.myCID,
                                              "Sample JS Component",
                                              "mozilla.jssample.1", fileSpec,
                                              location, true, true, 
                                              "text/javascript");
        } catch (e) { 
            dump(" FAILURE[" + e + "]");
        }
        dump("RegisterSelf done\n");
    },

    GetClassObject:function (compMgr, cid, iid) {
        dump("GetClassObject(" + Array.prototype.join.apply(arguments, ",") +
             ")\n");

        if (!cid.equals(this.myCID)) {
            dump("GetClassObject: CID mismatch:\nmine:\t" + this.myCID +
                 "\nin:\t" + cid + "\n");
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        dump("GetClassObject: have right CID\n");
        if (iid.equals(Components.interfaces.nsIFactory)) {
            dump("JS: looking for factory (" + iid + ")\n");
            return this.myFactory;
        }
        dump("GetClassObject: IID mismatch:\nfact:\t" +
             Components.interfaces.nsIFactory + "\nin:\t" +
             iid + "\n");
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    },

    /* CID for this class */
    myCID:Components.ID("{dea98e50-1dd1-11b2-9344-8902b4805a2e}"),

    /* factory object */
    myFactory:{
        CreateInstance:function (outer, iid) {
            dump ("JS: CreateInstance(" 
                  + Array.prototype.join.apply(arguments, ",") + ")\n");
            if (outer != null) {
                throw Components.results.NS_ERROR_NO_AGGREGATION;
            }
            return new mySample();
        }
    }
};
    
function NSGetModule(compMgr, fileSpec) {
    return myModule;
}
