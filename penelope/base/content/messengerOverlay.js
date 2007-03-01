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
 *     Jeff Beckley <beckley@qualcomm.com>
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

var PenelopeMessenger = { 
    onLoad: function() 
    { 

        // TODO If the window has already been created, close the new one and focus the old one

        // quit if this function has already been called
        if (arguments.callee.done) return;

        // flag this function so we don't do the same thing twice
        arguments.callee.done = true;


        // update the mailbox and transfer menus
        setTimeout(delayedOnLoadPenelopeMessenger, 50); 

        // perform column remapping once
        setTimeout(delayedOnLoadPenelopeColumns, 60); 

        // We only need to set the sound once
        setNewMailSound();

        // Set up an observer for when the view has been created
        // in order to register the Group Select column
        var ObserverService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
        ObserverService.addObserver(CreateDbObserver, "MsgCreateDBView", false);

    }
}; 

//load event handler
window.addEventListener("DOMContentLoaded", function(e) { PenelopeMessenger.onLoad(e); }, false); 

function delayedOnLoadPenelopeMessenger()
{
    // Add a listener for handling double-click and Enter key on a folder
    var folderTree = document.getElementById("folderTree"); 
    folderTree.addEventListener("click",FolderTreeOnClick,true);
    folderTree.addEventListener("keypress",FolderTreeOnKeyPress,true);
    
    // Add a listener for handling Alt+click
    var threadTree = document.getElementById("threadTree"); 
    threadTree.addEventListener("mousedown",ThreadTreeOnMouseDown,true);
    threadTree.addEventListener("click",ThreadTreeOnMouseClick,true);
    threadTree.addEventListener("keypress",ThreadTreeOnKeyPress,true);
}

function checkIfWindowOpen(folderURL, bDontCheckSelf)
{
    var mediator = Components.classes["@mozilla.org/appshell/window-mediator;1"].getService(Components.interfaces.nsIWindowMediator);

    // Grab window list; for all windows, pass null as the parameter.
    var windows = mediator.getXULWindowEnumerator(null);

    while (windows.hasMoreElements()) 
    {
        var win = windows.getNext();

        var ifaces = Components.interfaces;
        if(!(win instanceof ifaces.nsIXULWindow)) 
        {
            dump("Found invalid item\n");
            continue;
        }

        var requestor = win.docShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
        var mywin = requestor.getInterface(Components.interfaces.nsIDOMWindow);

        var topwin = mywin.document.getElementById("messengerWindow");
        dump("topwin "+topwin+"\n");
        if (topwin)
        {
            if (topwin.getAttribute("folderwin") == "true")
            {
                dump("topwin found\n");
                continue;
            }
        
            if (bDontCheckSelf && mywin == window)
            {
                continue;
            }

            var url = GetSelectedFolderURIForWin(mywin);
            dump("Folder URL for window = "+url+"\n");

            if (url == folderURL)
            {
                return mywin;
            }
        }
    }

    return null;
}

function penelopeMessengerOnLoad()
{
    dump("In penelopeMessengerOnLoad\n");

    // Get the preferences for the window instance
    var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                getService(Components.interfaces.nsIPrefBranch);

    var uri = null;
    if (window.arguments.length)
    {
        dump("arguements.length "+window.arguments.length+"\n");

        // filter our any feed urls that came in as arguments to the new window...
        if (/^feed:/i.test(window.arguments[0]))
        {
            // Leave uri=null
        }
        else
        {
            uri = window.arguments[0];
            dump("Folder URI = "+uri+"\n");
        }
    }

    // If the user wants the 2-Pane UI colapse the folder
    var use3PaneUI = prefs.getBoolPref("penelope.ui3pane");

    // This call does nothing if in 2-pane mode and the current win is the folder win
    initializeFolderWin(uri, use3PaneUI, prefs);

    OnLoadMessenger();

    // if we are in 2-pane mode, the first window is the folder window
    if (!uri && !use3PaneUI)
    {
        // This is the 2-pane folder summary window
        initializeSummaryFolderWin(prefs);
    }
}

