/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
*
* The contents of this file are subject to the Mozilla Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is Netscape Communications
* Corporation.  Portions created by Netscape are
* Copyright (C) 2000 Netscape Communications Corporation. All
* Rights Reserved.
*
* Contributor(s):
*   Adrian Havill <havill@redhat.com>
*/

//GLOBALS


//locale bundles
var regionsBundle;
var languagesBundle;
var acceptedBundle;
var prefLangBundle;

//dictionary of all supported locales
var availLanguageDict;

//XUL tree handles
var available_languages;
var available_languages_treeroot;

//XUL tree handles
var active_languages;
var active_languages_treeroot;

//XUL window pref window interface object
var pref_string = new String();

//Reg expression for splitting multiple pref values
var separatorRe = /\s*,\s*/;

function GetBundles()
{
  if (!regionsBundle)    regionsBundle   = srGetStrBundle("chrome://global/locale/regionNames.properties");
  if (!languagesBundle)  languagesBundle = srGetStrBundle("chrome://global/locale/languageNames.properties");
  if (!prefLangBundle)  prefLangBundle = srGetStrBundle("chrome://communicator/locale/pref/pref-languages.properties");
  if (!acceptedBundle)   acceptedBundle  = srGetStrBundle("resource:/res/language.properties");
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
      active_languages_treeroot     = document.getElementById('active_languages_root');
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
      available_languages_treeroot  = document.getElementById('available_languages_root');
      active_languages              = window.opener.document.getElementById('active_languages');
      active_languages_treeroot     = window.opener.document.getElementById('active_languages_root');
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
    window.openDialog("chrome://communicator/content/pref/pref-languages-add.xul","_blank","modal,chrome,centerscreen,resizable=yes,titlebar", "addlangwindow");
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
       stringName = curItem.getKey();
       stringNameProperty = stringName.split('.');

       if (stringNameProperty[1] == 'accept') {

          //dump the UI string (value)
           visible   = curItem.getValue();

          //if (visible == 'true') {

             str = stringNameProperty[0];
             var stringLangRegion = stringNameProperty[0].split('-');

             if (stringLangRegion[0]) {
                 var tit;
                 try {
                    tit = languagesBundle.GetStringFromName(stringLangRegion[0]);
                 }

                 catch (ex) {
                    tit = "";
                 }


                 if (stringLangRegion.length > 1) {

                   try {
                    tit += "/" + regionsBundle.GetStringFromName(stringLangRegion[1]);
                   }

                   catch (ex) {
          tit += "["+str+"]";
                   }

                 } //if region

                 tit = tit + "  [" + str + "]";

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

        AddTreeItem(document, available_languages_treeroot, availLanguageDict[i][1], availLanguageDict[i][0]);

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
        AddTreeItem(document, active_languages_treeroot, str, tit);

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
  /* Valid language codes examples:
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
      if (tags[1].length < 2 || tags[1].length > 3) return false;
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


  //selected languages
  for (var nodeIndex=0; nodeIndex < available_languages.selectedItems.length; nodeIndex++) {

    var selItem =  available_languages.selectedItems[nodeIndex];
    var selRow  =  selItem.firstChild;
    var selCell =  selRow.firstChild;

    Languagename    = selCell.getAttribute('label');
    Languageid      = selCell.getAttribute('id');

    if (!LangAlreadyActive(Languageid)) {

      AddTreeItem(window.opener.document, active_languages_treeroot, Languageid, Languagename);

    }//if

  } //loop selected languages


  //user-defined languages
  var otherField = document.getElementById( "languages.other" );

  if (otherField.value) {

    /* reject bogus lang strings, INCLUDING those with HTTP "q"
       values kludged on the end of them
    */
    if (!IsRFC1766LangTag(otherField.value)) {
      window.alert(prefLangBundle.GetStringFromName("illegalOtherLanguage"));
      return false;
    }

    Languageid      = otherField.value;
    Languageid      = Languageid.toLowerCase();
    Languagename    = GetLanguageTitle(Languageid);
    if (!Languagename)
       Languagename = '[' + Languageid + ']';

    if (!LangAlreadyActive(Languageid)) {

      AddTreeItem(window.opener.document, active_languages_treeroot, Languageid, Languagename);

    }//if
  }

  available_languages.clearItemSelection();
  return true;
} //AddAvailableLanguage

function HandleDoubleClick(node)
{
  var languageId    = node.id;
  var languageName  = node.getAttribute('label');

  if (languageName && languageName.length > 0)
  {
    if (!LangAlreadyActive(languageId)) {
      AddTreeItem(window.opener.document, active_languages_treeroot, languageId, languageName);
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

    var row  =  selectedNode.firstChild;
    var cell =  row.firstChild;

    row.removeChild(cell);
    selectedNode.removeChild(row);
    active_languages_treeroot.removeChild(selectedNode);

   } //while

  if (nextNode) {
    active_languages.selectItem(nextNode)
  } else {
    //active_languages.clearItemSelection();
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


function AddTreeItem(doc, treeRoot, langID, langTitle)
{
  try {  //let's beef up our error handling for languages without label / title

      // Create a treerow for the new Language
      var item = doc.createElement('treeitem');
      var row  = doc.createElement('treerow');
      var cell = doc.createElement('treecell');

      // Copy over the attributes
      cell.setAttribute('label', langTitle);
      cell.setAttribute('id', langID);

      // Add it to the active languages tree
      item.appendChild(row);
      row.appendChild(cell);

      treeRoot.appendChild(item);

  } //try

  catch (ex) {
  } //catch

}


function UpdateSavePrefString()
{
  var num_languages = 0;
  pref_string = "";

  for (var item = active_languages_treeroot.firstChild; item != null; item = item.nextSibling) {

    var row  =  item.firstChild;
    var cell =  row.firstChild;
    var languageid = cell.getAttribute('id');

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

  UpdateSavePrefString();

} //MoveUp


function MoveDown() {

  if (active_languages.selectedItems.length == 1) {
    var selected = active_languages.selectedItems[0];
    if (selected.nextSibling) {
      if (selected.nextSibling.nextSibling) {
        selected.parentNode.insertBefore(selected, selected.nextSibling.nextSibling);
      }
      else {
        selected.parentNode.appendChild(selected);
      }
      active_languages.selectItem(selected);
    }
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
