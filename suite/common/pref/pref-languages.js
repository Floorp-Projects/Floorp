/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *         Adrian Havill <havill@redhat.com>
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

//GLOBALS


//locale bundles
var regionsBundle;
var languagesBundle;
var acceptedBundle;
var prefLangBundle;

//dictionary of all supported locales
var availLanguageDict;

//XUL listbox handles
var available_languages;
var active_languages;

//XUL window pref window interface object
var pref_string = new String();

//Reg expression for splitting multiple pref values
var separatorRe = /\s*,\s*/;

function GetBundles()
{
  if (!regionsBundle)    regionsBundle   = srGetStrBundle("chrome://global/locale/regionNames.properties");
  if (!languagesBundle)  languagesBundle = srGetStrBundle("chrome://global/locale/languageNames.properties");
  if (!prefLangBundle)  prefLangBundle = srGetStrBundle("chrome://communicator/locale/pref/pref-languages.properties");
  if (!acceptedBundle)   acceptedBundle  = srGetStrBundle("resource://gre/res/language.properties");
}


function Init()
{
  try {
    GetBundles();
  }

  catch(ex) {
  }

  ReadAvailableLanguages();

  if (!("arguments" in window)) {

    //no caller arguments - load base window

    try {
      active_languages              = document.getElementById('active_languages');
    } //try

    catch(ex) {
    } //catch

    try {
      parent.initPanel('chrome://communicator/content/pref/pref-languages.xul');
    }

    catch(ex) {
      //pref service backup
    } //catch

    pref_string = active_languages.getAttribute("prefvalue");
    LoadActiveLanguages();
    SelectLanguage();

  } else {

    //load available languages popup
    try {

      //add language popup
      available_languages           = document.getElementById('available_languages');
      active_languages              = window.opener.document.getElementById('active_languages');
      pref_string                   = window.opener.document.getElementById('intlAcceptLanguages').label;

    } //try

    catch(ex) {
    } //catch

    LoadAvailableLanguages();
  }
}


function AddLanguage()
{
    //cludge: make pref string available from the popup
    document.getElementById('intlAcceptLanguages').label = pref_string;
    window.openDialog("chrome://communicator/content/pref/pref-languages-add.xul","_blank","modal,chrome,centerscreen,titlebar", "addlangwindow");
    UpdateSavePrefString();
    SelectLanguage();
}


function ReadAvailableLanguages()
{
  availLanguageDict   = new Array();
  var visible = new String();
  var str = new String();
  var i =0;

  var acceptedBundleEnum = acceptedBundle.getSimpleEnumeration();

  var curItem;
  var stringName;
  var stringNameProperty;

  while (acceptedBundleEnum.hasMoreElements()) {

    //progress through the bundle
    curItem = acceptedBundleEnum.getNext();

    //"unpack" the item, nsIPropertyElement is now partially scriptable
    curItem = curItem.QueryInterface(Components.interfaces.nsIPropertyElement);

    //dump string name (key)
    stringName = curItem.key;
    stringNameProperty = stringName.split('.');

    if (stringNameProperty[1] == 'accept') {

      //dump the UI string (value)
      visible   = curItem.value;

      //if (visible == 'true') {

        str = stringNameProperty[0];
        var stringLangRegion = stringNameProperty[0].split('-');

        if (stringLangRegion[0]) {
          var tit;
          var language;
          var region;
          var use_region_format = false;

          try {
            language = languagesBundle.GetStringFromName(stringLangRegion[0]);
          }
          catch (ex) {
            language = "";
          }

          if (stringLangRegion.length > 1) {

            try {
              region = regionsBundle.GetStringFromName(stringLangRegion[1]);
              use_region_format = true;
            }
            catch (ex) {
            }
          }
          
          if (use_region_format) {
            tit = prefLangBundle.formatStringFromName("languageRegionCodeFormat",
                                                      [language, region, str], 3);
          } else {
            tit = prefLangBundle.formatStringFromName("languageCodeFormat",
                                                      [language, str], 2);
          }

        } //if language

        if (str && tit) {

          availLanguageDict[i] = new Array(3);
          availLanguageDict[i][0] = tit;
          availLanguageDict[i][1] = str;
          availLanguageDict[i][2] = visible;
          i++;

        } // if str&tit
      //} //if visible
    } //if accepted
  } //while
  availLanguageDict.sort( // sort on first element
    function (a, b) {
      if (a[0] < b[0]) return -1;
      if (a[0] > b[0]) return  1;
      return 0;
    });
}


