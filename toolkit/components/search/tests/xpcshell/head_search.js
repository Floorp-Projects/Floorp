/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is POTI Inc.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
const XULAPPINFO_CID = Components.ID("{c763b610-9d49-455a-bbd2-ede71682a1ac}");

var gXULAppInfo = null;

/**
 * Creates an nsIXULAppInfo
 * @param   id
 *          The ID of the test application
 * @param   name
 *          A name for the test application
 * @param   version
 *          The version of the application
 * @param   platformVersion
 *          The gecko version of the application
 */
function createAppInfo(id, name, version, platformVersion)
{
  gXULAppInfo = {
    vendor: "Mozilla",
    name: name,
    ID: id,
    version: version,
    appBuildID: "2007010101",
    platformVersion: platformVersion,
    platformBuildID: "2007010101",
    inSafeMode: false,
    logConsoleErrors: true,
    OS: "XPCShell",
    XPCOMABI: "noarch-spidermonkey",
    invalidateCachesOnRestart: function invalidateCachesOnRestart() {},

    QueryInterface: function QueryInterface(iid) {
      if (iid.equals(Components.interfaces.nsIXULAppInfo)
       || iid.equals(Components.interfaces.nsIXULRuntime)
       || iid.equals(Components.interfaces.nsISupports))
        return this;

      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  };

  var XULAppInfoFactory = {
    createInstance: function (outer, iid) {
      if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      return gXULAppInfo.QueryInterface(iid);
    }
  };
  var registrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
  registrar.registerFactory(XULAPPINFO_CID, "XULAppInfo",
                            XULAPPINFO_CONTRACTID, XULAppInfoFactory);
}

// Need to create and register a profile folder.
var gProfD = do_get_profile();

