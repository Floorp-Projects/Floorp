/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   William A. ("PowerGUI") Law <law@netscape.com>
 *   Blake Ross <blakeross@telocity.com>
 *   Gervase Markham <gerv@gerv.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*------------------------------ nsContextMenu ---------------------------------
|   This JavaScript "class" is used to implement the browser's content-area    |
|   context menu.                                                              |
|                                                                              |
|   For usage, see references to this class in navigator.xul.                  |
|                                                                              |
|   Currently, this code is relatively useless for any other purpose.  In the  |
|   longer term, this code will be restructured to make it more reusable.      |
------------------------------------------------------------------------------*/
function nsContextMenu( xulMenu ) {
    this.target         = null;
    this.menu           = null;
    this.popupURL       = null;
    this.onTextInput    = false;
    this.onImage        = false;
    this.onLink         = false;
    this.onMailtoLink   = false;
    this.onSaveableLink = false;
    this.onMetaDataItem = false;
    this.onMathML       = false;
    this.link           = false;
    this.inFrame        = false;
    this.hasBGImage     = false;
    this.isTextSelected = false;
    this.inDirList      = false;
    this.shouldDisplay  = true;

    // Initialize new menu.
    this.initMenu( xulMenu );
}

// Prototype for nsContextMenu "class."
nsContextMenu.prototype = {
    // onDestroy is a no-op at this point.
    onDestroy : function () {
    },
    // Initialize context menu.
    initMenu : function ( popup ) {
        const xulNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
        if ( document.popupNode.namespaceURI == xulNS ) {
          this.shouldDisplay = false;
          return;
        }
        // Save menu.
        this.menu = popup;

        // Get contextual info.
        this.setTarget( document.popupNode );
        
        this.isTextSelected = this.isTextSelection();

        this.initPopupURL();

        // Initialize (disable/remove) menu items.
        this.initItems();
    },
    initItems : function () {
        this.initOpenItems();
        this.initNavigationItems();
        this.initViewItems();
        this.initMiscItems();
        this.initSaveItems();
        this.initClipboardItems();
        this.initMetadataItems();
    },
    initOpenItems : function () {
        var showOpen = this.onSaveableLink || ( this.inDirList && this.onLink );

        this.showItem( "context-openlink", showOpen );
        this.showItem( "context-openlinkintab", showOpen );

        this.showItem( "context-sep-open", showOpen );
    },
    initNavigationItems : function () {
        // Back determined by canGoBack broadcaster.
        this.setItemAttrFromNode( "context-back", "disabled", "canGoBack" );

        // Forward determined by canGoForward broadcaster.
        this.setItemAttrFromNode( "context-forward", "disabled", "canGoForward" );

        var showNav = !( this.isTextSelected || this.onLink || this.onImage || this.onTextInput );
        
        this.showItem( "context-back", showNav );
        this.showItem( "context-forward", showNav );

        this.showItem( "context-reload", showNav );
        
        this.showItem( "context-stop", showNav );
        this.showItem( "context-sep-stop", showNav );

        // XXX: Stop is determined in navigator.js; the canStop broadcaster is broken
        //this.setItemAttrFromNode( "context-stop", "disabled", "canStop" );
    },
    initSaveItems : function () {
        this.showItem( "context-savepage", 
                       !( this.inDirList || this.isTextSelected || this.onTextInput || this.onStandaloneImage ||
                         (this.onLink && this.onImage)));

        // Save link depends on whether we're in a link.
        this.showItem( "context-savelink", this.onSaveableLink );

        // Save image depends on whether there is one.
        this.showItem( "context-saveimage", this.onImage || this.onStandaloneImage);
        
        this.showItem( "context-sendimage", this.onImage || this.onStandaloneImage);
    },
    initViewItems : function () {
        // View source is always OK, unless in directory listing.
        this.showItem( "context-viewpartialsource-selection", this.isTextSelected && !this.onTextInput );
        this.showItem( "context-viewpartialsource-mathml", this.onMathML && !this.isTextSelected );

        var showView = !( this.inDirList || this.onImage || this.isTextSelected || this.onLink || this.onTextInput );

        this.showItem( "context-viewsource", showView );
        this.showItem( "context-viewinfo", showView );

        this.showItem( "context-sep-properties", !( this.inDirList || this.isTextSelected || this.onTextInput ) );
        // Set As Wallpaper depends on whether an image was clicked on, and only works on Windows.
        var isWin = navigator.appVersion.indexOf("Windows") != -1;
        this.showItem( "context-setWallpaper", isWin && (this.onImage || this.onStandaloneImage));

        this.showItem( "context-sep-image", this.onImage || this.onStandaloneImage);

        if( isWin && this.onImage )
            // Disable the Set As Wallpaper menu item if we're still trying to load the image
          this.setItemAttr( "context-setWallpaper", "disabled", (("complete" in this.target) && !this.target.complete) ? "true" : null );

        this.showItem( "context-fitimage", this.onStandaloneImage && _content.document.imageResizingEnabled );
        if ( this.onStandaloneImage && _content.document.imageResizingEnabled ) {
          this.setItemAttr( "context-fitimage", "disabled", _content.document.imageIsOverflowing ? null : "true");
          this.setItemAttr( "context-fitimage", "checked", _content.document.imageIsResized ? "true" : null);
        }

        // View Image depends on whether an image was clicked on.
        this.showItem( "context-viewimage", this.onImage && !this.onStandaloneImage);

        // View background image depends on whether there is one.
        this.showItem( "context-viewbgimage", showView && !this.onStandaloneImage);
        this.showItem( "context-sep-viewbgimage", showView && !this.onStandaloneImage);
        this.setItemAttr( "context-viewbgimage", "disabled", this.hasBGImage ? null : "true");
    },
    initMiscItems : function () {
        // Use "Bookmark This Link" if on a link.
        this.showItem( "context-bookmarkpage", !( this.isTextSelected || this.onTextInput || this.onStandaloneImage ) );
        this.showItem( "context-bookmarklink", this.onLink && !this.onMailtoLink );
        this.showItem( "context-searchselect", this.isTextSelected && !this.onTextInput );
        this.showItem( "frame", this.inFrame );
        this.showItem( "frame-sep", this.inFrame );
        var blocking = true;
        if (this.popupURL)
          try {
            const PM = Components.classes["@mozilla.org/PopupWindowManager;1"]
                       .getService(Components.interfaces.nsIPopupWindowManager);
            blocking = PM.testPermission(this.popupURL) ==
                       Components.interfaces.nsIPopupWindowManager.DENY_POPUP;
          } catch (e) {
          }

        this.showItem( "popupwindow-reject", this.popupURL && !blocking);
        this.showItem( "popupwindow-allow", this.popupURL && blocking);
        this.showItem( "context-sep-popup", this.popupURL);
    },
    initClipboardItems : function () {

        // Copy depends on whether there is selected text.
        // Enabling this context menu item is now done through the global
        // command updating system
        // this.setItemAttr( "context-copy", "disabled", !this.isTextSelected() );

        goUpdateGlobalEditMenuItems();

        this.showItem( "context-undo", this.onTextInput );
        this.showItem( "context-redo", this.onTextInput );
        this.showItem( "context-sep-undo", this.onTextInput );
        this.showItem( "context-cut", this.onTextInput );
        this.showItem( "context-copy", this.isTextSelected || this.onTextInput);
        this.showItem( "context-paste", this.onTextInput );
        this.showItem( "context-delete", this.onTextInput );
        this.showItem( "context-sep-paste", this.onTextInput );
        this.showItem( "context-selectall", true );
        this.showItem( "context-sep-selectall", this.isTextSelected && !this.onTextInput );
        // In a text area there will be nothing after select all, so we don't want a sep
        // Otherwise, if there's text selected then there are extra menu items
        // (search for selection and view selection source), so we do want a sep

        // XXX dr
        // ------
        // nsDocumentViewer.cpp has code to determine whether we're
        // on a link or an image. we really ought to be using that...

        // Copy email link depends on whether we're on an email link.
        this.showItem( "context-copyemail", this.onMailtoLink );

        // Copy link location depends on whether we're on a link.
        this.showItem( "context-copylink", this.onLink );
        this.showItem( "context-sep-copylink", this.onLink );

        // Copy image location depends on whether we're on an image.
        this.showItem( "context-copyimage", this.onImage );
        this.showItem( "context-sep-copyimage", this.onImage );
    },
    initMetadataItems : function () {
        // Show if user clicked on something which has metadata.
        this.showItem( "context-metadata", this.onMetaDataItem );
    },
    // Set various context menu attributes based on the state of the world.
    setTarget : function ( node ) {
        // Initialize contextual info.
        this.onImage    = false;
        this.onStandaloneImage = false;
        this.onMetaDataItem = false;
        this.onTextInput = false;
        this.imageURL   = "";
        this.onLink     = false;
        this.onMathML   = false;
        this.inFrame    = false;
        this.hasBGImage = false;
        this.bgImageURL = "";

        // Remember the node that was clicked.
        this.target = node;

        // See if the user clicked on an image.
        if ( this.target.nodeType == Node.ELEMENT_NODE ) {
             if ( this.target instanceof Components.interfaces.nsIImageLoadingContent &&
                  this.target.currentURI != null ) {
                this.onImage = true;
                this.imageURL = this.target.currentURI.spec;

                var documentType = window._content.document.contentType;
                if ( documentType.substr(0,6) == "image/" )
                    this.onStandaloneImage = true;
             } else if ( this.target.localName.toUpperCase() == "INPUT") {
               type = this.target.getAttribute("type");
               this.onTextInput = this.isTargetATextBox(this.target);
            } else if ( this.target.localName.toUpperCase() == "TEXTAREA" ) {
                 this.onTextInput = true;
            } else if ( this.target.localName.toUpperCase() == "HTML" ) {
               // pages with multiple <body>s are lame. we'll teach them a lesson.
               var bodyElt = this.target.ownerDocument.getElementsByTagName("body")[0];
               if ( bodyElt ) {
                 var computedURL = this.getComputedURL( bodyElt, "background-image" );
                 if ( computedURL ) {
                   this.hasBGImage = true;
                   this.bgImageURL = this.makeURLAbsolute( bodyElt.baseURI,
                                                           computedURL );
                 }
               }
            } else if ( "HTTPIndex" in _content &&
                        _content.HTTPIndex instanceof Components.interfaces.nsIHTTPIndex ) {
                this.inDirList = true;
                // Bubble outward till we get to an element with URL attribute
                // (which should be the href).
                var root = this.target;
                while ( root && !this.link ) {
                    if ( root.tagName == "tree" ) {
                        // Hit root of tree; must have clicked in empty space;
                        // thus, no link.
                        break;
                    }
                    if ( root.getAttribute( "URL" ) ) {
                        // Build pseudo link object so link-related functions work.
                        this.onLink = true;
                        this.link = { href : root.getAttribute("URL"),
                                      getAttribute: function (attr) {
                                          if (attr == "title") {
                                              return root.firstChild.firstChild.getAttribute("label");
                                          } else {
                                              return "";
                                          }
                                      }
                                    };
                        // If element is a directory, then you can't save it.
                        if ( root.getAttribute( "container" ) == "true" ) {
                            this.onSaveableLink = false;
                        } else {
                            this.onSaveableLink = true;
                        }
                    } else {
                        root = root.parentNode;
                    }
                }
            }
        }

        // We have meta data on images.
        this.onMetaDataItem = this.onImage;
        
        // See if the user clicked on MathML
        const NS_MathML = "http://www.w3.org/1998/Math/MathML";
        if ((this.target.nodeType == Node.TEXT_NODE &&
             this.target.parentNode.namespaceURI == NS_MathML)
             || (this.target.namespaceURI == NS_MathML))
          this.onMathML = true;

        // See if the user clicked in a frame.
        if ( this.target.ownerDocument != window._content.document ) {
            this.inFrame = true;
        }
        
        // Bubble out, looking for items of interest
        var elem = this.target;
        while ( elem ) {
            if ( elem.nodeType == Node.ELEMENT_NODE ) {
                var localname = elem.localName.toUpperCase();
                
                // Link?
                if ( !this.onLink && 
                    ( (localname === "A" && elem.href) ||
                      localname === "AREA" ||
                      localname === "LINK" ||
                      elem.getAttributeNS( "http://www.w3.org/1999/xlink", "type") == "simple" ) ) {
                    // Clicked on a link.
                    this.onLink = true;
                    this.onMetaDataItem = true;
                    // Remember corresponding element.
                    this.link = elem;
                    this.onMailtoLink = this.isLinkType( "mailto:", this.link );
                    // Remember if it is saveable.
                    this.onSaveableLink = this.isLinkSaveable( this.link );
                }
                
                // Text input?
                if ( !this.onTextInput ) {
                    // Clicked on a link.
                    this.onTextInput = this.isTargetATextBox(elem);
                }
                
                // Metadata item?
                if ( !this.onMetaDataItem ) {
                    // We currently display metadata on anything which fits
                    // the below test.
                    if ( ( localname === "BLOCKQUOTE" && 'cite' in elem && elem.cite)  ||
                         ( localname === "Q" && 'cite' in elem && elem.cite)           ||
                         ( localname === "TABLE" && 'summary' in elem && elem.summary) ||
                         ( ( localname === "INS" || localname === "DEL" ) &&
                           ( ( 'cite' in elem && elem.cite ) ||
                             ( 'dateTime' in elem && elem.dateTime ) ) )               ||
                         ( 'title' in elem && elem.title )                             ||
                         ( 'lang' in elem && elem.lang ) ) {
                        dump("On metadata item.\n");
                        this.onMetaDataItem = true;
                    }
                }

                // Background image?  Don't bother if we've already found a 
                // background image further down the hierarchy.  Otherwise,
                // we look for the computed background-image style.
                if ( !this.hasBGImage ) {
                    var bgImgUrl = this.getComputedURL( elem, "background-image" );
                    if ( bgImgUrl ) {
                        this.hasBGImage = true;
                        this.bgImageURL = this.makeURLAbsolute( elem.baseURI,
                                                                bgImgUrl );
                    }
                }
            }
            elem = elem.parentNode;    
        }
    },
    initPopupURL: function() {
      // quick check: if no opener, it can't be a popup
      if (!window.content.opener)
        return;
      try {
        var show = false;
        // is it a popup window?
        const CI = Components.interfaces;
        var xulwin = window
                    .QueryInterface(CI.nsIInterfaceRequestor)
                    .getInterface(CI.nsIWebNavigation)
                    .QueryInterface(CI.nsIDocShellTreeItem)
                    .treeOwner
                    .QueryInterface(CI.nsIInterfaceRequestor)
                    .getInterface(CI.nsIXULWindow);
        if (xulwin.contextFlags &
            CI.nsIWindowCreator2.PARENT_IS_LOADING_OR_RUNNING_TIMEOUT) {
          // do the pref settings allow site-by-site popup management?
          const PB = Components.classes["@mozilla.org/preferences-service;1"]
                     .getService(CI.nsIPrefBranch);
          show = !PB.getBoolPref("dom.disable_open_during_load");
        }
        if (show) {
          // initialize popupURL
          const IOS = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(CI.nsIIOService);
          var opener = new XPCNativeWrapper(window.content, "opener").opener;
          var location = new XPCNativeWrapper(opener, "location").location;
          this.popupURL = IOS.newURI(location.href, null, null);

          // but cancel if it's an unsuitable URL
          const PM = Components.classes["@mozilla.org/PopupWindowManager;1"]
                     .getService(CI.nsIPopupWindowManager);
        }
      } catch(e) {
      }
    },
    // Returns the computed style attribute for the given element.
    getComputedStyle: function( elem, prop ) {
         return elem.ownerDocument.defaultView.getComputedStyle( elem, '' ).getPropertyValue( prop );
    },
    // Returns a "url"-type computed style attribute value, with the url() stripped.
    getComputedURL: function( elem, prop ) {
         var url = elem.ownerDocument.defaultView.getComputedStyle( elem, '' ).getPropertyCSSValue( prop );
         return ( url.primitiveType == CSSPrimitiveValue.CSS_URI ) ? url.getStringValue() : null;
    },
    // Returns true iff clicked on link is saveable.
    isLinkSaveable : function ( link ) {
        // We don't do the Right Thing for news/snews yet, so turn them off
        // until we do.
        return !(this.isLinkType( "mailto:" , link )     ||
                 this.isLinkType( "javascript:" , link ) ||
                 this.isLinkType( "news:", link )        || 
                 this.isLinkType( "snews:", link ) ); 
    },
    // Returns true iff clicked on link is of type given.
    isLinkType : function ( linktype, link ) {        
        try {
            // Test for missing protocol property.
            if ( !link.protocol ) {
                // We must resort to testing the URL string :-(.
                var protocol;
                if ( link.href ) {
                    protocol = link.href.substr( 0, linktype.length );
                } else {
                    protocol = link.getAttributeNS("http://www.w3.org/1999/xlink","href");
                    if ( protocol ) {
                        protocol = protocol.substr( 0, linktype.length );
                    }
                }
                return protocol.toLowerCase() === linktype;        
            } else {
                // Presume all but javascript: urls are saveable.
                return link.protocol.toLowerCase() === linktype;
            }
        } catch (e) {
            // something was wrong with the link,
            // so we won't be able to save it anyway
            return false;
        }
    },
    // Block popup windows
    rejectPopupWindows: function(andClose) {
      const PM = Components.classes["@mozilla.org/PopupWindowManager;1"]
                 .getService(Components.interfaces.nsIPopupWindowManager);
      PM.add(this.popupURL, false);
      if (andClose) {
        const OS = Components.classes["@mozilla.org/observer-service;1"]
                   .getService(Components.interfaces.nsIObserverService);
        OS.notifyObservers(window, "popup-perm-close", this.popupURL.spec);
      }
    },
    // Unblock popup windows
    allowPopupWindows: function() {
      const PM = Components.classes["@mozilla.org/PopupWindowManager;1"]
                 .getService(Components.interfaces.nsIPopupWindowManager);
      PM.add(this.popupURL, true);
    },
    // Open linked-to URL in a new window.
    openLink : function () {
        // Determine linked-to URL.
        openNewWindowWith( this.linkURL(), true );
    },
    // Open linked-to URL in a new tab.
    openLinkInTab : function () {
        // Determine linked-to URL.
        openNewTabWith( this.linkURL(), true, false );
    },
    // Open frame in a new tab.
    openFrameInTab : function () {
        // Determine linked-to URL.
        openNewTabWith( this.target.ownerDocument.location.href );
    },
    // Reload clicked-in frame.
    reloadFrame : function () {
        this.target.ownerDocument.location.reload();
    },
    // Open clicked-in frame in its own window.
    openFrame : function () {
        openNewWindowWith( this.target.ownerDocument.location.href );
    },
    // Open clicked-in frame in the same window
    showOnlyThisFrame : function () {
        window.loadURI(this.target.ownerDocument.location.href);
    },
    // View Partial Source
    viewPartialSource : function ( context ) {
        var focusedWindow = document.commandDispatcher.focusedWindow;
        if (focusedWindow == window)
          focusedWindow = _content;
        var docCharset = null;
        if (focusedWindow)
          docCharset = "charset=" + focusedWindow.document.characterSet;

        // "View Selection Source" and others such as "View MathML Source"
        // are mutually exclusive, with the precedence given to the selection
        // when there is one
        var reference = null;
        if (context == "selection")
          reference = focusedWindow.__proto__.getSelection.call(focusedWindow);
        else if (context == "mathml")
          reference = this.target;
        else
          throw "not reached";

        var docUrl = null; // unused (and play nice for fragments generated via XSLT too)
        window.openDialog("chrome://navigator/content/viewPartialSource.xul",
                          "_blank", "scrollbars,resizable,chrome,dialog=no",
                          docUrl, docCharset, reference, context);
    },
    // Open new "view source" window with the frame's URL.
    viewFrameSource : function () {
        BrowserViewSourceOfDocument(this.target.ownerDocument);
    },
    viewInfo : function () {
        BrowserPageInfo();
    },
    viewFrameInfo : function () {
        BrowserPageInfo(this.target.ownerDocument);
    },
    toggleImageSize : function () {
        _content.document.toggleImageSize();
    },
    // Change current window to the URL of the image.
    viewImage : function () {
        openTopWin( this.imageURL );
    },
    // Change current window to the URL of the background image.
    viewBGImage : function () {
        openTopWin( this.bgImageURL );
    },
    setWallpaper: function() {
      // Confirm since it's annoying if you hit this accidentally.
      var promptService       = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
      var gNavigatorBundle    = document.getElementById("bundle_navigator");
      var promptTitle         = gNavigatorBundle.getString("wallpaperConfirmTitle");
      var promptMsg           = gNavigatorBundle.getString("wallpaperConfirmMsg");
      var promptConfirmButton = gNavigatorBundle.getString("wallpaperConfirmButton");

      var buttonPressed = promptService.confirmEx(window, promptTitle, promptMsg,
                                                   (promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_0) +
                                                   (promptService.BUTTON_TITLE_CANCEL    * promptService.BUTTON_POS_1),
                                                   promptConfirmButton, null, null, null, {value:0});
 
      if (buttonPressed != 0)
        return;

      var winhooks = Components.classes[ "@mozilla.org/winhooks;1" ].
                       getService(Components.interfaces.nsIWindowsHooks);
      
      winhooks.setImageAsWallpaper(this.target, false);
    },    
    // Save URL of clicked-on frame.
    saveFrame : function () {
        saveDocument( this.target.ownerDocument );
    },
    // Save URL of clicked-on link.
    saveLink : function () {
        saveURL( this.linkURL(), this.linkText(), null, true );
    },
    // Save URL of clicked-on image.
    saveImage : function () {
        saveURL( this.imageURL, null, "SaveImageTitle", false );
    },
    // Generate email address and put it on clipboard.
    copyEmail : function () {
        // Copy the comma-separated list of email addresses only.
        // There are other ways of embedding email addresses in a mailto:
        // link, but such complex parsing is beyond us.
        var url = this.linkURL();
        var qmark = url.indexOf( "?" );
        var addresses;
        
        if ( qmark > 7 ) {                   // 7 == length of "mailto:"
            addresses = url.substring( 7, qmark );
        } else {
            addresses = url.substr( 7 );
        }

        // Let's try to unescape it using a character set
        // in case the address is not ASCII.
        try {
          var ownerDocument = new XPCNativeWrapper(this.target, "ownerDocument").ownerDocument;
          var characterSet = new XPCNativeWrapper(ownerDocument, "characterSet").characterSet;
          const textToSubURI = Components.classes["@mozilla.org/intl/texttosuburi;1"]
                                         .getService(Components.interfaces.nsITextToSubURI);
          addresses = textToSubURI.unEscapeNonAsciiURI(characterSet, addresses);
        }
        catch(ex) {
          // Do nothing.
        }

        var clipboard = this.getService( "@mozilla.org/widget/clipboardhelper;1",
                                         Components.interfaces.nsIClipboardHelper );
        clipboard.copyString(addresses);
    },    
    addBookmark : function() {
      var docshell = document.getElementById( "content" ).webNavigation;
      BookmarksUtils.addBookmark( docshell.currentURI.spec,
                                  docshell.document.title,
                                  docshell.document.charset,
                                  false );
    },
    addBookmarkForFrame : function() {
      var doc = this.target.ownerDocument;
      var uri = doc.location.href;
      var title = doc.title;
      if ( !title )
        title = uri;
      BookmarksUtils.addBookmark( uri,
                                  title,
                                  doc.charset,
                                  false );
    },
    // Open Metadata window for node
    showMetadata : function () {
        window.openDialog(  "chrome://navigator/content/metadata.xul",
                            "_blank",
                            "scrollbars,resizable,chrome,dialog=no",
                            this.target);
    },

    ///////////////
    // Utilities //
    ///////////////

    // Create instance of component given contractId and iid (as string).
    createInstance : function ( contractId, iidName ) {
        var iid = Components.interfaces[ iidName ];
        return Components.classes[ contractId ].createInstance( iid );
    },
    // Get service given contractId and iid (as string).
    getService : function ( contractId, iidName ) {
        var iid = Components.interfaces[ iidName ];
        return Components.classes[ contractId ].getService( iid );
    },
    // Show/hide one item (specified via name or the item element itself).
    showItem : function ( itemOrId, show ) {
        var item = itemOrId.constructor == String ? document.getElementById(itemOrId) : itemOrId;
        if (item) 
          item.hidden = !show;
    },
    // Set given attribute of specified context-menu item.  If the
    // value is null, then it removes the attribute (which works
    // nicely for the disabled attribute).
    setItemAttr : function ( id, attr, val ) {
        var elem = document.getElementById( id );
        if ( elem ) {
            if ( val == null ) {
                // null indicates attr should be removed.
                elem.removeAttribute( attr );
            } else {
                // Set attr=val.
                elem.setAttribute( attr, val );
            }
        }
    },
    // Set context menu attribute according to like attribute of another node
    // (such as a broadcaster).
    setItemAttrFromNode : function ( item_id, attr, other_id ) {
        var elem = document.getElementById( other_id );
        if ( elem && elem.getAttribute( attr ) == "true" ) {
            this.setItemAttr( item_id, attr, "true" );
        } else {
            this.setItemAttr( item_id, attr, null );
        }
    },
    // Temporary workaround for DOM api not yet implemented by XUL nodes.
    cloneNode : function ( item ) {
        // Create another element like the one we're cloning.
        var node = document.createElement( item.tagName );

        // Copy attributes from argument item to the new one.
        var attrs = item.attributes;
        for ( var i = 0; i < attrs.length; i++ ) {
            var attr = attrs.item( i );
            node.setAttribute( attr.nodeName, attr.nodeValue );
        }

        // Voila!
        return node;
    },
    // Generate fully-qualified URL for clicked-on link.
    linkURL : function () {
        if (this.link.href) {
          return this.link.href;
        }
        var href = this.link.getAttributeNS("http://www.w3.org/1999/xlink","href");
        if (!href || !href.match(/\S/)) {
          throw "Empty href"; // Without this we try to save as the current doc, for example, HTML case also throws if empty
        }
        href = this.makeURLAbsolute(this.link.baseURI,href);
        return href;
    },
    // Get text of link.
    linkText : function () {
        var text = gatherTextUnder( this.link );
        if (!text || !text.match(/\S/)) {
          text = this.link.getAttribute("title");
          if (!text || !text.match(/\S/)) {
            text = this.link.getAttribute("alt");
            if (!text || !text.match(/\S/)) {
              if (this.link.href) {                
                text = this.link.href;
              } else {
                text = getAttributeNS("http://www.w3.org/1999/xlink", "href");
                if (text && text.match(/\S/)) {
                  text = this.makeURLAbsolute(this.link.baseURI, text);
                }
              }
            }
          }
        }

        return text;
    },

    //Get selected object and convert it to a string to get
    //selected text.   Only use the first 15 chars.
    isTextSelection : function() {
        var result = false;
        var selection = this.searchSelected(16);

        var bundle = srGetStrBundle("chrome://communicator/locale/contentAreaCommands.properties");

        var searchSelectText;
        if (selection != "") {
            searchSelectText = selection.toString();
            if (searchSelectText.length > 15)
                searchSelectText = searchSelectText.substr(0,15) + "...";
            result = true;

          // format "Search for <selection>" string to show in menu
          searchSelectText = bundle.formatStringFromName("searchText",
                                                         [searchSelectText], 1);
          this.setItemAttr("context-searchselect", "label", searchSelectText);
        } 
        return result;
    },
    
    searchSelected : function( charlen ) {
        var focusedWindow = document.commandDispatcher.focusedWindow;
        var searchStr = focusedWindow.__proto__.getSelection.call(focusedWindow);
        searchStr = searchStr.toString();
        // searching for more than 150 chars makes no sense
        if (!charlen)
            charlen = 150;
        if (charlen < searchStr.length) {
            // only use the first charlen important chars. see bug 221361
            var pattern = new RegExp("^(?:\\s*.){0," + charlen + "}");
            pattern.test(searchStr);
            searchStr = RegExp.lastMatch;
        }
        searchStr = searchStr.replace(/\s*(.*?)\s*$/, "$1");
        searchStr = searchStr.replace(/\s+/g, " ");
        return searchStr;
    },
    
    // Convert relative URL to absolute, using document's <base>.
    makeURLAbsolute : function ( base, url ) {
        // Construct nsIURL.
        var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
        var baseURI  = ioService.newURI(base, null, null);
        
        return ioService.newURI(baseURI.resolve(url), null, null).spec;
    },
    toString : function () {
        return "contextMenu.target     = " + this.target + "\n" +
               "contextMenu.onImage    = " + this.onImage + "\n" +
               "contextMenu.onLink     = " + this.onLink + "\n" +
               "contextMenu.link       = " + this.link + "\n" +
               "contextMenu.inFrame    = " + this.inFrame + "\n" +
               "contextMenu.hasBGImage = " + this.hasBGImage + "\n";
    },
    isTargetATextBox : function ( node )
    {
      if (node.nodeType != Node.ELEMENT_NODE)
        return false;

      if (node.localName.toUpperCase() == "INPUT") {
        var attrib = "";
        var type = node.getAttribute("type");

        if (type)
          attrib = type.toUpperCase();

        return( (attrib != "IMAGE") &&
                (attrib != "CHECKBOX") &&
                (attrib != "RADIO") &&
                (attrib != "SUBMIT") &&
                (attrib != "RESET") &&
                (attrib != "HIDDEN") &&
                (attrib != "RESET") &&
                (attrib != "BUTTON") );
      } else  {
        return(node.localName.toUpperCase() == "TEXTAREA");
      }
    },
    
    // Determines whether or not the separator with the specified ID should be 
    // shown or not by determining if there are any non-hidden items between it
    // and the previous separator. 
    shouldShowSeparator : function ( aSeparatorID )
    {
      var separator = document.getElementById(aSeparatorID);
      if (separator) {
        var sibling = separator.previousSibling;
        while (sibling && sibling.localName != "menuseparator") {
          if (sibling.getAttribute("hidden") != "true")
            return true;
          sibling = sibling.previousSibling;
        }
      }
      return false;  
    }
};