function initializeSummaryFolderWin(prefs)
{
    var topwin = document.getElementById("messengerWindow");
    topwin.setAttribute("folderwin", "true");
    dump("set topwin "+topwin+"\n");

    // reconfigure for 2-pane mode folder window
    var toolbar = document.getElementById('mail-bar2');
    toolbar.collapsed = true;

    // Add flex to the folder pane
    document.getElementById('folderPaneBox').setAttribute('flex', '1');

    // Remove flex from vbox with no ID
    document.getElementById('folderpane_splitter').nextSibling.removeAttribute('flex');
    document.getElementById('folderpane_splitter').nextSibling.collapsed = true;

    document.getElementById('threadPaneBox').removeAttribute('flex');
    document.getElementById('threadTree').removeAttribute('flex');
    document.getElementById('threadTree').setAttribute("flags", "dont-build-content");
    document.getElementById('threadTree').disabled=true;

    //var splitter = document.getElementById('threadpane-splitter');
    //splitter.collapsed = false;
    document.documentElement.setAttribute("width", 300);
    document.documentElement.setAttribute("height", 700);

    var menuitem = document.getElementById('menu_customizeToolbar');
    menuitem.hidden = true;

    menuitem = document.getElementById('menu_saveAs');
    menuitem.disabled = true;
    menuitem = document.getElementById('printSetupMenuItem');
    menuitem.disabled = true;
    menuitem = document.getElementById('printMenuItem');
    menuitem.disabled = true;
    menuitem = document.getElementById('menu_delete');
    menuitem.disabled = true;
    menuitem = document.getElementById('penelopeTransferMenu');
    menuitem.hidden = true;
    menuitem = document.getElementById('messageMenu');
    menuitem.hidden = true;

    // View menu
    menuitem = document.getElementById('menu_Toolbars');
    menuitem.hidden = true;
    menuitem = document.getElementById('menu_MessagePaneLayout');
    menuitem.hidden = true;
    menuitem = document.getElementById('viewSortMenuSeparator');
    menuitem.hidden = true;
    menuitem = document.getElementById('viewMessagesMenu');
    menuitem.hidden = true;
    menuitem = document.getElementById('viewheadersmenu');
    menuitem.hidden = true;

    // hide the separator with no ID
    menuitem.previousSibling.hidden = true;

    menuitem = document.getElementById('viewMessageViewMenu');
    menuitem.hidden = true;
    menuitem = document.getElementById('viewAttachmentsInlineMenuitem');
    menuitem.hidden = true;
    menuitem = document.getElementById('viewTextSizeMenu');

    // hide the separator with no ID
    menuitem.previousSibling.hidden = true;

    menuitem.hidden = true;
    menuitem = document.getElementById('viewBodyMenu');
    menuitem.hidden = true;
    menuitem = document.getElementById('mailviewCharsetMenu');
    menuitem.hidden = true;
    menuitem = document.getElementById('pageSourceMenuItem');
    menuitem.hidden = true;

    // hide the separator with no ID
    menuitem.previousSibling.hidden = true;

    menuitem = document.getElementById('viewSortMenu');
    menuitem.hidden = true;
    menuitem = document.getElementById('viewSortMenuSeparator');
    menuitem.hidden = true;

    // Get the Go menu
    menuitem = document.getElementById('goNextMenu').parentNode.parentNode;
    menuitem.hidden = true;

    try
    {
        // printPreviewMenuItem is only defined on non-Mac
        menuitem = document.getElementById('printPreviewMenuItem');
        menuitem.disabled = true;
    }
    catch(e)
    {
    }

    try
    {
        dump("Folder Window Dimentions:\n");
        var width = prefs.getIntPref("penelope.__folderwindow.width");
        var height = prefs.getIntPref("penelope.__folderwindow.height");
        var screenX = prefs.getIntPref("penelope.__folderwindow.screenX");
        var screenY = prefs.getIntPref("penelope.__folderwindow.screenY");

        dump("Moving to: "+screenX+", "+screenY+"\n");
        window.moveTo(screenX, screenY);

        dump("Resize to: "+width+", "+height+"\n");
        document.documentElement.setAttribute("width", width);
        document.documentElement.setAttribute("height", height);
    }
    catch(e)
    {
        dump("Error setting folder pane attributes: "+e+"\n");
    }
}

