/* vim:set ts=2 sw=2 sts=2 et cin: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

const SIMPLETEST_CONTRACTID  = "@test.mozilla.org/simple-test;1?impl=js";
const SIMPLETEST_CLASSID     = Components.ID("{4177e257-a0dc-49b9-a774-522a000a49fa}");

function SimpleTest() {
}

SimpleTest.prototype = {
  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsISimpleTest) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  add: function(a, b) {
    dump("add(" + a + "," + b + ") from JS\n");
    return a + b;
  }
};

var Module = {
  _classes: {
    simpletest: {
      classID     : SIMPLETEST_CLASSID,
      contractID  : SIMPLETEST_CONTRACTID,
      className   : "SimpleTest",
      factory     : {
        createInstance: function(delegate, iid) {
          if (delegate)
            throw Components.results.NS_ERROR_NO_AGGREGATION;
          return new SimpleTest().QueryInterface(iid);
        }
      }
    }
  },

  registerSelf: function(compMgr, fileSpec, location, type)
  {
    var reg = compMgr.QueryInterface(
        Components.interfaces.nsIComponentRegistrar);

    for (var key in this._classes) {
      var c = this._classes[key];
      reg.registerFactoryLocation(c.classID, c.className, c.contractID,
                                  fileSpec, location, type);
    }
  },
                                                                                                        
  getClassObject: function(compMgr, cid, iid)
  {
    if (!iid.equals(Components.interfaces.nsIFactory))
      throw Components.results.NS_ERROR_NO_INTERFACE;

    for (var key in this._classes) {
      var c = this._classes[key];
      if (cid.equals(c.classID))
        return c.factory;
    }

    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },
                                                                                                        
  canUnload: function (aComponentManager)
  {
    return true;
  }
};

function NSGetModule(compMgr, fileSpec) {
  return Module;
}
