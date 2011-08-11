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
 * The Original Code is Mozilla XUL Toolkit Testing Code.
 *
 * The Initial Developer of the Original Code is
 * Paolo Amadini <http://www.amadzone.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009
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

/**
 * Allows registering a mock XPCOM component, that temporarily replaces the
 *  original one when an object implementing a given ContractID is requested
 *  using createInstance.
 *
 * @param aContractID
 *        The ContractID of the component to replace, for example
 *        "@mozilla.org/filepicker;1".
 *
 * @param aReplacementCtor
 *        The constructor function for the JavaScript object that will be
 *        created every time createInstance is called. This object must
 *        implement QueryInterface and provide the XPCOM interfaces required by
 *        the specified ContractID (for example
 *        Components.interfaces.nsIFilePicker).
 */

function MockObjectRegisterer(aContractID, aReplacementCtor) {
  this._contractID = aContractID;
  this._replacementCtor = aReplacementCtor;
}

MockObjectRegisterer.prototype = {
  /**
   * Replaces the current factory with one that returns a new mock object.
   *
   * After register() has been called, it is mandatory to call unregister() to
   * restore the original component. Usually, you should use a try-catch block
   * to ensure that unregister() is called.
   */
  register: function MOR_register() {
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
    if (this._originalFactory)
      throw new Exception("Invalid object state when calling register()");

    // Define a factory that creates a new object using the given constructor.
    var providedConstructor = this._replacementCtor;
    this._mockFactory = {
      createInstance: function MF_createInstance(aOuter, aIid) {
        if (aOuter != null)
          throw Components.results.NS_ERROR_NO_AGGREGATION;
        return new providedConstructor().QueryInterface(aIid);
      }
    };

    var componentRegistrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);

    // Get the component CID.
    this._cid = componentRegistrar.contractIDToCID(this._contractID);

    // ... and make sure we correctly replace the original factory with the mock one.
    this._originalFactory = Components.manager.getClassObject(Components.classes[this._contractID],
                                                              Components.interfaces.nsIFactory);

    componentRegistrar.unregisterFactory(this._cid, this._originalFactory);

    componentRegistrar.registerFactory(this._cid,
                                       "",
                                       this._contractID,
                                       this._mockFactory);
  },

  /**
   * Restores the original factory.
   */
  unregister: function MOR_unregister() {
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
    if (!this._originalFactory)
      throw new Exception("Invalid object state when calling unregister()");

    // Free references to the mock factory.
    var componentRegistrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    componentRegistrar.unregisterFactory(this._cid,
                                         this._mockFactory);

    // Restore the original factory.
    componentRegistrar.registerFactory(this._cid,
                                       "",
                                       this._contractID,
                                       this._originalFactory);

    // Allow registering a mock factory again later.
    this._cid = null;
    this._originalFactory = null;
    this._mockFactory = null;
  },

  // --- Private methods and properties ---

  /**
   * The factory of the component being replaced.
   */
  _originalFactory: null,

  /**
   * The CID under which the mock contractID was registered.
   */
  _cid: null,

  /**
   * The nsIFactory that was automatically generated by this object.
   */
  _mockFactory: null
}