// Init 2-pane or 3-pane window depending on the UI mode
function initializeFolderWin(uri, use3PaneUI, prefs)
{
    if (!uri)
    {
        if (use3PaneUI)
        {
            // This is the folder pane
            // Get the last set of opened windows
            dump("Opened 1st 3-pane window\n");

            try
            {
                var defaultAccount = accountManager.defaultAccount;
                defaultServer = defaultAccount.incomingServer;
                var inboxFolder = GetInboxFolder(defaultServer);
                uri = inboxFolder.URI;
            }
            catch(e)
            {
                dump("Could not determine default folder\n");
            }
        }
        // 2-pane handling is below call to OnLoadMessenger()
    }

    if (uri)
    {
        if (checkIfWindowOpen(uri, false))
        {
            dump("Window "+uri+" is already open\n");
        }
        else
        {
            dump("Window "+uri+" is not open\n");
        }

        try
        {
            dump("FolderURI: "+uri+"\n");
            var width = prefs.getIntPref("penelope."+uri+".window.width");
            var height = prefs.getIntPref("penelope."+uri+".window.height");
            var screenX = prefs.getIntPref("penelope."+uri+".window.screenX");
            var screenY = prefs.getIntPref("penelope."+uri+".window.screenY");

            dump("Moving to: "+screenX+", "+screenY+"\n");
            window.moveTo(screenX, screenY);

            dump("Resize to: "+width+", "+height+"\n");
            document.documentElement.setAttribute("width", width);
            document.documentElement.setAttribute("height", height);
        
            var folderPane = document.getElementById("folderPaneBox"); 
            if (use3PaneUI)
            {
                dump("Using 3-pane UI\n");
                var fpcollapsed = prefs.getBoolPref("penelope."+uri+".window.fpcollapsed");
                var fpwidth = prefs.getIntPref("penelope."+uri+".window.fpwidth");

                var folderPaneSplitter = document.getElementById("folderpane_splitter"); 
                var fpsplitter = prefs.getCharPref("penelope."+uri+".window.fpsplitter");

                dump("Setting fpwidth = "+fpwidth+"\n");
                dump("Setting fpcollapsed = "+fpcollapsed+"\n");
                dump("Setting fpsplitter = "+fpsplitter+"\n");
                if (fpwidth)
                {
                    folderPane.width = fpwidth;
                }

                if (fpcollapsed)
                {
                    folderPane.collapsed = true;
                }
                else
                {
                    folderPane.collapsed = false;
                }
                if (fpsplitter != "")
                {
                    folderPaneSplitter.setAttribute("state", fpsplitter);
                }
            }
            else
            {
                dump("Using 2-pane UI\n");
                folderPane.collapsed = true;
                var folderPaneSplitter = document.getElementById("folderpane_splitter"); 
                folderPaneSplitter.setAttribute("state", "collapsed");
            }
        }
        catch(e)
        {
            dump("Error setting folder pane attributes: "+e+"\n");
        }

        try
        {
            var messagePane = document.getElementById("messagepanebox"); 
            var mpwidth = prefs.getIntPref("penelope."+uri+".window.mpwidth");
            var mpheight = prefs.getIntPref("penelope."+uri+".window.mpheight");

            var threadPaneSplitter = document.getElementById("threadpane-splitter"); 
            var tpsplitter = prefs.getCharPref("penelope."+uri+".window.tpsplitter");

            dump("Setting tpsplitter = "+tpsplitter+"\n");
            dump("Setting mpwidth = "+mpwidth+"\n");
            dump("Setting mpheight = "+mpheight+"\n");
            if (mpwidth)
            {
                messagePane.width = mpwidth;
            }
            if (mpheight)
            {
                messagePane.height = mpheight;
            }
            if (tpsplitter != "")
            {
                threadPaneSplitter.setAttribute("state", tpsplitter);
            }
        }
        catch(e)
        {
            dump("Error setting thread pane attributes: "+e+"\n");
        }
    }
}

function GetSelectedFolderURIForWin(win)
{
    try
    {
        dump("WINDOW: "+document.title+"");
        var folderTree = win.document.getElementById("folderTree");
        var folderResource = folderTree.builderView.getResourceAtIndex(folderTree.currentIndex);
        var url = folderResource.Value;
        //var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
        //var url = msgFolder.folderURL;
        dump("WIN URL = "+url+"\n");
        return url;
    }
    catch(e)
    {
        dump("Caught exception in GetSelectedFolderURIForWin\n");
    }
}

