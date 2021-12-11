/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["EnterprisePolicies"];

function EnterprisePolicies() {
  // eslint-disable-next-line mozilla/use-services
  const appinfo = Cc["@mozilla.org/xre/app-info;1"].getService(
    Ci.nsIXULRuntime
  );
  if (appinfo.processType == appinfo.PROCESS_TYPE_DEFAULT) {
    const { EnterprisePoliciesManager } = ChromeUtils.import(
      "resource://gre/modules/EnterprisePoliciesParent.jsm"
    );
    return new EnterprisePoliciesManager();
  }
  const { EnterprisePoliciesManagerContent } = ChromeUtils.import(
    "resource://gre/modules/EnterprisePoliciesContent.jsm"
  );
  return new EnterprisePoliciesManagerContent();
}