function LoadAvailableLanguages()
{
  if (availLanguageDict)
    for (var i = 0; i < availLanguageDict.length; i++) {

    if (availLanguageDict[i][2] == 'true') {

        AddListItem(document, available_languages, availLanguageDict[i][1], availLanguageDict[i][0]);

      } //if

    } //for
}


function LoadActiveLanguages()
{
  if (pref_string) {
    var arrayOfPrefs = pref_string.split(separatorRe);

    for (var i = 0; i < arrayOfPrefs.length; i++) {
      var str = arrayOfPrefs[i];
      var tit = GetLanguageTitle(str);

      if (str) {
        if (!tit)
           tit = '[' + str + ']';
        AddListItem(document, active_languages, str, tit);

      } //if
    } //for
  }
}


function LangAlreadyActive(langId)
{
  var found = false;
  try {
    var arrayOfPrefs = pref_string.split(separatorRe);

    if (arrayOfPrefs)
      for (var i = 0; i < arrayOfPrefs.length; i++) {
      if (arrayOfPrefs[i] == langId) {
         found = true;
         break;
      }
    }

    return found;
  }

  catch(ex){
     return false;
  }
}

function isAlpha(mixedCase) {
  var allCaps = mixedCase.toUpperCase();
  for (var i = allCaps.length - 1; i >= 0; i--) {
    var c = allCaps.charAt(i);
    if (c < 'A' || c > 'Z') return false;
  }
  return true;
}

function isAlphaNum(mixedCase) {
  var allCaps = mixedCase.toUpperCase();
  for (var i = allCaps.length - 1; i >= 0; i--) {
    var c = allCaps.charAt(i);
    if ((c < 'A' || c > 'Z') && (c < '0' || c > '9')) return false;
  }
  return true;
}

function IsRFC1766LangTag(candidate) {

  /* reject bogus lang strings, INCLUDING those with HTTP "q"
     values kludged on the end of them

     Valid language codes examples:
     i.e. ja-JP-kansai (Kansai dialect of Japanese)
          en-US-texas (Texas dialect)
          i-klingon-tng (did TOS Klingons speak in non-English?)
          sgn-US-MA (Martha Vineyard's Sign Language)
  */
  var tags = candidate.split('-');

  /* if not IANA "i" or a private "x" extension, the primary
     tag should be a ISO 639 country code, two or three letters long.
     we don't check if the country code is bogus or not.
  */
  var checkedTags = 0;
  if (tags[0].toLowerCase() != "x" && tags[0].toLowerCase() != "i") {
    if (tags[0].length != 2 && tags[0].length != 3) return false;
    if (!isAlpha(tags[0])) return false;
    checkedTags++;

    /* the first subtag can be either a 2 letter ISO 3166 country code,
       or an IANA registered tag from 3 to 8 characters.
    */
    if (tags.length > 1) {
      if (tags[1].length < 2 || tags[1].length > 8) return false;
      if (!isAlphaNum(tags[1])) return false;

      /* do not allow user-assigned ISO 3166 country codes */
      if (tags[1].length == 2 && isAlpha(tags[1])) {
        var countryCode = tags[1].toUpperCase();
        if (countryCode == "AA" || countryCode == "ZZ") return false;
        if (countryCode[0] == 'X') return false;
        if (countryCode[0] == 'Q' && countryCode[1] > 'L') return false;
      }
      checkedTags++;
    }
  }
  else if (tags.length < 2) return false;
  else {
    if ((tags[1].length < 1) || (tags[1].length > 8)) return false;
    if (!isAlphaNum(tags[1])) return false;
    checkedTags++;
  }

  /* any remaining subtags must be one to eight alphabetic characters */

  for (var i = checkedTags; i < tags.length; i++) {
    if ((tags[1].length < 1) || (tags[i].length > 8)) return false;
    if (!isAlphaNum(tags[i])) return false;
    checkedTags++;
  }
  return true;
}

