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
 * The Original Code is the Mozilla Penelope project.
 *
 * The Initial Developer of the Original Code is
 * QUALCOMM incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     Mark Charlebois <mcharleb@qualcomm.com> original author
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

/* components defined in this file */
var PenelopeCompose = { 
    onLoad: function() 
    { 
        // quit if this function has already been called
        if (arguments.callee.done) return;

        // flag this function so we don't do the same thing twice
        arguments.callee.done = true;

        // rely on startup from MsgComposeCommands.js
        window.setTimeout("penelopeComposeLoaded()", 0);
    } 

}; 

function penelopeComposeLoaded()
{
    var currentEditor = GetCurrentEditor();
    currentEditor.returnInParagraphCreatesNewParagraph = true;
    dump("set returnInParagraphCreatesNewParagraph\n");
}

//load event handler
window.addEventListener("DOMContentLoaded", function(e) { PenelopeCompose.onLoad(e); }, false); 

// The following global variables are defined in MsgComposeCommands.js
// var sMsgComposeService;
// var gMsgCompose;

function increaseMsgPriority()
{
    if (gMsgCompose)
    {
        var msgCompFields = gMsgCompose.compFields;
        if (msgCompFields)
        {
            switch (msgCompFields.priority)
            {
            case "Highest":
                break;
            case "High":
                msgCompFields.priority = "Highest";
                break;
            case "Low":
                msgCompFields.priority = "Normal";
                break;
            case "Lowest":
                msgCompFields.priority = "Low";
                break;
            case "Normal":
            default:
                msgCompFields.priority = "High";
                break;
            }
            // keep priority toolbar button in synch with possible changes via the menu item
            updatePriorityToolbarButton(msgCompFields.priority);
        }
    }
}

function decreaseMsgPriority()
{
    if (gMsgCompose)
    {
        var msgCompFields = gMsgCompose.compFields;
        if (msgCompFields)
        {
            switch (msgCompFields.priority)
            {
            case "Highest":
                msgCompFields.priority = "High";
                break;
            case "High":
                msgCompFields.priority = "Normal";
                break;
            case "Low":
                msgCompFields.priority = "Lowest";
                break;
            case "Lowest":
                break;
            case "Normal":
            default:
                msgCompFields.priority = "Low";
                break;
            }
            // keep priority toolbar button in synch with possible changes via the menu item
            updatePriorityToolbarButton(msgCompFields.priority);
        }
    }
}

function removeFormatting()
{
    var currentEditor = GetCurrentEditor();
    if (GetCurrentEditorType() != "textmail" && currentEditor.canCopy())
    {
        currentEditor.beginTransaction();
        // copy the selection to the clipboard 
        currentEditor.copy();
        // paste without formatting
        currentEditor.pasteNoFormatting(nsISelectionController.SELECTION_NORMAL);
        currentEditor.endTransaction();
    }
}

function penelopeNextMisspelling()
{
    //get the misspelling selection
    var currentEditor = GetCurrentEditor();
    var selcon = currentEditor.selectionController;

    var sel = selcon.getSelection(selcon.SELECTION_SPELLCHECK);
    var nsel=selcon.getSelection(selcon.SELECTION_NORMAL);
    

    var before = -1;
    for (var i=0; i<sel.rangeCount; i++)
    {
        var range = sel.getRangeAt(i);
        var pos = range.comparePoint(nsel.focusNode, nsel.focusOffset)
        if (pos != -1)
        {
            before = pos;
        }
        else if (before != -1 && pos == -1)
        {
            currentEditor.selection.removeAllRanges();
            currentEditor.selection.addRange(range);
            return;
        }
        
    }
    if (sel.rangeCount)
    {
        // Next spelling error must be the first range
        currentEditor.selection.removeAllRanges();
        currentEditor.selection.addRange(sel.getRangeAt(0));
    }
}


