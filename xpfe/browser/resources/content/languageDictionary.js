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
 * The Original Code is Eric Hodel's <drbrain@segment7.net> code.
 *
 * The Initial Developer of the Original Code is
 * Eric Hodel.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Christopher Hoess <choess@force.stwing.upenn.edu>
 *      Tim Taylor <tim@tool-man.org>
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

/*
 * LanguageDictionary is a Singleton for looking up a language name
 * given its language code.
 */
function LanguageDictionary() {
  this.dictionary = null;
}

LanguageDictionary.prototype = {
  lookupLanguageName: function(languageCode) 
  {
    if (this.getDictionary()[languageCode])
      return this.getDictionary()[languageCode];
    else
      // XXX: handle non-standard language code's per
      //    hixie's spec (see bug 2800)
      return "";
  },

  getDictionary: function() 
  {
    if (!this.dictionary)
      this.dictionary = LanguageDictionary.createDictionary();

    return this.dictionary;
  }

}

LanguageDictionary.createDictionary = function() 
{
  var dictionary = new Array();

  var e = LanguageDictionary.getLanguageNames().getSimpleEnumeration();
  while (e.hasMoreElements()) {
    var property = e.getNext();
    property = property.QueryInterface(
        Components.interfaces.nsIPropertyElement);
    dictionary[property.getKey()] = property.getValue();
  }

  return dictionary;
}

LanguageDictionary.getLanguageNames = function() 
{
  return srGetStrBundle(
      "chrome://global/locale/languageNames.properties");
}

const languageDictionary = new LanguageDictionary();