function AddAvailableLanguage()
{
  var Languagename = new String();
  var Languageid = new String();
  
  var addThese = new Array();
  var addTheseNames = new Array();
  var invalidLangs = new Array();

  //selected languages
  for (var nodeIndex=0; nodeIndex < available_languages.selectedItems.length; nodeIndex++) {

    var selItem =  available_languages.selectedItems[nodeIndex];

    Languagename    = selItem.getAttribute('label');
    Languageid      = selItem.getAttribute('id');

    if (!LangAlreadyActive(Languageid)) {
      addThese[nodeIndex] = Languageid;
      addTheseNames[nodeIndex] = Languagename;
    }
  }

  //user-defined languages
  var otherField = document.getElementById( "languages.other" );

  var selCount = addThese.length;

  if (otherField.value) {

    var LanguageidsString = otherField.value.replace(/\s/g,"").toLowerCase();
    var Languageids = LanguageidsString.split(separatorRe);
    for (var i = 0; i < Languageids.length; i++) {
      Languageid = Languageids[i];
      
      if (IsRFC1766LangTag(Languageid)) {
        if (!LangAlreadySelected(Languageid)) {
          if (!LangAlreadyActive(Languageid)) {
            Languagename = GetLanguageTitle(Languageid);
            if (!Languagename)
              Languagename = '[' + Languageid + ']';
            addThese[i+selCount] = Languageid;
            addTheseNames[i+selCount] = Languagename;
          }
        }
      }
      else {
        invalidLangs[invalidLangs.length] = Languageid;
      }
    }
    if (invalidLangs.length > 0) {
      var errorMsg = prefLangBundle.GetStringFromName("illegalOtherLanguage") + " " +
                     invalidLangs.join(", ");
      var errorTitle = prefLangBundle.GetStringFromName("illegalOtherLanguageTitle");
      
      var prompter = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                 .getService(Components.interfaces.nsIPromptService);
                                 
      prompter.alert(this.window, errorTitle, errorMsg);
      otherField.focus();
      return false;
    }
  }
  
  for (i = 0; i < addThese.length; i++) {
    AddListItem(window.opener.document, active_languages, addThese[i], addTheseNames[i]);
  }

  available_languages.clearSelection();
  return true;
}

function LangAlreadySelected(langid) {
  var found = false;

  for (var i = 0; i < available_languages.selectedItems.length; i++) {
    var currentLang = available_languages.selectedItems[i]; 
    var Languageid  = currentLang.getAttribute('id');
    if (Languageid == langid)
      return true;
  }
  return false;
}

function HandleDoubleClick(node)
{
  var languageId    = node.id;
  var languageName  = node.getAttribute('label');

  if (languageName && languageName.length > 0)
  {
    if (!LangAlreadyActive(languageId)) {
      AddListItem(window.opener.document, active_languages, languageId, languageName);
    }
    window.close();
  }//if
} //HandleDoubleClick

function RemoveActiveLanguage()
{

  var nextNode = null;
  var numSelected = active_languages.selectedItems.length;
  var deleted_all = false;

  while (active_languages.selectedItems.length > 0) {

  var selectedNode = active_languages.selectedItems[0];
    nextNode = selectedNode.nextSibling;

  if (!nextNode)

    if (selectedNode.previousSibling)
    nextNode = selectedNode.previousSibling;

    active_languages.removeChild(selectedNode);
   } //while

  if (nextNode) {
    active_languages.selectItem(nextNode)
  } else {
    //active_languages.clearSelection();
  }

  UpdateSavePrefString();

} //RemoveActiveLanguage


