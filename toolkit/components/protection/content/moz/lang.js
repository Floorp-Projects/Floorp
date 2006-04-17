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
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Boodman <aa@google.com> (original author)
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


// Firefox-specific additions to lib/js/lang.js.


/**
 * The always-useful alert. 
 */
function alert(msg, opt_title) {
  opt_title |= "message";

  Cc["@mozilla.org/embedcomp/prompt-service;1"]
    .getService(Ci.nsIPromptService)
    .alert(null, opt_title, msg.toString());
}


/**
 * The instanceof operator cannot be used on a pure js object to determine if 
 * it implements a certain xpcom interface. The QueryInterface method can, but
 * it throws an error which makes things more complex.
 */
function jsInstanceOf(obj, iid) {
  try {
    obj.QueryInterface(iid);
    return true;
  } catch (e) {
    if (e == Components.results.NS_ERROR_NO_INTERFACE) {
      return false;
    } else {
      throw e;
    }
  }
}


/**
 * Unbelievably, Function inheritence is broken in chrome in Firefox
 * (still as of FFox 1.5b1). Hence if you're working in an extension
 * and not using the subscriptloader, you can't use the method
 * above. Instead, use this global function that does roughly the same
 * thing.
 *
 ***************************************************************************
 *   NOTE THE REVERSED ORDER OF FUNCTION AND OBJECT REFERENCES AS bind()   *
 ***************************************************************************
 * 
 * // Example to bind foo.bar():
 * var bound = BindToObject(bar, foo, "arg1", "arg2");
 * bound("arg3", "arg4");
 * 
 * @param func {string} Reference to the function to be bound
 *
 * @param obj {object} Specifies the object which |this| should point to
 * when the function is run. If the value is null or undefined, it will default
 * to the global object.
 *
 * @param opt_{...} Dummy optional arguments to make a jscompiler happy
 *
 * @returns {function} A partially-applied form of the speficied function.
 */
function BindToObject(func, obj, opt_A, opt_B, opt_C, opt_D, opt_E, opt_F) {
  // This is the sick product of Aaron's mind. Not for the feint of heart.
  var args = Array.prototype.splice.call(arguments, 1, arguments.length);
  return Function.prototype.bind.apply(func, args);
}
