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
 * The Original Code is XPConnect Native Wrapper.
 *
 * The Initial Developer of the Original Code is
 * Christopher A. Aillon <christopher@aillon.com>.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher A. Aillon <christopher@aillon.com>
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
 * Get a wrapper to certain XPConnect wrapped native properties of an
 * untrusted object.
 *
 * Usage:
 *   var wrapper = new XPCNativeWrapper(untrusted, 'title', 'getElementById()');
 *   var title = wrapper.title;
 *   var elem  = wrapper.getElementById('fooId');
 *
 * @param aUntrustedObject the untrusted object we want native wrappers for
 * @param arg1...argN strings of the form 'property' or 'method()' which
 *                    denote the property or method names we wish to wrap
 * @throws NS_ERROR_XPC_BAD_CONVERT_JS if a property or method requested
 *                                     does not have a native wrapper on
 *                                     the untrusted object.
 * @see nsXPCComponents::LookupMethod()
 */
function XPCNativeWrapper(aUntrustedObject)
{
  this.mUntrustedObject = aUntrustedObject;

  for (var i = arguments.length - 1; i > 0; --i)
    this.importXPCNative(arguments[i]);
}

XPCNativeWrapper.prototype =
{
  importXPCNative:
    function(aName)
    {
      // If we are passed a string like "foo()", ary[2] will be of type string,
      // and contain "()".  If passed "foo", then it will be of type undefined.
      if (aName.slice(-2) == "()")
        this._doImportMethod(aName.slice(0, -2));
      else
        this._doImportProperty(aName);
    },

  _doImportMethod:
    function(aMethodName)
    {
      var nativeMethod = Components.lookupMethod(this.mUntrustedObject,
                                                 aMethodName);
      this[aMethodName] = function()
      {
        return nativeMethod.apply(this.mUntrustedObject, arguments);
      };
    },

  _doImportProperty:
    function(aPropName)
    {
      var nativeMethod = Components.lookupMethod(this.mUntrustedObject,
                                                 aPropName);
      var theGetter = function()
      {
        return nativeMethod.call(this.mUntrustedObject);
      };
      var theSetter = function(val)
      {
        return nativeMethod.call(this.mUntrustedObject, val);
      };

      this.__defineGetter__(aPropName, theGetter);
      this.__defineSetter__(aPropName, theSetter);
    }
};