function GetLanguageTitle(id)
{

  if (availLanguageDict)
    for (var j = 0; j < availLanguageDict.length; j++) {

      if ( availLanguageDict[j][1] == id) {
      //title =
  return availLanguageDict[j][0];
      }
    }
  return '';
}


function AddListItem(doc, listbox, langID, langTitle)
{
  try {  //let's beef up our error handling for languages without label / title

      // Create a listitem for the new Language
      var item = doc.createElement('listitem');

      // Copy over the attributes
      item.setAttribute('label', langTitle);
      item.setAttribute('id', langID);

      listbox.appendChild(item);

  } //try

  catch (ex) {
  } //catch

}


function UpdateSavePrefString()
{
  var num_languages = 0;
  pref_string = "";

  for (var item = active_languages.firstChild; item != null; item = item.nextSibling) {

    var languageid = item.getAttribute('id');

    if (languageid.length > 1) {

          num_languages++;

      //separate >1 languages by commas
      if (num_languages > 1) {
        pref_string = pref_string + "," + " " + languageid;
      } else {
        pref_string = languageid;
      } //if
    } //if
  }//for

  active_languages.setAttribute("prefvalue", pref_string);
}


function MoveUp() {

  if (active_languages.selectedItems.length == 1) {
    var selected = active_languages.selectedItems[0];
    var before = selected.previousSibling
    if (before) {
      before.parentNode.insertBefore(selected, before);
      active_languages.selectItem(selected);
      active_languages.ensureElementIsVisible(selected);
    }
  }
  
  if (active_languages.selectedIndex == 0)
  {
    // selected item is first
    var moveUp = document.getElementById("up");
    moveUp.disabled = true;
  }

  if (active_languages.childNodes.length > 1)
  {
    // more than one item so we can move selected item back down
    var moveDown = document.getElementById("down");
    moveDown.disabled = false;
  }

  UpdateSavePrefString();

} //MoveUp


function MoveDown() {

  if (active_languages.selectedItems.length == 1) {
    var selected = active_languages.selectedItems[0];
    if (selected.nextSibling) {
      if (selected.nextSibling.nextSibling) {
        active_languages.insertBefore(selected, selected.nextSibling.nextSibling);
      }
      else {
        active_languages.appendChild(selected);
      }
      active_languages.selectItem(selected);
    }
  }

  if (active_languages.selectedIndex == active_languages.childNodes.length - 1)
  {
    // selected item is last
    var moveDown = document.getElementById("down");
    moveDown.disabled = true;
  }

  if (active_languages.childNodes.length > 1)
  {
    // more than one item so we can move selected item back up 
    var moveUp = document.getElementById("up");
    moveUp.disabled = false;
  }

  UpdateSavePrefString();

} //MoveDown


function SelectLanguage() {
  if (active_languages.selectedItems.length) {
    document.getElementById("remove").disabled = false;
    var selected = active_languages.selectedItems[0];
    document.getElementById("down").disabled = !selected.nextSibling;

    document.getElementById("up").disabled = !selected.previousSibling;
  }
  else {
    document.getElementById("remove").disabled = true;
    document.getElementById("down").disabled = true;
    document.getElementById("up").disabled = true;
  }

  if (parent.hPrefWindow.getPrefIsLocked(document.getElementById("up").getAttribute("prefstring")))
    document.getElementById("up").disabled = true;
  if (parent.hPrefWindow.getPrefIsLocked(document.getElementById("down").getAttribute("prefstring")))
    document.getElementById("down").disabled = true;
  if (parent.hPrefWindow.getPrefIsLocked(document.getElementById("add").getAttribute("prefstring")))
    document.getElementById("add").disabled = true;
  if (parent.hPrefWindow.getPrefIsLocked(document.getElementById("remove").getAttribute("prefstring")))
    document.getElementById("remove").disabled = true;
} // SelectLanguage