function penelopeMessengerOnUnload()
{
    dump("In penelopeMessengerOnUnload\n");

    var uri = null;

    var folderwin = document.getElementById("messengerWindow");

    // See if this window is the main folder window
    if (folderwin)
    {
        if (folderwin.getAttribute("folderwin") != "true")
        {
            // This is not the 2-pane folder window
            uri = GetSelectedFolderURI();
        }
    }

    var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                getService(Components.interfaces.nsIPrefBranch);

    // If this is not the main folder pane
    if (uri)
    {
        // If the user wants the 2-Pane UI ignore folder settings
        var use3PaneUI = prefs.getBoolPref("penelope.ui3pane");

        // Set the preferences for the window instance
        dump("Saving dimentions as: "+window.innerWidth+", "+window.innerHeight+"\n");
        dump("Saving position as: "+window.screenX+", "+window.screenY+"\n");
        prefs.setIntPref("penelope."+uri+".window.width", window.innerWidth);
        prefs.setIntPref("penelope."+uri+".window.height", window.innerHeight);
        prefs.setIntPref("penelope."+uri+".window.screenX", window.screenX);
        prefs.setIntPref("penelope."+uri+".window.screenY", window.screenY);

        // Only store foldre pane settings if in 3 pane mode
        if (use3PaneUI)
        {
            try
            {
                var folderPane = document.getElementById("folderPaneBox");
                var fpcollapsed = folderPane.collapsed;
                var fpwidth = folderPane.width;

                var folderPaneSplitter = document.getElementById("folderpane_splitter");
                var fpsplitter = folderPaneSplitter.getAttribute("state");

                if (fpcollapsed == null)
                {
                    fpcollapsed = false;
                }
                dump("Saving fpcollapsed = "+fpcollapsed+"\n");
                prefs.setBoolPref("penelope."+uri+".window.fpcollapsed", fpcollapsed);
                dump("Saving fpwidth = "+fpwidth+"\n");
                prefs.setIntPref("penelope."+uri+".window.fpwidth", fpwidth);
                dump("Saving fpsplitter = "+fpsplitter+"\n");
                prefs.setCharPref("penelope."+uri+".window.fpsplitter", fpsplitter);

            }
            catch(e)
            {
                dump("Caught folder pane exception: "+e+"\n");
            }
        }

        try
        {
            var messagePane = document.getElementById("messagepanebox");
            var mpwidth = messagePane.width;
            var mpheight = messagePane.height;
            var threadPaneSplitter = document.getElementById("threadpane-splitter");
            var tpsplitter = threadPaneSplitter.getAttribute("state");

            dump("Saving tpsplitter = "+tpsplitter+"\n");
            prefs.setCharPref("penelope."+uri+".window.tpsplitter", tpsplitter);
            dump("Saving mpwidth = "+mpwidth+"\n");
            prefs.setIntPref("penelope."+uri+".window.mpwidth", mpwidth);
            dump("Saving mpheight = "+mpheight+"\n");
            prefs.setIntPref("penelope."+uri+".window.mpheight", mpheight);
        }
        catch(e)
        {
            dump("Caught thread pane exception: "+e+"\n");
        }
    }
    else
    {
        try
        {
            // Set the preferences for the folder window 
            dump("Saving dimentions as: "+window.innerWidth+", "+window.innerHeight+"\n");
            dump("Saving position as: "+window.screenX+", "+window.screenY+"\n");
            prefs.setIntPref("penelope.__folderwindow.width", window.innerWidth);
            prefs.setIntPref("penelope.__folderwindow.height", window.innerHeight);
            prefs.setIntPref("penelope.__folderwindow.screenX", window.screenX);
            prefs.setIntPref("penelope.__folderwindow.screenY", window.screenY);
        }
        catch(e)
        {
            dump("Exception: In folder win - "+ e+"\n");
        }
    }

    OnUnloadMessenger();
}

function FolderTreeOnClick(event)
{
    // we only care about button 0 (left click) events, and double-click
    //
    // We have to catch this during the single click event because otherwise
    // the mailnews code will get the single click event before we get the
    // double-click event, and we need to prevent further handling of the event.
    if (event.button != 0 || event.detail != 2)
        return;

    var folderTree = GetFolderTree();
    var row = {};
    var col = {};
    var elt = {};
    folderTree.treeBoxObject.getCellAt(event.clientX, event.clientY, row, col, elt);
    if (row.value >= 0 && elt.value != "twisty")
        FolderTreeDoubleClick(row.value, event);
}

function FolderTreeDoubleClick(folderIndex, event)
{
    var folderResource = GetFolderResource(GetFolderTree(), folderIndex);
    // Open a new msg window only if we are double clicking on
    // folders or newsgroups, not top-level servers
    if (folderResource && folderResource.isServer == false)
    {
        goMailbox(folderResource.Value, true);
        event.stopPropagation();
    }
}

function FolderTreeOnKeyPress(event)
{
    if (event.keyCode == event.DOM_VK_ENTER || event.keyCode == event.DOM_VK_RETURN)
    {
        var folderTree = GetFolderTree();
        FolderTreeDoubleClick(folderTree.currentIndex, event);
    }
}

var eatNextMouseClick = false;

function ThreadTreeOnMouseDown(event)
{
    // See if user Alt+clicked on a message summary
    if (event.button == 0 && event.altKey)
    {
        var threadTree = document.getElementById("threadTree"); 
        var treeBoxObj = threadTree.treeBoxObject;

        var row = {}, col= {}, type = {};
        treeBoxObj.getCellAt(event.clientX, event.clientY, row, col, type);
        if (row.value >= 0)
        {

            MailboxGroupSelect(threadTree, row.value, col.value, !event.shiftKey);
            
            // Don't let the mouse down continue as it will cause the
            // clicked item in some columns (e.g. Starred and Junk) to
            // change state
            event.stopPropagation();
            // Also, we need to eat the resulting mouse click as otherwise
            // it will cause the clicked item to only be selected after
            // we've done all of the special selection work
            eatNextMouseClick = true;
        }
    }
}

