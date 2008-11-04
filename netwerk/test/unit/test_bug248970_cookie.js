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
 * The Original Code is Private Browsing Tests.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

var _PBSvc = null;
function get_PBSvc() {
  if (_PBSvc)
    return _PBSvc;

  try {
    _PBSvc = Components.classes["@mozilla.org/privatebrowsing;1"].
             getService(Components.interfaces.nsIPrivateBrowsingService);
    return _PBSvc;
  } catch (e) {}
  return null;
}

var _CMSvc = null;
function get_CookieManager() {
  if (_CMSvc)
    return _CMSvc;

  return _CMSvc = Components.classes["@mozilla.org/cookiemanager;1"].
                  getService(Components.interfaces.nsICookieManager2);
}

function is_cookie_available1(domain, path, name, value,
                              secure, httponly, session, expires) {
  var cm = get_CookieManager();
  var enumerator = cm.enumerator;
  while (enumerator.hasMoreElements()) {
    var cookie = enumerator.getNext().QueryInterface(Components.interfaces.nsICookie);
    if (cookie.host == domain &&
        cookie.path == path &&
        cookie.name == name &&
        cookie.value == value &&
        cookie.isSecure == secure &&
        cookie.expires == expires)
      return true;
  }
  return false;
}

function is_cookie_available2(domain, path, name, value,
                              secure, httponly, session, expires) {
  var cookie = {
    name: name,
    value: value,
    isDomain: true,
    host: domain,
    path: path,
    isSecure: secure,
    expires: expires,
    status: 0,
    policy: 0,
    isSession: session,
    expiry: expires,
    isHttpOnly: httponly,
    QueryInterface: function(iid) {
      var validIIDs = [Components.interfaces.nsISupports,
                       Components.interfaces.nsICookie,
                       Components.interfaces.nsICookie2];
      for (var i = 0; i < validIIDs.length; ++i) {
        if (iid == validIIDs[i])
          return this;
      }
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  };

  var cm = get_CookieManager();
  return cm.cookieExists(cookie);
}

var cc_observer = null;
function setup_cookie_changed_observer() {
  cc_observer = {
    gotReloaded: false,
    QueryInterface: function (iid) {
      const interfaces = [Components.interfaces.nsIObserver,
                          Components.interfaces.nsISupports];
      if (!interfaces.some(function(v) iid.equals(v)))
        throw Components.results.NS_ERROR_NO_INTERFACE;
      return this;
    },
    observe: function (subject, topic, data) {
      if (topic == "cookie-changed") {
        if (!subject) {
          if (data == "reload")
            this.gotReloaded = true;
        }
      }
    }
  };
  var os = Components.classes["@mozilla.org/observer-service;1"].
           getService(Components.interfaces.nsIObserverService);
  os.addObserver(cc_observer, "cookie-changed", false);
}

function run_test() {
  var pb = get_PBSvc();
  if (pb) { // Private Browsing might not be available
    var prefBranch = Components.classes["@mozilla.org/preferences-service;1"].
                     getService(Components.interfaces.nsIPrefBranch);
    prefBranch.setBoolPref("browser.privatebrowsing.keep_current_session", true);

    var cm = get_CookieManager();
    do_check_neq(cm, null);

    setup_cookie_changed_observer();
    do_check_neq(cc_observer, null);

    try {
      // create Cookie-A
      const time = (new Date("Jan 1, 2030")).getTime() / 1000;
      cm.add("pbtest.example.com", "/", "C1", "V1", false, true, false, time);
      // make sure Cookie-A is retrievable
      do_check_true(is_cookie_available1("pbtest.example.com", "/", "C1", "V1", false, true, false, time));
      do_check_true(is_cookie_available2("pbtest.example.com", "/", "C1", "V1", false, true, false, time));
      // enter private browsing mode
      pb.privateBrowsingEnabled = true;
      // make sure the "cleared" notification was fired
      do_check_true(cc_observer.gotReloaded);
      cc_observer.gotReloaded = false;
      // make sure Cookie-A is not retrievable
      do_check_false(is_cookie_available1("pbtest.example.com", "/", "C1", "V1", false, true, false, time));
      do_check_false(is_cookie_available2("pbtest.example.com", "/", "C1", "V1", false, true, false, time));
      // create Cookie-B
      const time2 = (new Date("Jan 2, 2030")).getTime() / 1000;
      cm.add("pbtest2.example.com", "/", "C2", "V2", false, true, false, time2);
      // make sure Cookie-B is retrievable
      do_check_true(is_cookie_available1("pbtest2.example.com", "/", "C2", "V2", false, true, false, time2));
      do_check_true(is_cookie_available2("pbtest2.example.com", "/", "C2", "V2", false, true, false, time2));
      // exit private browsing mode
      pb.privateBrowsingEnabled = false;
      // make sure the "reload" notification was fired
      do_check_true(cc_observer.gotReloaded);
      // make sure Cookie-B is not retrievable
      do_check_false(is_cookie_available1("pbtest2.example.com", "/", "C2", "V2", false, true, false, time2));
      do_check_false(is_cookie_available2("pbtest2.example.com", "/", "C2", "V2", false, true, false, time2));
      // make sure Cookie-A is retrievable
      do_check_true(is_cookie_available1("pbtest.example.com", "/", "C1", "V1", false, true, false, time));
      do_check_true(is_cookie_available2("pbtest.example.com", "/", "C1", "V1", false, true, false, time));
    } catch (e) {
      do_throw("Unexpected exception while testing cookies: " + e);
    }

    prefBranch.clearUserPref("browser.privatebrowsing.keep_current_session");
  }
}