/*************************************************************************
 *
 *   nsDefaultEngine : nsIObserver
 *
 *************************************************************************/
function nsDefaultEngine()
{
    try
    {
        var pb = Components.classes["@mozilla.org/preferences-service;1"].
                   getService(Components.interfaces.nsIPrefBranch);
        var pbi = pb.QueryInterface(
                    Components.interfaces.nsIPrefBranchInternal);
        pbi.addObserver(this.domain, this, false);

        // reuse code by explicitly invoking initial |observe| call
        // to initialize the |icon| and |name| member variables
        this.observe(pb, "", this.domain);
    }
    catch (ex)
    {
    }
}

nsDefaultEngine.prototype = 
{
    name: "",
    icon: "",
    domain: "browser.search.defaultengine",

    // nsIObserver implementation
    observe: function(aPrefBranch, aTopic, aPrefName)
    {
        try
        {
            var rdf = Components.
                        classes["@mozilla.org/rdf/rdf-service;1"].
                        getService(Components.interfaces.nsIRDFService);
            var ds = rdf.GetDataSource("rdf:internetsearch");
            var defaultEngine = aPrefBranch.getCharPref(aPrefName);
            var res = rdf.GetResource(defaultEngine);

            // get engine ``pretty'' name
            const kNC_Name = rdf.GetResource(
                               "http://home.netscape.com/NC-rdf#Name");
            var engineName = ds.GetTarget(res, kNC_Name, true);
            if (engineName)
            {
                this.name = engineName.QueryInterface(
                              Components.interfaces.nsIRDFLiteral).Value;
            }

            // get URL to engine vendor icon
            const kNC_Icon = rdf.GetResource(
                               "http://home.netscape.com/NC-rdf#Icon");
            var iconURL = ds.GetTarget(res, kNC_Icon, true);
            if (iconURL)
            {
                this.icon = iconURL.QueryInterface(
                  Components.interfaces.nsIRDFLiteral).Value;
            }
        }
        catch (ex)
        {
        }
    }
}
