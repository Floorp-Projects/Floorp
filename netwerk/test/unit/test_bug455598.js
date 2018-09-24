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
      const validIIDs = [Ci.nsISupports,
                         Ci.nsICookie,
                         Ci.nsICookie2];
      for (var i = 0; i < validIIDs.length; ++i)
        if (iid == validIIDs[i])
          return this;
      throw Cr.NS_ERROR_NO_INTERFACE;
    }
  };
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
  Assert.ok(!cm.cookieExists(cookie.host, cookie.path, cookie.name, {}));
  // if the above line does not crash, the test was successful
  do_test_finished();
}
