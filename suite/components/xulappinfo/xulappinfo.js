const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
const XULAPPINFO_CID = Components.ID("{f8b59582-5808-4e6e-a467-339e9c82756f}");

const nsIXULAppInfo = Components.interfaces.nsIXULAppInfo;
const nsIComponentRegistrar = Components.interfaces.nsIComponentRegistrar;
const nsIFactory = Components.interfaces.nsIFactory;

function XULAppInfoService()
{
}

XULAppInfoService.prototype.vendor = "mozilla.org";
XULAppInfoService.prototype.ID = "{92650c4d-4b8e-4d2a-b7eb-24ecf4f6b63a}";
#expand XULAppInfoService.prototype.name = "__MOZ_APP_DISPLAYNAME__";
#expand XULAppInfoService.prototype.version = "__MOZ_APP_VERSION__";
#expand XULAppInfoService.prototype.appBuildID = "__BUILD_ID__";
#expand XULAppInfoService.prototype.platformVersion = "__TOOLKIT_EM_VERSION__";
#expand XULAppInfoService.prototype.platformBuildID = "__BUILD_ID__";

XULAppInfoService.prototype.QueryInterface =
function appinfo_QueryInterface(iid)
{
    if (!iid.equals(nsIXULAppInfo) &&
        !iid.equals(nsISupports))
    {
        throw Components.results.NS_ERROR_NO_INTERFACE;
    }

    return this;
}

var XULAppInfoFactory = new Object();

XULAppInfoFactory.createInstance =
function(outer, iid)
{
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsIXULAppInfo) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new XULAppInfoService();
}


var XULAppInfoModule = new Object();

XULAppInfoModule.registerSelf =
function mod_registerSelf(compMgr, fileSpec, location, type)
{
    compMgr = compMgr.QueryInterface(nsIComponentRegistrar);

    compMgr.registerFactoryLocation(XULAPPINFO_CID,
                                    "XUL AppInfo service",
                                    XULAPPINFO_CONTRACTID,
                                    fileSpec, location, type);
}

XULAppInfoModule.unregisterSelf =
function mod_unregisterSelf(compMgr, fileSpec, location)
{
}

XULAppInfoModule.getClassObject =
function mod_getClassObject(compMgr, cid, iid)
{
    if (cid.equals(XULAPPINFO_CID))
        return XULAppInfoFactory;

    if (!iid.equals(nsIFactory))
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    throw Components.results.NS_ERROR_NO_INTERFACE;
}

XULAppInfoModule.canUnload =
function mod_canUnload(compMgr)
{
    return true;
}

/* entrypoint */
function NSGetModule(compMgr, fileSpec)
{
    return XULAppInfoModule;
}
