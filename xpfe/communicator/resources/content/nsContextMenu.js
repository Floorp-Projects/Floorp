/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *     William A. ("PowerGUI") Law <law@netscape.com>
 *     Blake Ross <blakeross@telocity.com>
 *     Gervase Markham <gerv@gerv.net>
 */

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
    this.onTextInput    = false;
    this.onImage        = false;
    this.onLink         = false;
    this.onMailtoLink   = false;
    this.onSaveableLink = false;
    this.link           = false;
    this.inFrame        = false;
    this.hasBGImage     = false;
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
        // Save menu.
        this.menu = popup;

        // Get contextual info.
        this.setTarget( document.popupNode );

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
        // Remove open/edit link if not applicable.
        this.showItem( "context-openlink", this.onSaveableLink || ( this.inDirList && this.onLink ) );
        this.showItem( "context-openlinkintab", this.onSaveableLink || ( this.inDirList && this.onLink ) );
        this.showItem( "context-editlink", this.onSaveableLink && !this.inDirList );

        // Remove open frame if not applicable.
        this.showItem( "context-openframe", this.inFrame );
        this.showItem( "context-showonlythisframe", this.inFrame );

        // Remove separator after open items if neither link nor frame.
        this.showItem( "context-sep-open", this.onSaveableLink || ( this.inDirList && this.onLink ) || this.inFrame );
    },
    initNavigationItems : function () {
        // Back determined by canGoBack broadcaster.
        this.setItemAttrFromNode( "context-back", "disabled", "canGoBack" );

        // Forward determined by canGoForward broadcaster.
        this.setItemAttrFromNode( "context-forward", "disabled", "canGoForward" );

        // Reload is OK if not on a frame; vice-versa for reload-frame.
        this.showItem( "context-reload", !this.inFrame );
        this.showItem( "context-reload-frame", this.inFrame );

        // XXX: Stop is determined in navigator.js; the canStop broadcaster is broken
        //this.setItemAttrFromNode( "context-stop", "disabled", "canStop" );
    },
    initSaveItems : function () {
        // Save page is always OK, unless in directory listing.
        this.showItem( "context-savepage", !this.inDirList );

        // Save frame as depends on whether we're in a frame.
        this.showItem( "context-saveframe", this.inFrame );

        // Save link depends on whether we're in a link.
        this.showItem( "context-savelink", this.onSaveableLink );

        // Save image depends on whether there is one.
        this.showItem( "context-saveimage", this.onImage );
        if (this.onImage){ //if onImage, let's get the imagename into the context menu
           var saveImageMenuItem = document.getElementById( 'context-saveimage' );
           var imageName = extractFileNameFromUrl(this.imageURL);
           var bundle = srGetStrBundle("chrome://communicator/locale/contentAreaCommands.properties");
           var caption = bundle.formatStringFromName("saveImageAs",[imageName],1);

           saveImageMenuItem.setAttribute( "label", caption );
        }

        // Remove separator if none of these were shown.
        var showSep = !this.inDirList || this.inFrame || this.onSaveableLink || this.hasBGImage || this.onImage;
        this.showItem( "context-sep-save", showSep );
    },
    initViewItems : function () {
        // View source is always OK, unless in directory listing.
        this.showItem( "context-viewsource", !( this.inDirList || this.onImage ) );

        // View frame source depends on whether we're in a frame.
        this.showItem( "context-viewframesource", this.inFrame );

        // View Info is available, unless in directory listing
        this.showItem( "context-viewinfo", !this.inDirList );

        // View Frame Info depends on whether we're in a frame
        this.showItem( "context-viewframeinfo", this.inFrame );

        // View Image depends on whether an image was clicked on.
        this.showItem( "context-viewimage", this.onImage );

        // View background image depends on whether there is one.
        this.showItem( "context-viewbgimage", this.hasBGImage && !this.onImage );

        // Remove separator if all items are removed.
        this.showItem( "context-sep-view", !this.inDirList || this.inFrame || this.onImage );
    },
    initMiscItems : function () {
        // Use "Bookmark This Link" if on a link.
        this.showItem( "context-bookmarkpage", !this.onLink );
        this.showItem( "context-bookmarklink", this.onLink );
        this.showItem( "context-searchselect", this.isTextSelected() );
        // Send Page not working yet.
        this.showItem( "context-sendpage", false );
    },
    initClipboardItems : function () {
        // Select All is always OK, unless in directory listing.
        this.showItem( "context-selectall", !this.inDirList );

        // Copy depends on whether there is selected text.
        // Enabling this context menu item is now done through the global
        // command updating system
        // this.setItemAttr( "context-copy", "disabled", this.isNoTextSelected() );

        goUpdateGlobalEditMenuItems();

        // Items for text areas
        this.showItem( "context-cut", this.onTextInput );
        this.showItem( "context-paste", this.onTextInput );

        // XXX dr
        // ------
        // nsDocumentViewer.cpp has code to determine whether we're
        // on a link or an image. we really ought to be using that...

        // Copy email link depends on whether we're on an email link.
        this.showItem( "context-copyemail", this.onMailtoLink );

        // Copy link location depends on whether we're on a link.
        this.showItem( "context-copylink", this.onLink );

        // Copy image location depends on whether we're on an image.
        this.showItem( "context-copyimage", this.onImage );
    },
    initMetadataItems : function () {
        // Show if user clicked on something which has metadata.
        this.showItem( "context-metadata", this.onMetaDataItem );
        this.showItem( "context-sep-clip", this.onMetaDataItem );
    },
    // Set various context menu attributes based on the state of the world.
    setTarget : function ( node ) {
        // Initialize contextual info.
        this.onImage    = false;
        this.onMetaDataItem = false;
        this.onTextInput = false;
        this.imageURL   = "";
        this.onLink     = false;
        this.inFrame    = false;
        this.hasBGImage = false;
        this.bgImageURL = "";

        // Remember the node that was clicked.
        this.target = node;

        // See if the user clicked on an image.
        if ( this.target.nodeType == Node.ELEMENT_NODE ) {
             if ( this.target.localName.toUpperCase() == "IMG" ) {
                this.onImage = true;
                this.imageURL = this.target.src;
                // Look for image map.
                var mapName = this.target.getAttribute( "usemap" );
                if ( mapName ) {
                    // Find map.
                    var map = this.target.ownerDocument.getElementById( mapName.substr(1) );
                    if ( map ) {
                        // Search child <area>s for a match.
                        var areas = map.childNodes;
                        //XXX Client side image maps are too hard for now!
                        areas.length = 0;
                        for ( var i = 0; i < areas.length && !this.onLink; i++ ) {
                            var area = areas[i];
                            if ( area.nodeType == Node.ELEMENT_NODE
                                 &&
                                 area.localName.toUpperCase() == "AREA" ) {
                                // Get type (rect/circle/polygon/default).
                                var type = area.getAttribute( "type" );
                                var coords = this.parseCoords( area );
                                switch ( type.toUpperCase() ) {
                                    case "RECT":
                                    case "RECTANGLE":
                                        break;
                                    case "CIRC":
                                    case "CIRCLE":
                                        break;
                                    case "POLY":
                                    case "POLYGON":
                                        break;
                                    case "DEFAULT":
                                        // Default matches entire image.
                                        this.onLink = true;
                                        this.link = area;
                                        this.onSaveableLink = this.isLinkSaveable( this.link );
                                        break;
                                }
                            }
                        }
                    }
                }
             } else if ( this.target.localName.toUpperCase() == "OBJECT"
                         &&
                         // See if object tag is for an image.
                         this.objectIsImage( this.target ) ) {
                // This is an image.
                this.onImage = true;
                // URL must be constructed.
                this.imageURL = this.objectImageURL( this.target );
             } else if ( this.target.localName.toUpperCase() == "INPUT") {
               if(this.target.getAttribute( "type" ).toUpperCase() == "IMAGE") {
                 this.onImage = true;
                 // Convert src attribute to absolute URL.
                 this.imageURL = this.makeURLAbsolute( this.target.baseURI,
                                                       this.target.src );
               } else /* if (this.target.getAttribute( "type" ).toUpperCase() == "TEXT") */ {
                 this.onTextInput = this.isTargetATextBox(this.target);
               }
            } else if ( this.target.localName.toUpperCase() == "TEXTAREA" ) {
                 this.onTextInput = true;
            } else if ( this.target.getAttribute( "background" ) ) {
                 this.hasBGImage = true;
                 // Convert background attribute to absolute URL.
                 this.bgImageURL = this.makeURLAbsolute( this.target.baseURI,
                                                         this.target.getAttribute( "background" ) );
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
                        this.link = { href : root.getAttribute("URL") };
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
            } else if ( this.target.parentNode && 
                        'tagName' in this.target.parentNode &&
                        ( this.target.parentNode.tagName == "scrollbar" ||
                        this.target.parentNode.tagName == "thumb" ||
                        this.target.parentNode.tagName == "xul:slider") ) {
                this.shouldDisplay = false;
            } else {
                try {
                    var cssAttr = this.target.style.getPropertyValue( "list-style-image" ) ||
                                  this.target.style.getPropertyValue( "list-style" ) ||
                                  this.target.style.getPropertyValue( "background-image" ) ||
                                  this.target.style.getPropertyValue( "background" );
                    if ( cssAttr ) {
                        this.onImage = true;
                        var url = cssAttr.toLowerCase().replace(/url\("*(.+)"*\)/, "$1");
                        // Convert attribute to absolute URL.
                        this.imageURL = this.makeURLAbsolute( this.target.baseURI, url );
                    }
                } catch ( exception ) {
                }
            }
        }

        // We have meta data on images.
        this.onMetaDataItem = this.onImage;
        
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

                // Background image?
                if ( !this.hasBGImage && 'background' in elem && elem.background ) {
                    this.hasBGImage = true;
                    // Convert background attribute to absolute URL.
                    this.bgImageURL = this.makeURLAbsolute( elem.baseURI,
                                                            elem.background );
                }
            }
            elem = elem.parentNode;    
        }
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
    // Open linked-to URL in a new window.
    openLink : function () {
        // Determine linked-to URL.
        openNewWindowWith( this.linkURL() );
    },
    // Open linked-to URL in a new tab.
    openLinkInTab : function () {
        // Determine linked-to URL.
        openNewTabWith( this.linkURL() );
    },
    // Edit linked-to URL in a new window.
    editLink : function () {
        urlSecurityCheck( this.linkURL(), window.document );
        editPage( this.linkURL(), window, false );
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
        window._content.location.href = this.target.ownerDocument.location.href;
    },
    // Open new "view source" window with the frame's URL.
    viewFrameSource : function () {
      window.openDialog(  "chrome://navigator/content/viewSource.xul",
                          "_blank",
                          "scrollbars,resizable,chrome,dialog=no",
                          this.target.ownerDocument.location.href,
                          "charset=" + this.target.ownerDocument.characterSet);
    },
    viewInfo : function () {
      BrowserPageInfo(this.target.ownerDocument);
    },
    viewFrameInfo : function () {
      BrowserPageInfo(this.target.ownerDocument);
    },
    // Change current window to the URL of the image.
    viewImage : function () {
        openTopWin( this.imageURL );
    },
    // Change current window to the URL of the background image.
    viewBGImage : function () {
        openTopWin( this.bgImageURL );
    },
    // Save URL of clicked-on frame.
    saveFrame : function () {
        this.savePage( this.target.ownerDocument.location.href, true );
    },
    // Save URL of clicked-on link.
    saveLink : function () {
        this.savePage( this.linkURL(), false );
    },
    // Save URL of clicked-on image.
    saveImage : function () {
        this.savePage( this.imageURL, true );
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

        var clipboard = this.getService( "@mozilla.org/widget/clipboardhelper;1",
                                         Components.interfaces.nsIClipboardHelper );
        clipboard.copyString(addresses);
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
        var item = null;
        if ( itemOrId.constructor == String ) {
            // Argument specifies item id.
            item = document.getElementById( itemOrId );
        } else {
            // Argument is the item itself.
            item = itemOrId;
        }
        if ( item ) {
            var styleIn = item.getAttribute( "style" );
            var styleOut = styleIn;
            if ( show ) {
                // Remove style="display:none;".
                styleOut = styleOut.replace( "display:none;", "" );

            } else {
                // Set style="display:none;".
                if ( styleOut.indexOf( "display:none;" ) == -1 ) {
                    // Add style the first time we need to.
                    styleOut += "display:none;";
                }
            }
            // Only set style if it's different.
            if ( styleIn != styleOut ) {
                item.setAttribute( "style", styleOut );
            }
        }
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
        if (href == "") {
          throw "Empty href"; // Without this we try to save as the current doc, for example, HTML case also throws if empty
        }
        href = this.makeURLAbsolute(this.link.baseURI,href);
        return href;
    },
    // Get text of link (if possible).
    linkText : function () {
        var text = this.gatherTextUnder( this.link );
        return text;
    },
    // Gather all descendent text under given document node.
    gatherTextUnder : function ( root ) {
         var text = "";
         var node = root.firstChild;
         var depth = 1;
         while ( node && depth > 0 ) {
             // See if this node is text.
             if ( node.nodeName == "#text" ) {
                 // Add this text to our collection.
                 text += " " + node.data;
             } else if ( node.nodeType == Node.ELEMENT_NODE 
                         && node.localName.toUpperCase() == "IMG" ) {
                 // If it has an alt= attribute, use that.
                 var altText = node.getAttribute( "alt" );
                 if ( altText && altText != "" ) {
                     text = altText;
                     break;
                 }
             }
             // Find next node to test.
             // First, see if this node has children.
             if ( node.hasChildNodes() ) {
                 // Go to first child.
                 node = node.firstChild;
                 depth++;
             } else {
                 // No children, try next sibling.
                 if ( node.nextSibling ) {
                     node = node.nextSibling;
                 } else {
                     // Last resort is our next oldest uncle/aunt.
                     node = node.parentNode.nextSibling;
                     depth--;
                 }
             }
         }
         // Strip leading whitespace.
         text = text.replace( /^\s+/, "" );
         // Strip trailing whitespace.
         text = text.replace( /\s+$/, "" );
         // Compress remaining whitespace.
         text = text.replace( /\s+/g, " " );
         return text;
    },
    // Returns "true" if there's no text selected, null otherwise.
    isNoTextSelected : function ( event ) {
        // Not implemented so all text-selected-based options are disabled.
        return "true";
    },

    //Get selected object and convert it to a string to get
    //selected text.   Only use the first 15 chars.
    isTextSelected : function() {
       var result = false;
       if (_content.getSelection() != "") {
          var searchSelect = document.getElementById('context-searchselect');
          var searchSelectText = _content.getSelection().toString();
          var bundle = srGetStrBundle("chrome://communicator/locale/contentAreaCommands.properties");
          if (searchSelectText.length > 15)
             searchSelectText = searchSelectText.substr(0,15) + "...";
          searchSelectText = bundle.formatStringFromName("searchText",[searchSelectText],1);
          searchSelect.setAttribute("label", searchSelectText);
          result = true;
       }
       return result;
    },
    
    searchSelected : function() {
       var aSearchStr = _content.getSelection();
       aSearchStr = aSearchStr.toString();
       aSearchStr = aSearchStr.replace( /^\s+/, "" );
       aSearchStr = aSearchStr.replace(/\s+$/,"");
       return aSearchStr;
    },
    
    // Determine if target <object> is an image.
    objectIsImage : function ( objElem ) {
        var result = false;
        // Get type and data attributes.
        var type = objElem.getAttribute( "type" );
        var data = objElem.getAttribute( "data" );
        // Presume any mime type of the form "image/..." is an image.
        // There must be a data= attribute with an URL, also.
        if ( type.substring( 0, 6 ) == "image/" && data && data != "" ) {
            result = true;
        }
        return result;
    },
    // Extract image URL from <object> tag.
    objectImageURL : function ( objElem ) {
        // Extract url from data= attribute.
        var data = objElem.getAttribute( "data" );
        // Make it absolute.
        return this.makeURLAbsolute( objElem.baseURI, data );
    },
    // Convert relative URL to absolute, using document's <base>.
    makeURLAbsolute : function ( base, url ) {
        // Construct nsIURL.
        var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
        var baseURI  = ioService.newURI(base, null);
        
        return ioService.newURI(baseURI.resolve(url), null).spec;
    },
    // Save specified URL in user-selected file.
    savePage : function ( url, doNotValidate ) {
        var postData = null; // No post data, usually.
        var cacheKey = null;

        // Default is to save current page.
        if ( !url ) {
            url = window._content.location.href;

            try {
                var sessionHistory = getWebNavigation().sessionHistory;
                var entry = sessionHistory.getEntryAtIndex(sessionHistory.index, false).QueryInterface(Components.interfaces.nsISHEntry);
                postData = entry.postData;
                cacheKey = entry.cacheKey;
            } catch(e) {
            }
        }

        // Use stream xfer component to prompt for destination and save.
        var xfer = this.getService( "@mozilla.org/appshell/component/xfer;1",
                                    "nsIStreamTransfer" );
        try {
            xfer.SelectFileAndTransferLocationSpec( url, window, "", "", doNotValidate, postData, cacheKey );
        } catch( exception ) {
            // Failed (or cancelled), give them another chance.
        }
        return;
    },
    // Parse coords= attribute and return array.
    parseCoords : function ( area ) {
        return [];
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
        var attrib = node.getAttribute("type").toUpperCase();
        return( (attrib != "IMAGE") &&
                (attrib != "PASSWORD") &&
                (attrib != "CHECKBOX") &&
                (attrib != "RADIO") &&
                (attrib != "SUBMIT") &&
                (attrib != "RESET") &&
                (attrib != "FILE") &&
                (attrib != "HIDDEN") &&
                (attrib != "RESET") &&
                (attrib != "BUTTON") );
      } else  {
        return(node.localName.toUpperCase() == "TEXTAREA");
      }
    }
};
