
const nsIFilePicker = Components.interfaces.nsIFilePicker;
const nsIHelperAppLauncherDialog = Components.interfaces.nsIHelperAppLauncherDialog;
const nsISupports = Components.interfaces.nsISupports;

function DLProgressListener() {}


function HelperAppDlg() {
}

HelperAppDlg.prototype = {

    QueryInterface: function (iid) {
        if (iid.equals(nsIHelperAppLauncherDialog) ||
            iid.equals(nsISupports))
            return this;

        Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
        return null;
    },

    show: function(aLauncher, aContext, aReason)  {
        aLauncher.saveToDisk( null, false );
    },
    
    promptForSaveToFile: function(aLauncher, aContext, aDefaultFile, aSuggestedFileExtension) {
        var result = "";

        // Use file picker to show dialog.
        var picker = Components.classes[ "@mozilla.org/filepicker;1" ]
                               .createInstance( nsIFilePicker );

        var parent = aContext.QueryInterface( Components.interfaces.nsIInterfaceRequestor )
                             .getInterface( Components.interfaces.nsIDOMWindowInternal );

        picker.init( parent, null, nsIFilePicker.modeSave );
        picker.defaultString = aDefaultFile;
        picker.appendFilters( nsIFilePicker.filterAll );

        if (picker.show() == nsIFilePicker.returnCancel || !picker.file) {
            return null;
        }

        return picker.file;
    },
}


var module = {
    firstTime: true,

    // registerSelf: Register this component.
    registerSelf: function (compMgr, fileSpec, location, type) {
        if (this.firstTime) {
            this.firstTime = false;
            throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
        }
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

        compMgr.registerFactoryLocation( this.cid,
                                         "Mozilla Helper App Launcher Dialog",
                                         this.contractId,
                                         fileSpec,
                                         location,
                                         type );
    },

    // getClassObject: Return this component's factory object.
    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.cid)) {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        if (!iid.equals(Components.interfaces.nsIFactory)) {
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
        }

        return this.factory;
    },

    /* CID for this class */
    cid: Components.ID("{494cf5f6-c473-46ea-ad70-230ad7a0fe64}"),

    /* Contract ID for this class */
    contractId: "@mozilla.org/helperapplauncherdialog;1",

    /* factory object */
    factory: {
        // createInstance: Return a new nsProgressDialog object.
        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;

            return (new HelperAppDlg()).QueryInterface(iid);
        }
    },

    // canUnload: n/a (returns true)
    canUnload: function(compMgr) {
        return true;
    }
};

// NSGetModule: Return the nsIModule object.
function NSGetModule(compMgr, fileSpec) {
    return module;
}

