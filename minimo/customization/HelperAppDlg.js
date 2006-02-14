
const nsIFilePicker = Components.interfaces.nsIFilePicker;
const nsIHelperAppLauncherDialog = Components.interfaces.nsIHelperAppLauncherDialog;
const nsISupports = Components.interfaces.nsISupports;


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

        const prefSvcContractID = "@mozilla.org/preferences-service;1";
        const prefSvcIID = Components.interfaces.nsIPrefService;
        const nsIFile = Components.interfaces.nsIFile;

        var folderDirFile = Components.classes["@mozilla.org/file/local;1"].createInstance(nsIFile);

        var branch = Components.classes[prefSvcContractID].getService(prefSvcIID)
                                                          .getBranch("browser.download.");
        var dir = null;

        const nsILocalFile = Components.interfaces.nsILocalFile;
        const kDownloadDirPref = "dir";
        // Try and pull in download directory pref
        try {

            var dirStringPath=branch.getCharPref(kDownloadDirPref);
            var localFileDir = folderDirFile.QueryInterface(nsILocalFile);
            dir = localFileDir.initWithPath(dirStringPath);

            //dir = branch.getComplexValue(kDownloadDirPref, nsILocalFile);
            
        } catch (e) { }

        if (dir && dir.exists())
        {
            if (aDefaultFile == "")
                aDefaultFile = "download";

            dir.append(aDefaultFile);
            return uniqueFile(dir);
        }

        // Use file picker to show dialog.
        var picker = Components.classes[ "@mozilla.org/filepicker;1" ]
                               .createInstance( nsIFilePicker );

        var parent = aContext.QueryInterface( Components.interfaces.nsIInterfaceRequestor )
                             .getInterface( Components.interfaces.nsIDOMWindowInternal );

        picker.init( parent, null, nsIFilePicker.modeSave );
        picker.defaultString = aDefaultFile;

        if (aSuggestedFileExtension) {
            // aSuggestedFileExtension includes the period, so strip it
            picker.defaultExtension = aSuggestedFileExtension.substring(1);
        } else {
            try {
                picker.defaultExtension = aLauncher.MIMEInfo.primaryExtension;
            } catch (ex) {
            }
        }

        var wildCardExtension = "*";
        if ( aSuggestedFileExtension ) {
            wildCardExtension += aSuggestedFileExtension;
            picker.appendFilter( wildCardExtension, wildCardExtension );
        }

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

