/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  const time = (new Date("Jan 1, 2030")).getTime() / 1000;
  var cookie = {
    name: "foo",
    value: "bar",
    isDomain: true,
    host: "example.com",
    path: "/baz",
    isSecure: false,
    expires: time,
    status: 0,
    policy: 0,
    isSession: false,
    expiry: time,
    isHttpOnly: true,
    QueryInterface: function(iid) {
      const validIIDs = [Components.interfaces.nsISupports,
                         Components.interfaces.nsICookie,
                         Components.interfaces.nsICookie2];
      for (var i = 0; i < validIIDs.length; ++i)
        if (iid == validIIDs[i])
          return this;
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  };
  var cm = Components.classes["@mozilla.org/cookiemanager;1"].
           getService(Components.interfaces.nsICookieManager2);
  do_check_false(cm.cookieExists(cookie));
  // if the above line does not crash, the test was successful
  do_test_finished();
}