function ThreadTreeOnMouseClick(event)
{
    if (event.button == 0 && eatNextMouseClick)
    {
        eatNextMouseClick = false;
        event.stopPropagation();
    }
}

var lastKeyPress = 0;
var ttsBuffer;

function ThreadTreeOnKeyPress(event)
{
    if (!event.ctrlKey && !event.altKey)
    {
        var charCode = event.charCode;
        
        if (charCode >= 32)
        {
            event.stopPropagation();
            
            var now = Date.now();
            var findAgain = false;
            
            // Start search criteria from the beginning if it's
            // been more than a second since a key was pressed
            if (now > lastKeyPress + 1000)
            {
                // Period key after timeout is the "find again" hotkey
                //
                // Yeah, yeah, yeah, Shift+<period> is not necessarily the greater-than character (62)
                // on all keyboards, but key presses don't give you the key code and I don't feel like
                // writing an additional key down event handler for such a minor thing.  ;^>
                if (charCode == 46 || (event.shiftKey && charCode == 62 && ttsBuffer))
                    findAgain = true;
                else
                    ttsBuffer = "";
            }
            
            if (!findAgain)
            {
                lastKeyPress = now;
                
                // Encode the characters so as to not to be interpreted as RegEx specials (e.g. '*')
                ttsBuffer += "\\x" + charCode.toString(16);
            }
            
            var tree = document.getElementById("threadTree"); 
            var view = tree.view;
            var treeBoxObj = tree.treeBoxObject;
            var selection = view.selection;
            var numItems = view.rowCount;
            var subjectCol = treeBoxObj.columns.getNamedColumn("subjectCol");

            // If the mailbox is ascending sorted then search from top to bottom.
            // If descending or not sorted, then go bottom to top.
            // Reverse it for Date sorting as we want to search more recent messages first.
            var bSubjectSorted = (gDBView.sortType == nsMsgViewSortType.bySubject);
            var bWhoSorted = (gDBView.sortType == nsMsgViewSortType.byAuthor || gDBView.sortType == nsMsgViewSortType.byRecipient);
            var bAscendingSort = (gDBView.sortOrder == nsMsgViewSortOrder.ascending);
            var bDateSorted = (gDBView.sortType == nsMsgViewSortType.byDate);
            var row, inc;
            if (bAscendingSort && !bDateSorted)
            {
                row = 0;
                inc = 1;
            }
            else
            {
                row = numItems - 1;
                inc = -1;
            }
            // If doing a find again (user hit period key) then start at the item after the currently selected one 
            if (findAgain)
            {
                // Shift key held down means go the opposite direction
                if (event.shiftKey)
                    inc = -inc;
                row = selection.currentIndex + inc;
            }

            // Match the beginning of the text or white space, followed by zero or more punctuation characters, followed by the match string
            var matchExpression = new RegExp("(?:^|\\s)[\\x21-\\x2F\\x3A-\\x40\\x5B-\\x60\\x7B-\\x7E]*" + ttsBuffer, "i");
            var matchRow = -1;
            var senderColElement = document.getElementById("senderCol");
            var recipientColElement = document.getElementById("recipientCol");
            var whoCol;
            
            if (recipientColElement.hidden == false)
                whoCol = treeBoxObj.columns.getNamedColumn("recipientCol");
            else if (senderColElement.hidden == false)
                whoCol = treeBoxObj.columns.getNamedColumn("senderCol");
            
            for (; row >= 0 && row < numItems; row += inc)
            {
                // Don't bother searching Subject if its a secondary column
                // and we've already found a secondary match, or if
                // it isn't currently being shown
                if ((!bWhoSorted || matchRow < 0) && subjectCol)
                {
                    var subject = GetItemValue(view, row, subjectCol, nsMsgViewSortType.bySubject);
                    if (matchExpression.exec(subject))
                    {
                        if (!bWhoSorted)
                        {
                            matchRow = row;
                            break;
                        }
                        if (matchRow < 0)
                            matchRow = row;
                    }
                }
                
                // Don't bother searching Who if its a secondary column
                // and we've already found a secondary match, or if
                // it isn't currently being shown
                if ((bWhoSorted || matchRow < 0) && whoCol)
                {
                    var who = GetItemValue(view, row, whoCol, nsMsgViewSortType.byNone);
                    if (matchExpression.exec(who))
                    {
                        if (bWhoSorted)
                        {
                            matchRow = row;
                            break;
                        }
                        if (matchRow < 0)
                            matchRow = row;
                    }
                }
            }
            
            if (matchRow >= 0)
            {
                selection.select(matchRow);
                treeBoxObj.ensureRowIsVisible(matchRow);
            }
            else
            {
                var sound = Components.classes["@mozilla.org/sound;1"].createInstance(Components.interfaces.nsISound);
                sound.playSystemSound("beep");
            }
        }
    }
}

