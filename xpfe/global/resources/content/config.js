/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributors:
 *  Chip Clark <chipc@netscape.com>
 *  Seth Spitzer <sspitzer@netscape.com>
 */

var baseArray = new Array();

const kDefault = 0;
const kUserSet = 1;
const kLocked = 2;

const nsIAtomService = Components.interfaces.nsIAtomService;
const nsAtomService_CONTRACTID = "@mozilla.org/atom-service;1";

var atomService = Components.classes[nsAtomService_CONTRACTID].getService(nsIAtomService);
var gLockAtoms = [atomService.getAtom("default"), atomService.getAtom("user"), atomService.getAtom("locked")];
// XXX get these from a string bundle
var gLockStrs = ["default", "user set","locked"]; 

const kStrType = 0;
const kIntType = 1;
const kBoolType = 2;

var gTypeStrs = ["string","int","bool"];

var gOutliner;

var view = ({
    rowCount : 0, 
    getCellText : function(k, col) {	
            if (!baseArray[k])
                return "";
            
            var value = baseArray[k][col];
            var strvalue;

            switch (col) {
              case "lockCol":           
                strvalue = gLockStrs[value];
                break;
              case "typeCol":
                strvalue = gTypeStrs[value];
                break;
              default:
                // already a str.
                strvalue = value;    
                break;
            }
            return strvalue;
        },
    getRowProperties : function(index, prop) {},
    getCellProperties : function(index, col, prop) {
      prop.AppendElement(gLockAtoms[baseArray[index]["lockCol"]]);
    },
    getColumnProperties : function(col, elt, prop) {},
    outlinerbox : null,
    selection : null,
    isContainer : function(index) { return false; },
    isContainerOpen : function(index) { return false; },
    isContainerEmpty : function(index) { return false; },
    isSorted : function() { },
    canDropOn : function(index) { return false; },
    canDropBeforeAfter : function(index,before) { return false; },
    drop : function(row,orientation) {},
    setOutliner : function(out) { this.outlinerbox = out; },
    getParentIndex: function(rowIndex) { return -1 },
    hasNextSibling: function(rowIndex, afterIndex) { return false },
    getLevel: function(index) { return 1},
    toggleOpenState : function(index) {},
    cycleHeader: function(colID, elt) {},
    selectionChanged : function() {},
    cycleCell: function(row, colID) {},
    isEditable: function(row, valueCol) {return true},
    isEditable: function(row, colID) {return false},
    setCellText: function(row, colID, value) {},
    performAction: function(action) {},
    performActionOnRow: function(action, row) {},
    performActionOnCell: function(action, row, colID) {},
    isSeparator: function(index) {return false}
});

function onConfigLoad()
{
    document.title = "about:config";

    var prefCount = {value:0};
    var prefService = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService);
    var prefBranch = prefService.getBranch(null);
    var prefArray = prefBranch.getChildList("" , prefCount);
    var prefName, prefType, prefValue, prefIndex, prefLockState;

    var i = 0;
    var j = 0;   // This is used to avoid counting the "capability" preferences
    var k = 0;	 // This is to maintain a count of prefs (not including the "capability" prefs);

    prefArray.sort();
    for (i = 0; i < prefCount.value; i++) 
    {
        if((prefArray[i].indexOf("capability", 0) + 1) > 0)    // avoid displaying "private" preferences
        {
            j++;
            continue;
        }

        k = (i - j);   // avoid numbering "capability" prefs

        prefIndex = k + 1;
	
        if (prefBranch.prefIsLocked(prefArray[i]))
          prefLockState = kLocked;
        else if(prefBranch.prefHasUserValue(prefArray[i]))
          prefLockState = kUserSet;
        else
          prefLockState = kDefault;

        const nsIPrefBranch = Components.interfaces.nsIPrefBranch;

        switch (prefBranch.getPrefType(prefArray[i])) {
            case nsIPrefBranch.PREF_INT:
            prefType = kIntType;
            // convert to a string
            prefValue = "" + prefBranch.getIntPref(prefArray[i]);
                break;
            case nsIPrefBranch.PREF_BOOL:
            prefType = kBoolType;
            // convert to a string
            if (prefBranch.getBoolPref(prefArray[i]))
              prefValue = "true";
            else
              prefValue = "false";
                break;
          case nsIPrefBranch.PREF_STRING:
            default: 
            prefType = kStrType;
            prefValue = htmlEscape(prefBranch.getCharPref(prefArray[i]));
                break;
        }

        baseArray[k] = {indexCol:prefIndex, prefCol:prefArray[i], lockCol:prefLockState, typeCol:prefType, valueCol:prefValue};
    }
  
    view.rowCount = k + 1;

    gOutliner = document.getElementById("configOutliner");
    gOutliner.outlinerBoxObject.view = view;
}

function htmlEscape(s) 
{ 
    s = s.replace(/\&/g, "&amp;"); 
    s = s.replace(/\>/g, "&gt;"); 
    s = s.replace(/\</g, "&lt;"); 
    return s; 
}


function ConfigOnClick(event)
{
    // we only care about button 0 (left click) events
    if (event.button != 0) return;

    var t = event.originalTarget;

    if (t.localName == "outlinercol") {
       HandleColumnClick(t.id);
    }
}

var gSortedColumn = "indexCol";
var gSortAscending = true;

function stringSortFunction(a,b)
{
  var value;
  if (a<b)
    value = 1;
  else if (a>b) 
    value = -1;
  else
    value = 0;
 
  if (gSortAscending)
    return value;
  else
    return (value * -1);
}

function intSortFunction(a,b)
{
  if (gSortAscending) 
    return (a - b);
  else 
    return (b - a);
}

function indexColSortFunction(x,y)
{
  return intSortFunction(x.indexCol, y.indexCol);
}

function prefColSortFunction(x,y)
{
  return stringSortFunction(x.prefCol, y.prefCol);
}

function lockColSortFunction(x,y)
{
  return stringSortFunction(x.lockCol, y.lockCol);
}

function typeColSortFunction(x,y)
{
  return intSortFunction(x.typeCol, y.typeCol);
}

function valueColSortFunction(x,y)
{
  return stringSortFunction(x.valueCol, y.valueCol);
}

var gSortFunctions = {indexCol:indexColSortFunction, 
                 prefCol:prefColSortFunction, 
                 lockCol:lockColSortFunction, 
                 typeCol:typeColSortFunction, 
                 valueCol:valueColSortFunction};

function HandleColumnClick(id)
{
  if (id == gSortedColumn) {
    gSortAscending = !gSortAscending;
    baseArray.reverse();
  }
  else {
    baseArray.sort(gSortFunctions[id]);
    gSortedColumn = id;
  }

  gOutliner.outlinerBoxObject.invalidate();
}




