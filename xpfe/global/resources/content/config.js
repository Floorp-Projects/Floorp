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
 */

var arr = new Array();

var view = ({
    rowCount : 0, 
    getCellText : function(k, col) 
        {	
            if ( !arr[k] )
                return;
            return arr[k][col];
        },
    getRowProperties : function(index, prop) {},
    getCellProperties : function(index, col, prop) {},
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
});

function onConfigLoad()
{
    document.title = "about:config";
    var prefCount = {value:0};
    var prefService = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService);
    var prefBranch = prefService.getBranch(null);
    var prefArray = prefBranch.getChildList("" , prefCount);
    const nsIPrefBranch = Components.interfaces.nsIPrefBranch;
    var prefName, prefType, prefTypeName, prefValue, prefIndex, prefLockState;

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
        prefLockState = "default";
	
        if(prefBranch.prefIsLocked(prefArray[i]))  // 0 = false
            prefLockState = "locked";
        else if(prefBranch.prefHasUserValue(prefArray[i]))    // 0 = false
            prefLockState = "user set";

        prefType = prefBranch.getPrefType(prefArray[i]);
        switch(prefType)
        {
            case nsIPrefBranch.PREF_STRING:
                prefTypeName = "string";
                prefValue = prefBranch.getCharPref(prefArray[i]);
                prefValue = htmlEscape(prefValue);
                break;
            case nsIPrefBranch.PREF_INT:
                prefTypeName = "int";
                prefValue = prefBranch.getIntPref(prefArray[i]);
                break;
            case nsIPrefBranch.PREF_BOOL:
                prefTypeName = "bool";
                prefValue = prefBranch.getBoolPref(prefArray[i]);
                break;
            default: 
                prefTypeName = "unknown";
                break;
        }

        arr[k] = {indexCol:prefIndex, prefCol:prefArray[i], lockCol:prefLockState, typeCol:prefTypeName, valueCol:prefValue};
    }
  
    view.rowCount = k + 1;

    var outliner = document.getElementById("out");
    outliner.outlinerBoxObject.view = view;

}

function htmlEscape(s) 
{ 
    s = s.replace(/\&/g, "&amp;"); 
    s = s.replace(/\>/g, "&gt;"); 
    s = s.replace(/\</g, "&lt;"); 
    return s; 
}