var groupSelectIndexArray;

var groupSelectColumnHandler = 
{
    getCellText:         function(row, col) {},
    getSortStringForRow: function(hdr) {},
    isString:            function() { return false; },
    getCellProperties:   function(row, col, props) {},
    getImageSrc:         function(row, col) { return null; },
    getSortLongForRow:   function(hdr) { return groupSelectIndexArray[hdr.messageKey]; }
}

var CreateDbObserver =
{
    observe: function(aMsgFolder, aTopic, aData)
    {
        gDBView.addColumnHandler("groupSelectCol", groupSelectColumnHandler);
    }
}

function GetCoreSubject(text)
{
    // Can't do this with a single reg ex as the "[Fwd: subject]" format
    // needs to strip off the trailing square bracket character

    var fwdRegEx = /\[fwd?:\s*(.*)\]/i;
    var initialBracketsRegEx = /^\[.*\]\s*(\S+.*)/i;    
    var prefixRegEx = /^\s*(?:(?:re|fwd?)[^a-z]*:\s*)+(.*)/i;
    while (true)
    {
        while (match = fwdRegEx.exec(text))
            text = match[1];

        while (match = initialBracketsRegEx.exec(text))
            text = match[1];

        if (match = prefixRegEx.exec(text))
            text = match[1];
        else
            break;
    }
    
    return text;
}

function GetItemValue(view, row, col, sortType)
{
    switch (sortType)
    {
    case nsMsgViewSortType.bySubject:
        return GetCoreSubject(view.getCellText(row, col));
        
    case nsMsgViewSortType.byThread:
        return view.isContainer(row);
        
    case nsMsgViewSortType.byPriority:
        var msgKey = gDBView.getKeyAt(row);
        var hdr = gDBView.db. GetMsgHdrForKey(msgKey);
        return hdr.priority;
        
    case nsMsgViewSortType.byFlagged:
        var msgKey = gDBView.getKeyAt(row);
        var hdr = gDBView.db. GetMsgHdrForKey(msgKey);
        return hdr.isFlagged;
        
    case nsMsgViewSortType.byJunkStatus:
        var msgKey = gDBView.getKeyAt(row);
        var hdr = gDBView.db. GetMsgHdrForKey(msgKey);
        var junkScoreStr = hdr.getStringProperty("junkscore");
        var junkThreshold = pref.getIntPref("mail.adaptivefilters.junk_threshold");
        return parseInt(junkScoreStr, 10) >= junkThreshold;
        
    case nsMsgViewSortType.byAttachments:
        var msgKey = gDBView.getKeyAt(row);
        var hdr = gDBView.db. GetMsgHdrForKey(msgKey);
        return (hdr.flags & 0x10000000);  // 0x10000000 is MSG_FLAG_ATTACHMENT
        
    default:
        return view.getCellText(row, col);
    }
}

function MailboxGroupSelect(tree, row, col, moveItems)
{
    var view = tree.view;
    var treeBoxObj = tree.treeBoxObject;
    var selection = view.selection;
    var sortType = ConvertColumnIDToSortType(col.id);
    var compareValue = GetItemValue(view, row, col, sortType);
    var numItems = view.rowCount;

    selection.selectEventsSuppressed = true;
    selection.clearSelection();

    var MatchBeforeNum = 0;
    var MatchAfterNum = 0;
    for (var r = 0; r < numItems; r++)
    {
        if (GetItemValue(view, r, col, sortType) == compareValue)
        {
            // Need to use rangedSelect() as it doesn't affect existing selection
            selection.rangedSelect(r, r, true);

            if (r < row)
            {
                MatchBeforeNum++;
            }
            else if (r > row)
            {
                MatchAfterNum++;
            }
        }
    }

    if (moveItems && (MatchBeforeNum || MatchAfterNum))
    {
        var NonMatchBeforeIndex = 0;
        var MatchBeforeIndex =    row - MatchBeforeNum;
        var NonMatchAfterIndex =  row + MatchAfterNum + 1;
        var MatchAfterIndex =     row + 1;

        groupSelectIndexArray = new Array();

        for (var r = 0; r < numItems; r++)
        {
            var newIndex = 0;
            if (GetItemValue(view, r, col, sortType) == compareValue)
            {
                if (r < row)
                {
                    newIndex = MatchBeforeIndex++;
                }
                else if (r > row)
                {
                    newIndex = MatchAfterIndex++;
                }
                else
                {
                    newIndex = row;
                }
            }
            else
            {
                if (r < row)
                {
                    newIndex = NonMatchBeforeIndex++;
                }
                else
                {
                    newIndex = NonMatchAfterIndex++;
                }
            }
            var msgKey = gDBView.getKeyAt(r);
            groupSelectIndexArray[msgKey] = newIndex;
        }

        var sortType = ConvertColumnIDToSortType("groupSelectCol");

        // Force sort to occur by thinking that the view is currently not sorted
        gDBView.sortType = nsMsgViewSortType.byNone;
        MsgSortThreadPane(sortType);
        // Consider not sorted when done
        gDBView.sortType = nsMsgViewSortType.byNone;

        groupSelectIndexArray = null;
    }

    selection.currentIndex = row;
    selection.selectEventsSuppressed = false;
}


