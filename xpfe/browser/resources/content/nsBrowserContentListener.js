/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
 */

const mediatorContractId = "@mozilla.org/rdf/datasource;1?name=window-mediator";
const nsIWebBrowserChrome = Components.interfaces.nsIWebBrowserChrome;

function nsBrowserContentListener(toplevelWindow, contentWindow)
{
    // this one is not as easy as you would hope.
    // need to convert toplevelWindow to an XPConnected object, instead
    // of a DOM-based object, to be able to QI() it to nsIXULWindow
    
    this.init(toplevelWindow, contentWindow);
}

/* implements nsIURIContentListener */

nsBrowserContentListener.prototype =
{
    init: function(toplevelWindow, contentWindow)
    {
        this.toplevelWindow = toplevelWindow;
        this.contentWindow = contentWindow;

        // hook up the whole parent chain thing
        var windowDocShell = this.convertWindowToDocShell(toplevelWindow);
        if (windowDocShell)
            windowDocshell.parentURIContentListener = this;
    
        var registerWindow = false;
        try {          
          var treeItem = contentWindow.docShell.QueryInterface(Components.interfaces.nsIDocShellTreeItem);
          var treeOwner = treeItem.treeOwner;
          var interfaceRequestor = treeOwner.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
          var webBrowserChrome = interfaceRequestor.getInterface(nsIWebBrowserChrome);
          if (webBrowserChrome)
          {
            var chromeFlags = webBrowserChrome.chromeFlags;
            var res = chromeFlags & nsIWebBrowserChrome.CHROME_ALL;
            var res2 = chromeFlags & nsIWebBrowserChrome.CHROME_DEFAULT;
            if ( res == nsIWebBrowserChrome.CHROME_ALL || res2 == nsIWebBrowserChrome.CHROME_DEFAULT)
            {             
              registerWindow = true;
            }
         }
       } catch (ex) {} 

        // register ourselves
       if (registerWindow)
       {
        var uriLoader = Components.classes["@mozilla.org/uriloader;1"].getService(Components.interfaces.nsIURILoader);
        uriLoader.registerContentListener(this);
       }
    },
    close: function()
    {
        this.contentWindow = null;
        var uriLoader = Components.classes["@mozilla.org/uriloader;1"].getService(Components.interfaces.nsIURILoader);

        uriLoader.unRegisterContentListener(this);
    },
    QueryInterface: function(iid)
    {
        if (iid.equals(Components.interfaces.nsIURIContentListener))
            return this;
        if (iid.equals(Components.interfaces.nsISupportsWeakReference))
            return this;
        throw Components.results.NS_NOINTERFACE;

    },
    onStartURIOpen: function(uri)
    {
        // ignore and don't abort
        return false;
    },

    doContent: function(contentType, command, request, contentHandler)
    {
        // forward the doContent to our content area webshell
        var docShell = this.contentWindow.docShell;
        var contentListener;
        try {
            contentListener =
                docShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                .getInterface(Components.interfaces.nsIURIContentListener);
        } catch (ex) {
            dump(ex);
        }
        
        if (!contentListener) return false;
        
        return contentListener.doContent(contentType, command, request, contentHandler);
        
    },

    isPreferred: function(contentType, command, desiredContentType)
    {
        // seems like we should be getting this from helper apps or something
        switch(contentType) {
            case "text/html":
            case "text/xul":
            case "text/rdf":
            case "text/xml":
            case "text/css":
            case "image/gif":
            case "image/jpeg":
            case "image/png":
            case "image/tiff":
            case "text/plain":
            case "application/http-index-format":
                return true;
        }
        return false;
    },
    canHandleContent: function(contentType, command, desiredContentType)
    {
        var docShell = this.contentWindow.docShell;
        var contentListener;
        try {
            contentListener =
                docShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor).getInterface(Components.interfaces.nsIURIContentListener);
        } catch (ex) {
            dump(ex);
        }
        if (!contentListener) return false;
        
        return contentListener.canHandleContent(contentType, command, desiredContentType);
    },
    convertWindowToDocShell: function(win) {
        // don't know how to do this
        return null;
    },
    loadCookie: null,
    parentContentListener: null,
}
