/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     William A. ("PowerGUI") Law <law@netscape.com>
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
    this.target     = null;
    this.menu       = null;
    this.onImage    = false;
    this.onLink     = false;
    this.link       = false;
    this.inFrame    = false;
    this.hasBGImage = false;

    // Initialize new menu.
    this.initMenu( xulMenu );
}

// Prototype for nsContextMenu "class."
nsContextMenu.prototype = {
    // Remove all the children which we added at oncreate.
    onDestroy : function () {
        this.removeAllItems();
    },
    // Initialize context menu.
    initMenu : function ( popup, event ) {
        // Save menu.
        this.menu = popup;

        // Get contextual info.
        this.setTarget( document.popupNode );
    
        // Populate menu from template.
        var template = document.getElementById( "context-template" );
        var items = template.childNodes;
        for ( var i = 0; i < items.length; i++ ) {
            // Replicate item.
            //var item = items.item(i).cloneNode( false );
    
            // cloneNode not implemented, fake it.
            var item = this.cloneNode( items.item(i) );
    
            // Change id.
            item.setAttribute( "id", item.getAttribute( "id" ).replace( "template-", "context-" ) );
    
            // Add it to popup menu.
            this.menu.appendChild( item );
        }

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
    },
    initOpenItems : function () {
        var needSep = false;
    
        // Remove open/edit link if not applicable.
        if ( !this.onLink ) {
            this.removeItem( "context-openlink" );
            this.removeItem( "context-editlink" );
        } else {
            needSep = true;
        }
    
        // Remove open frame if not applicable.
        if ( !this.inFrame ) {
            this.removeItem( "context-openframe" );
        } else {
            needSep = true;
        }
    
        if ( !needSep ) {
            // Remove separator after open items.
            this.removeItem( "context-sep-open" );
        }
    },
    initNavigationItems : function () {
        // Back determined by canGoBack broadcaster.
        this.setItemAttrFromNode( "context-back", "disabled", "canGoBack" );
    
        // Forward determined by canGoForward broadcaster.
        this.setItemAttrFromNode( "context-forward", "disabled", "canGoForward" );
    
        // Reload is always OK.
    
        // Stop determined by canStop broadcaster.
        this.setItemAttrFromNode( "context-stop", "disabled", "canStop" );
    },
    initSaveItems : function () {
        // Save page is always OK.
    
        // Save frame as depends on whether we're in a frame.
        if ( !this.inFrame ) {
            this.removeItem( "context-saveframe" );
        }
    
        // Save link depends on whether we're in a link.
        if ( !this.onLink ) {
            this.removeItem( "context-savelink" );
        }
    
        // Save background image depends on whether there is one.
        if ( !this.hasBGImage ) {
            this.removeItem( "context-savebgimage" );
        }
    
        // Save image depends on whether there is one.
        if ( !this.onImage ) {
            this.removeItem( "context-saveimage" );
        }
    },
    initViewItems : function () {
        // View source is always OK.
    
        // View frame source depends on whether we're in a frame.
        if ( !this.inFrame ) {
            this.removeItem( "context-viewframesource" );
        }
    
        // View Info don't work no way no how.
        this.removeItem( "context-viewinfo" );
    
        // View Frame Info isn't working, either.
        this.removeItem( "context-viewframeinfo" );
    
        // View Image depends on whether an image was clicked on.
        if ( !this.onImage ) {
            this.removeItem( "context-viewimage" );
        }
    },
    initMiscItems : function () {
        // Add bookmark always OK.
    
        // Send Page not working yet.
        this.removeItem( "context-sendpage" );
    },
    initClipboardItems : function () {
        // Select All is always OK.
    
        // Copy depends on whether there is selected text.
        this.setItemAttr( "context-copy", "disabled", this.isNoTextSelected() );
    
        // Copy link location depends on whether we're on a link.
        if ( !this.onLink ) {
            this.removeItem( "context-copylink" );
        }
    
        // Copy image location depends on whether we're on an image.
        if ( !this.onImage ) {
            this.removeItem( "context-copyimage" );
        }
    },
    // Set various context menu attributes based on the state of the world.
    setTarget : function ( node ) {
        // Initialize contextual info.
        this.onImage    = false;
        this.onLink     = false;
        this.inFrame    = false;
        this.hasBGImage = false;
    
        // Remember the node that was clicked.
        this.target = node;
    
        // See if the user clicked on an image.
        if ( this.target.nodeType == 1 ) {
             if ( this.target.tagName.toUpperCase() == "IMG" ) {
                this.onImage = true;
                // Look for image map.
                var mapName = this.target.getAttribute( "usemap" );
                if ( mapName ) {
                    // Find map.
                    var map = this.target.ownerDocument.getElementById( mapName.substr(1) );
                    if ( map ) {
                        // Search child <area>s for a match.
                        var areas = map.childNodes;
                        //XXX Client side image maps are too hard for now!
                        dump( "Client side image maps not supported yet, sorry!\n" );
                        areas.length = 0;
                        for ( var i = 0; i < areas.length && !this.onLink; i++ ) {
                            var area = areas[i];
                            if ( area.nodeType == 1
                                 &&
                                 area.tagName.toUpperCase() == "AREA" ) {
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
                                        break;
                                }
                            }
                        }
                    }
                }   
            }
        }
    
        // See if the user clicked in a frame.
        if ( this.target.ownerDocument != window.content.document ) {
            this.inFrame = true;
        }
    
        // Bubble out, looking for link.
        var elem = this.target;
        while ( elem && !this.onLink ) {
            // Test for element types of interest.
            if ( elem.nodeType == 1 && 
                 ( elem.tagName.toUpperCase() == "A"
                   ||
                   elem.tagName.toUpperCase() == "AREA" ) ) {
                // Clicked on a link.
                this.onLink = true;
                // Remember corresponding element.
                this.link = elem;
            }
            elem = elem.parentNode;
        }
    },
    // Open linked-to URL in a new window.
    openLink : function () {
        // Determine linked-to URL.
        openNewWindowWith( this.linkURL() );
    },
    // Edit linked-to URL in a new window.
    editLink : function () {
        BrowserEditPage( this.linkURL() );
    },
    // Open clicked-in frame in its own window.
    openFrame : function () {
        openNewWindowWith( this.target.ownerDocument.location.href );
    },
    // Open new "view source" window with the frame's URL.
    viewFrameSource : function () {
        window.openDialog( "chrome://navigator/content/",
                           "_blank",
                           "chrome,menubar,status,dialog=no", 
                           this.target.ownerDocument.location.href,
                           "view-source" );
    },
    viewInfo : function () {
        dump( "nsContextMenu.viewInfo not implemented yet\n" );
    },
    viewFrameInfo : function () {
        dump( "nsContextMenu.viewFrameInfo not implemented yet\n" );
    },
    // Open new window with the URL of the image.
    viewImage : function () {
        openNewWindowWith( this.target.src );
    },
    // Save URL of clicked-on frame.
    saveFrame : function () {
        this.savePage( this.target.ownerDocument.location.href );
    },
    // Save URL of clicked-on link.
    saveLink : function () {
        this.savePage( this.linkURL() );
    },
    // Save URL of clicked-on image.
    saveImage : function () {
        this.savePage( this.imageURL() );
    },
    // Save URL of background image.
    saveBGImage : function () {
        this.savePage( this.bgImageURL() );
    },
    // Generate link URL and put it on clibboard.
    copyLink : function () {
        this.copyToClipboard( this.linkURL() );
    },
    // Generate image URL and put it on the clipboard.
    copyImage : function () {
        this.copyToClipboard( this.imageURL() );
    },
    // Utilities
    // Remove one item (specified via name or the item element itself).
    removeItem : function ( itemOrId ) {
        var item = null;
        if ( itemOrId.constructor == String ) {
            // Argument specifies item id.
            item = document.getElementById( itemOrId );
        } else {
            // Argument is the item itself.
            item = itemOrId;
        }
        if ( item ) {
            // Change id so it doesn't interfere with this menu
            // the next time it is displayed.
            item.setAttribute( "id", "_dead" );
            this.menu.removeChild( item );
        }
    },
    // Remove all menu items.
    removeAllItems : function () {
        if ( this.menu ) {
            var items = this.menu.childNodes;
            for ( var i = 0; i < items.length; i++ ) {
                this.removeItem( items[i] );
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
        return this.link.href;
    },
    // Generate fully-qualified URL for clicked-on image.
    imageURL : function () {
        return this.target.src;
    },
    // Returns "true" if there's no text selected, null otherwise.
    isNoTextSelected : function ( event ) {
        // Not implemented so all text-selected-based options are disabled.
        return "true";
    },
    // Copy link/image url to clipboard.
    copyToClipboard : function ( text ) {
        // Get clipboard.
        var clipboard = getServiceById( "{8B5314BA-DB01-11d2-96CE-0060B0FB9956}",
                                        "nsIClipboard" );
        // Create tranferable that will transfer the text.
        var transferable = createInstanceById( "{8B5314BC-DB01-11d2-96CE-0060B0FB9956}",
                                               "nsITransferable" );
        transferable.addDataFlavor( "text/plain" );
        // Create wrapper for text.
        var data = createInstance( "component://netscape/supports-string",
                                   "nsISupportsString" );
        data.data = text ;
        transferable.setTransferData( "text/plain", data, text.length * 2 );
        // Put on clipboard.
        clipboard.setData( transferable, null );
    },
    // Save specified URL in user-selected file.
    savePage : function ( url ) {
        // Default is to save current page.
        if ( !url ) {
            url = window.content.location.href;
        }
        // Use stream xfer component to prompt for destination and save.
        var xfer = Components
                     .classes[ "component://netscape/appshell/component/xfer" ]
                       .getService( Components.interfaces.nsIStreamTransfer );
        try {
            // When Necko lands, we need to receive the real nsIChannel and
            // do SelectFileAndTransferLocation!

            // Use this for now...
            xfer.SelectFileAndTransferLocationSpec( url, window );
        } catch( exception ) {
            // Failed (or cancelled), give them another chance.
            dump( "SelectFileAndTransferLocationSpec failed, rv=" + exception + "\n" );
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
    }
};