function setNewMailSound()
{
    // Get the install location of the extension
    const id= "{D1D37B8A-4F3C-11DB-8373-B622A1EF5492}";
    var extDir = Components.classes["@mozilla.org/extensions/manager;1"]
                    .getService(Components.interfaces.nsIExtensionManager)
                    .getInstallLocation(id)
                    .getItemLocation(id);

    // Get the preferences object
    var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                getService(Components.interfaces.nsIPrefBranch);

    // Set the preferences for the sound to play for new mail
    var newmail = "file://"+extDir.path+"/newmail.wav";
    prefs.setIntPref("mail.biff.play_sound.type", 1);
    prefs.setCharPref("mail.biff.play_sound.url", newmail);
}

function goMailbox(folderUri, bUserAction)
{
    dump("selecting folder " + folderUri + "\n");

    // If this is a user action (e.g. double-click or Enter/Return), then don't consider the current
    // window a match because it will always match (because it is selected on the right)
    var win = checkIfWindowOpen(folderUri, bUserAction);
    if (win)
    {
        // focus the existing window
        dump("Window "+folderUri+" is open... changing focus\n");
        win.focus();
        return;
    }
    else
    {
        dump("Window "+folderUri+" is not open... creating\n");
    }
    // TODO - Decide whether to open a new window or just change the selection of the current window
    if (0)
    {
        var folderTree = GetFolderTree();
        var folderResource = RDF.GetResource(folderUri);
        var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);

        // before we can select a folder, we need to make sure it is "visible"
        // in the tree.  to do that, we need to ensure that all its
        // ancestors are expanded
        var folderIndex = EnsureFolderIndex(folderTree.builderView, msgFolder);

        ChangeSelection(folderTree, folderIndex);
    }
    else
    {
        toOpenWindowByType("mail:3pane", "chrome://messenger/content/messenger.xul");
        MsgOpenNewWindowForFolder(folderUri, -1);
    }
}

function OpenSpecialMailbox(flag)
{
    try
    {
        if (iterateSubfolders(gDBView.msgFolder.server.rootMsgFolder, flag))
        {
            return true;
        }
    }
    catch(e)
    {
    }

    return iterateSubfolders(accountManager.localFoldersServer.rootMsgFolder, flag);
}

function openInbox() 
{
    OpenSpecialMailbox(MSG_FOLDER_FLAG_INBOX);
}

function openOutbox() 
{
    OpenSpecialMailbox(MSG_FOLDER_FLAG_SENTMAIL);
}

function openJunk() 
{
    OpenSpecialMailbox(MSG_FOLDER_FLAG_JUNK);
}

function openTrash() 
{
    OpenSpecialMailbox(MSG_FOLDER_FLAG_TRASH);
}

function iterateSubfolders(folder, flag) 
{
    var isMatch = folder.flags & flag;

    if (isMatch > 0) 
    {
        var folderTree = GetFolderTree();
        var idx = folderTree.builderView.getIndexOfResource(folder);
        ChangeSelection(folderTree, idx);
        return true;
    }

    if (folder.hasSubFolders) 
    {
        var subfolders = folder.GetSubFolders();
        var done = false;

        while (!done) 
        {
            var subfolder = subfolders.currentItem().QueryInterface(Components.interfaces.nsIMsgFolder);
            if (iterateSubfolders(subfolder, flag))
            {
                return true;
            }
            try 
            {
                subfolders.next();
            }
            catch(e) 
            {
                done = true;
            }
        }
    }

    return false;
}

