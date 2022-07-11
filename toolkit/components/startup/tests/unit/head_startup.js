/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const XULRUNTIME_CONTRACTID = "@mozilla.org/xre/runtime;1";
const XULRUNTIME_CID = Components.ID("7685dac8-3637-4660-a544-928c5ec0e714}");

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

var gAppInfo = null;

function createAppInfo(ID, name, version, platformVersion = "1.0") {
  let { newAppInfo } = ChromeUtils.import(
    "resource://testing-common/AppInfo.jsm"
  );
  gAppInfo = newAppInfo({
    ID,
    name,
    version,
    platformVersion,
    crashReporter: true,
    replacedLockTime: 0,
  });

  let XULAppInfoFactory = {
    createInstance(iid) {
      return gAppInfo.QueryInterface(iid);
    },
  };

  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(
    XULRUNTIME_CID,
    "XULRuntime",
    XULRUNTIME_CONTRACTID,
    XULAppInfoFactory
  );
}
