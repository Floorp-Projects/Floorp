/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Seth Spitzer <sspitzer@netscape.com>
 * Shane Culpepper <pepper@netscape.com>
 */

var remoteControlContractID	= "@mozilla.org/browser/remote-browser-control;1";

var nsIRemoteBrowserControl = Components.interfaces.nsIRemoteBrowserControl;

function BrowserRemoteControl() {
  return browserRemoteControl;
}

// We need to implement nsIRemoteBrowserControl
	
var browserRemoteControl = {
    openURL: function(aURL, newWindow)
    {
        dump("openURL(" + aURL + "," + newWindow + ")\n");
        return;
    },
    
    openFile: function(aURL)
    {
        dump("openFile(" + aURL + ")\n");
        return;
    },

    saveAs: function(aURL)
    {
        dump("saveAs(" + aURL + ")\n");
        var saveAsDialog = Components.classes["@mozilla.org/xfer;1"].getService();
        saveAsDialog = saveAsDialog.QueryInterface(Components.interfaces.nsIMsgComposeService);
        if ( !saveAsDialog ) return(false);
       
        saveAsDialog.SelectFileAndTransferLocation(aURL, null); 
        return(true);
    },

    mailTo: function(mailToList)
    {
        dump("mailto(" + mailToList + ")\n");
        var msgComposeService = Components.classes["@mozilla.org/messengercompose;1"].getService();
        msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);
        if ( !msgComposeService ) return(false);

        params = Components.classes["@mozilla.org/messengercompose/composeparams;1"].createInstance(Components.interfaces.nsIMsgComposeParams);
        if (params)
        {
            params.type = Components.interfaces.nsIMsgCompType.New;
            params.format = Components.interfaces.nsIMsgCompFormat.Default;
            composeFields = Components.classes["@mozilla.org/messengercompose/composefields;1"].createInstance(Components.interfaces.nsIMsgCompFields);
            if (composeFields)
        {
                if (mailToList)
                   params.composeFields = composeFields;
                msgComposeService.OpenComposeWindowWithParams(null, params);
            }
        }
        return(true);
    },

    addBookmark: function(aURL, aTitle)
    {
        dump("addBookmark(" + aURL + "," + aTitle + ")\n");
        var bookmarkService = Components.classes["@mozilla.org/browser/bookmarks-service;1"].getService();
        bookmarkService = bookmarkService.QueryInterface(Components.interfaces.nsIBookmarksService);
        if ( !bookmarkService ) return(false);

        if ( !aURL ) return(false);
        if ( aTitle )
        {
            bookmarkService.AddBookmark(aURL, aTitle, bookmarkService.BOOKMARK_DEFAULT_TYPE, null );
        }
        else
        {
            bookmarkService.AddBookmark(aURL, null, bookmarkService.BOOKMARK_DEFAULT_TYPE);
        }
        return(true);
    }
};

var module = {
    registerSelf: function (compMgr, fileSpec, location, type) {
        dump("registerSelf for remoteControl\n");
        compMgr.registerComponentWithType(this.myCID,
                                          "Browser Remote Control",
                                          remoteControlContractID,
                                          fileSpec, location, true, true, 
                                          type);
    },

    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.myCID))
            throw Components.results.NS_ERROR_NO_INTERFACE;
        
        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        return this.myFactory;
    },

    canUnload: function () {
    },

    myCID: Components.ID("{97c8d0de-1dd1-11b2-bc64-86a3aaf8f5c5}"),

    myFactory: {
        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;
            
            if (!(iid.equals(nsIRemoteBrowserControl) ||
                  iid.equals(Components.interfaces.nsISupports))) {
                throw Components.results.NS_ERROR_INVALID_ARG;
            }

            return new BrowserRemoteControl();
        }
    }
};

function NSGetModule(compMgr, fileSpec) { return module; }