function addEmail()
{
    try 
    {
        if (gDBView.URIForFirstSelectedMessage) 
        {
            var hdr = messenger.msgHdrFromURI(gDBView.URIForFirstSelectedMessage);
            var hdrParser = Components.classes["@mozilla.org/messenger/headerparser;1"].getService(Components.interfaces.nsIMsgHeaderParser);
            var addresses = {};
            var names = {};
            var fullNames = {};
            var numAddresses = hdrParser.parseHeadersWithArray(hdr.author, addresses, names, fullNames);

            if (numAddresses <= 0)
            {
                alert("Add email to Addressbook failed");
            }

            var emailAddress = addresses.value[0];
            var displayName = names.value[0];
            window.openDialog("chrome://messenger/content/addressbook/abNewCardDialog.xul",
                              "",
                               "chrome,resizable=no,titlebar,modal,centerscreen",
                              {primaryEmail: emailAddress, displayName:displayName});
        }
    }
    catch(e) { alert("No message selected"); }
}

function delayedOnLoadPenelopeColumns()
{
    // Get the element nodes for the messanger window columns
    var threadTree = document.getElementById("threadTree");
    var sizeCol = document.getElementById("sizeCol");
    var priorityCol = document.getElementById("priorityCol");
    var senderCol = document.getElementById("senderCol");
    var recipientCol = document.getElementById("recipientCol");
    var subjectCol = document.getElementById("subjectCol");
    var attachmentCol = document.getElementById("attachmentCol");
    var statusCol = document.getElementById("statusCol");
    var dateCol = document.getElementById("dateCol");
    var threadCol = document.getElementById("threadCol");
    var tagsCol = document.getElementById("tagsCol");
    var junkCol = document.getElementById("junkStatusCol");
    var unreadCol = document.getElementById("unreadButtonColHeader");
    var idCol = document.getElementById("idCol");
    var flaggedCol = document.getElementById("flaggedCol");

    try 
    {
        // Remap the messanger window columns to the Penelope defaults
        // and change the visibility status of some of the columns
        statusCol.setAttribute("hidden","false");
        sizeCol.setAttribute("hidden","false");
        priorityCol.setAttribute("hidden","false");
        threadCol.setAttribute("hidden","true");
        tagsCol.setAttribute("hidden","false");
        flaggedCol.setAttribute("hidden","true");
        unreadCol.setAttribute("hidden","true");

        // Get the localized "Who" string
        //var strbundle=document.getElementById("penelopeStrings");
        //var who=strbundle.getString("who");

        // Update the sender and recipient columns to display "Who" 
        //senderCol.setAttribute("label", who);
        //recipientCol.setAttribute("label", who);

        // FIXME Somehow the junk column gets remapped after this
        threadTree._reorderColumn(subjectCol, idCol, true);
        threadTree._reorderColumn(junkCol, subjectCol, true);
        threadTree._reorderColumn(sizeCol, junkCol, true);
        threadTree._reorderColumn(dateCol, sizeCol, true);
        threadTree._reorderColumn(recipientCol, dateCol, true);
        threadTree._reorderColumn(senderCol, recipientCol, true);
        threadTree._reorderColumn(tagsCol, senderCol, true);
        threadTree._reorderColumn(attachmentCol, tagsCol, true);
        threadTree._reorderColumn(priorityCol, attachmentCol, true);
        threadTree._reorderColumn(unreadCol, priorityCol, true);
        threadTree._reorderColumn(statusCol, unreadCol, true);
    }
    catch(e) 
    { 
        dump("Column reorder error:"+e+"\n"); 
    }
}

function penelopeUpdatePriorityMenu()
{
    try 
    {
        if (gDBView.nsIMsgDBHdr) 
        {
            var priorityMenu = document.getElementById('penelopePriorityMenu' );
            priorityMenu.getElementsByAttribute( "checked", 'true' )[0].removeAttribute('checked');
            priorityMenu.getElementsByAttribute( "value", gDBView.nsIMsgDBHdr.priority )[0].setAttribute('checked', 'true');
        }
    }
    catch(e) 
    { 
        dump("Error in penelopeUpdatePriorityMenu:"+e+"\n"); 
    }
}

function penelopePriorityMenuSelect(target)
{
    gDBView.nsIMsgDBHdr.setPriorityString(target.getAttribute('value'));
}

function penelopeStatusMenuSelect(target)
{
    switch (target.getAttribute('value'))
    {
    case "Unread":
    case "Read":
    case "Replied":
    case "Forwarded":
    case "Redirected":
    case "Sent":
    case "Unsent":
        alert("Not implemented");
        break;
    default:
        alert("Unknown status");
    }
}


function penelopeRedirect()
{
  alert("Not implemented");
}

function penelopeFolderPaneSelectionChange()
{
    FolderPaneSelectionChange();

    var topwin = document.getElementById("messengerWindow");
    if (topwin.getAttribute("folderwin") == "true")
    {
        document.title = "Penelope";
    }
}
