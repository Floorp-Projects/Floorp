/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is this file as it was released on January 3, 2001.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com> (Original Author)
 *   Gervase Markham <gerv@gerv.net>
 *   Heikki Toivonen <heikki@netscape.com>
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

const XLinkNS = "http://www.w3.org/1999/xlink";
const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const XMLNS = "http://www.w3.org/XML/1998/namespace";
const XHTMLNS = "http://www.w3.org/1999/xhtml";
var gMetadataBundle;
var gLangBundle;
var gRegionBundle;
var nodeView;
var htmlMode = false;

var onLink   = false;
var onImage  = false;
var onInsDel = false;
var onQuote  = false;
var onMisc   = false;
var onTable  = false;
var onTitle  = false;
var onLang   = false;

const nsICacheService = Components.interfaces.nsICacheService;
const cacheService = Components.classes["@mozilla.org/network/cache-service;1"]
                     .getService(nsICacheService);
var httpCacheSession = cacheService.createSession("HTTP", 0, true);
httpCacheSession.doomEntriesIfExpired = false;
var ftpCacheSession = cacheService.createSession("FTP", 0, true);
ftpCacheSession.doomEntriesIfExpired = false;


function onLoad()
{
    gMetadataBundle = document.getElementById("bundle_metadata");
    gLangBundle = document.getElementById("bundle_languages");
    gRegionBundle = document.getElementById("bundle_regions");
    
    showMetadataFor(window.arguments[0]);

    nodeView = window.arguments[0].ownerDocument.defaultView;
}

function showMetadataFor(elem)
{
    // skip past non-element nodes
    while (elem && elem.nodeType != Node.ELEMENT_NODE)
        elem = elem.parentNode;

    if (!elem) {
        alert(gMetadataBundle.getString("unableToShowProps"));
        window.close();
    }

    if (elem.ownerDocument.getElementsByName && !elem.ownerDocument.namespaceURI)
        htmlMode = true;
    
    // htmllocalname is "" if it's not an html tag, or the name of the tag if it is.
    var htmllocalname = "";
    if (isHTMLElement(elem,"")) { 
        htmllocalname = elem.localName.toLowerCase();
    }
    
    // We only look for images once
    checkForImage(elem, htmllocalname);
    
    // Walk up the tree, looking for elements of interest.
    // Each of them could be at a different level in the tree, so they each
    // need their own boolean to tell us to stop looking.
    while (elem && elem.nodeType == Node.ELEMENT_NODE) {
        if (!onLink)   checkForLink(elem, htmllocalname);
        if (!onInsDel) checkForInsDel(elem, htmllocalname);
        if (!onQuote)  checkForQuote(elem, htmllocalname);
        if (!onTable)  checkForTable(elem, htmllocalname);
        if (!onTitle)  checkForTitle(elem, htmllocalname);
        if (!onLang)   checkForLang(elem, htmllocalname);
          
        elem = elem.parentNode;

        htmllocalname = "";
        if (isHTMLElement(elem,"")) { 
            htmllocalname = elem.localName.toLowerCase();
        }
    }
    
    // Decide which sections to show
    var onMisc = onTable || onTitle || onLang;
    if (!onMisc)   hideNode("misc-sec");
    if (!onLink)   hideNode("link-sec");
    if (!onImage)  hideNode("image-sec");
    if (!onInsDel) hideNode("insdel-sec");
    if (!onQuote)  hideNode("quote-sec");

    // Fix the Misc section visibilities
    if (onMisc) {
        if (!onTable) hideNode("misc-tblsummary");
        if (!onLang)  hideNode("misc-lang");
        if (!onTitle) hideNode("misc-title");
    }

    // Get rid of the "No properties" message. This is a backstop -
    // it should really never show, as long as nsContextMenu.js's
    // checking doesn't get broken.
    if (onLink || onImage || onInsDel || onQuote || onMisc)
        hideNode("no-properties")
}


function checkForImage(elem, htmllocalname)
{
    var img;
    var imgType;   // "img" = <img>
                   // "object" = <object>
                   // "input" = <input type=image>
                   // "background" = css background (to be added later)
    var ismap = false;

    if (htmllocalname === "img") {
        img = elem;
        imgType = "img";

    } else if (htmllocalname === "object" &&
               elem.type.substring(0,6) == "image/" &&
               elem.data) {
        img = elem;
        imgType = "object";

    } else if (htmllocalname === "input" &&
               elem.type.toUpperCase() == "IMAGE") {
        img = elem;
        imgType = "input";

    } else if (htmllocalname === "area" || htmllocalname === "a") {

        // Clicked in image map?
        var map = elem;
        ismap = true;
        setAlt(map);

        while (map && map.nodeType == Node.ELEMENT_NODE && !isHTMLElement(map,"map") )
            map = map.parentNode;

        if (map && map.nodeType == Node.ELEMENT_NODE) {
            img = getImageForMap(map);
            var imgLocalName = img && img.localName.toLowerCase();
            if (imgLocalName == "img" || imgLocalName == "object") {
                imgType = imgLocalName;
            }
        }

    }

    if (img) {

        var imgURL = imgType == "object" ? img.data : img.src;
        setInfo("image-url", imgURL);
        var size = getSize(imgURL);

        if (size != -1) {
            var kbSize = size / 1024;
            kbSize = Math.round(kbSize*100)/100;
            setInfo("image-filesize", gMetadataBundle.getFormattedString("imageSize", [kbSize, size]));
        } else {
            setInfo("image-filesize", gMetadataBundle.getString("imageSizeUnknown"));
        }
        if ("width" in img && img.width != "") {
            setInfo("image-width", gMetadataBundle.getFormattedString("imageWidth", [ img.width ]));
            setInfo("image-height", gMetadataBundle.getFormattedString("imageHeight", [ img.height ]));
        }
        else {
            setInfo("image-width", "");
            setInfo("image-height", "");
        }        
         
        if (imgType == "img") {
            setInfo("image-desc", getAbsoluteURL(img.longDesc, img));
        } else {
            setInfo("image-desc", "");
        }
        
        onImage = true;
    }

    if (!ismap) {
       if (imgType == "img" || imgType == "input") {
           setAlt(img);
       } else {
           hideNode("image-alt");
       }
    }
}

function checkForLink(elem, htmllocalname)
{
    if ((htmllocalname === "a" && elem.href != "") ||
        htmllocalname === "area") {

        setInfo("link-lang", convertLanguageCode(elem.getAttribute("hreflang")));
        setInfo("link-url",  elem.href);
        setInfo("link-type", elem.getAttribute("type"));
        setInfo("link-rel",  elem.getAttribute("rel"));
        setInfo("link-rev",  elem.getAttribute("rev"));

        var target = elem.target;

        switch (target) {
        case "_top":
            setInfo("link-target", gMetadataBundle.getString("sameWindowText"));
            break;
        case "_parent":
            setInfo("link-target", gMetadataBundle.getString("parentFrameText"));
            break;
        case "_blank":
            setInfo("link-target", gMetadataBundle.getString("newWindowText"));
            break;
        case "":
        case "_self":
            if (elem.ownerDocument != elem.ownerDocument.defaultView.content.document)
                setInfo("link-target", gMetadataBundle.getString("sameFrameText"));
            else
                setInfo("link-target", gMetadataBundle.getString("sameWindowText"));
            break;
        default:
            setInfo("link-target", "\"" + target + "\"");
        }
        
        onLink = true;
    }

    else if (elem.getAttributeNS(XLinkNS,"href") != "") {
        setInfo("link-url", getAbsoluteURL(elem.getAttributeNS(XLinkNS,"href"),elem));
        setInfo("link-lang", "");
        setInfo("link-type", "");
        setInfo("link-rel", "");
        setInfo("link-rev", "");

        switch (elem.getAttributeNS(XLinkNS,"show")) {
        case "embed":
            setInfo("link-target", gMetadataBundle.getString("embeddedText"));
            break;
        case "new":
            setInfo("link-target", gMetadataBundle.getString("newWindowText"));
            break;
        case "":
        case "replace":
            if (elem.ownerDocument != elem.ownerDocument.defaultView.content.document)
                setInfo("link-target", gMetadataBundle.getString("sameFrameText"));
            else
                setInfo("link-target", gMetadataBundle.getString("sameWindowText"));
            break;
        default:
            setInfo("link-target", "");
            break;
        }
        
        onLink = true;
    }
}

function checkForInsDel(elem, htmllocalname)
{
    if ((htmllocalname === "ins" || htmllocalname === "del") &&
        (elem.cite || elem.dateTime)) {
        setInfo("insdel-cite", getAbsoluteURL(elem.cite, elem));
        setInfo("insdel-date", elem.dateTime);
        onInsDel = true;
    } 
}


function checkForQuote(elem, htmllocalname)
{
    if ((htmllocalname === "q" || htmllocalname === "blockquote") && elem.cite) {
        setInfo("quote-cite", getAbsoluteURL(elem.cite, elem));
        onQuote = true;
    } 
}

function checkForTable(elem, htmllocalname)
{
    if (htmllocalname === "table" && elem.summary) {
        setInfo("misc-tblsummary", elem.summary);
        onTable = true;
    }
}

function checkForLang(elem, htmllocalname)
{
    if ((htmllocalname && elem.lang) || elem.getAttributeNS(XMLNS, "lang")) {
        var abbr;
        if (htmllocalname && elem.lang)
            abbr = elem.lang;
        else
            abbr = elem.getAttributeNS(XMLNS, "lang");
            
        setInfo("misc-lang", convertLanguageCode(abbr));
        onLang = true;
    }
}
    
function checkForTitle(elem, htmllocalname)
{
    if (htmllocalname && elem.title) {
        setInfo("misc-title", elem.title);
        onTitle = true;
    }    
}

/*
 * Set text of node id to value
 * if value="" the node with specified id is hidden.
 * Node should be have one of these forms
 * <xul:label id="id-text" value=""/>
 * <xul:description id="id-text"/>
 */
function setInfo(id, value)
{
    if (!value) {
        hideNode(id);
        return;
    }

    var node = document.getElementById(id+"-text");

    if (node.namespaceURI == XULNS && node.localName == "label" ||
       (node.namespaceURI == XULNS && node.localName == "textbox")) {
        node.setAttribute("value",value);

    } else if (node.namespaceURI == XULNS && node.localName == "description") {
        while (node.hasChildNodes())
            node.removeChild(node.firstChild);
        node.appendChild(node.ownerDocument.createTextNode(value));
    }
}

// Hide node with specified id
function hideNode(id)
{
    var style = document.getElementById(id).getAttribute("style");
    document.getElementById(id).setAttribute("style", "display:none;" + style);
}

const nsIScriptSecurityManager = Components.interfaces.nsIScriptSecurityManager;

// opens the link contained in the node's "value" attribute.
function openLink(node)
{
    var url = node.getAttribute("value");
    // Security-Critical: Only links to 'safe' protocols should be functional.
    // Specifically, javascript: and data: URLs must be made non-functional
    // here, because they will run with full privilege.
    var safeurls = /^https?:|^file:|^chrome:|^resource:|^mailbox:|^imap:|^s?news:|^nntp:|^about:|^mailto:|^ftp:|^gopher:/i;
    if (safeurls.test(url)) {
        var secMan = Components.classes["@mozilla.org/scriptsecuritymanager;1"].getService().
                         QueryInterface(nsIScriptSecurityManager);
        try {
            secMan.checkLoadURIStr(nodeView.content.document.location,
                                   url, nsIScriptSecurityManager.STANDARD);
        } catch (e) {
            return;
        }
        nodeView.content.document.location = url;
        window.close();
    }
}

/*
 * Find <img> or <object> which uses an imagemap.
 * If more then one object is found we can't determine which one
 * was clicked.
 *
 * This code has to be changed once bug 1882 is fixed.
 * Once bug 72527 is fixed this code should use the .images collection.
 */
function getImageForMap(map)
{
    var mapuri = "#" + map.getAttribute("name");
    var multipleFound = false;
    var img;

    var list = getHTMLElements(map.ownerDocument, "img");
    for (var i=0; i < list.length; i++) {
        if (list.item(i).getAttribute("usemap") == mapuri) {
            if (img) {
                multipleFound = true;
                break;
            } else {
                img = list.item(i);
                imgType = "img";
            }
        }
    }

    list = getHTMLElements(map.ownerDocument, "object");
    for (i = 0; i < list.length; i++) {
        if (list.item(i).getAttribute("usemap") == mapuri) {
            if (img) {
              multipleFound = true;
              break;
            } else {
              img = list.item(i);
              imgType = "object";
            }
        }
    }

    if (multipleFound)
        img = null;

    return img;
}

/*
 * Takes care of XMLBase and <base>
 * url is the possibly relative url.
 * node is the node where the url was given (needed for XMLBase)
 *
 * This function is called in many places as a workaround for bug 72524
 * Once bug 72522 is fixed this code should use the Node.baseURI attribute
 *
 * for node==null or url=="", empty string is returned
 */
function getAbsoluteURL(url, node)
{
    if (!url || !node)
        return "";

    var urlArr = new Array(url);
    var doc = node.ownerDocument;

    if (node.nodeType == Node.ATTRIBUTE_NODE)
        node = node.ownerElement;

    while (node && node.nodeType == Node.ELEMENT_NODE) {
        if (node.getAttributeNS(XMLNS, "base") != "")
            urlArr.unshift(node.getAttributeNS(XMLNS, "base"));

        node = node.parentNode;
    }

    // Look for a <base>.
    var baseTags = getHTMLElements(doc,"base");
    if (baseTags && baseTags.length) {
        urlArr.unshift(baseTags[baseTags.length - 1].getAttribute("href"));
    }

    // resolve everything from bottom up, starting with document location
    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                  .getService(Components.interfaces.nsIIOService);
    var URL = ioService.newURI(doc.location.href, null, null);
    for (var i=0; i<urlArr.length; i++) {
        URL.spec = URL.resolve(urlArr[i]);
    }

    return URL.spec;
}

function getHTMLElements(node, name)
{
    if (htmlMode)
        return node.getElementsByTagName(name);
    return node.getElementsByTagNameNS(XHTMLNS, name);
}

// name should be in lower case
function isHTMLElement(node, name)
{
    if (node.nodeType != Node.ELEMENT_NODE)
        return false;

    if (htmlMode)
        return !name || node.localName.toLowerCase() == name;

    return (!name || node.localName == name) && node.namespaceURI == XHTMLNS;
}

// This function coded according to the spec at:
// http://www.bath.ac.uk/~py8ieh/internet/discussion/metadata.txt
function convertLanguageCode(abbr)
{
    if (!abbr) return "";
    var result;
    var region = "";
    var tokens = abbr.split("-");
    var language = tokens.shift();

    if (language == "x" || language == "i")
    {
        // x and i prefixes mean unofficial ones. So we proper-case the next
        // word and leave the rest.
        if (tokens.length > 0)
        {
            // Upper-case first letter
            language = tokens[0].substr(0, 1).toUpperCase() + tokens[0].substr(1);
            tokens.shift();

            // Add on the rest as space-separated strings inside the brackets
            region = tokens.join(" ");
        }
    }
    else
    {
        // Otherwise we treat the first as a lang, the second as a region
        // and the rest as strings.
        try
        {
            language = gLangBundle.getString(language);
        }
        catch (e) 
        {
        }

        if (tokens.length > 0)
        {
            try
            {
                tokens[0] = gRegionBundle.getString(tokens[0].toLowerCase());
            }
            catch (e) 
            {
            }
            region = tokens.join(" ");
        }
    }

    if (region) {
        result = gMetadataBundle.getFormattedString("languageRegionFormat",
                                                    [language, region]);
    } else {
        result = language;
    }
    return result;
}

// Returns the size of the URL in bytes; must be cached and therefore an HTTP or FTP URL
function getSize(url) {
    try
    {
        var cacheEntryDescriptor = httpCacheSession.openCacheEntry(url, Components.interfaces.nsICache.ACCESS_READ, false);
        if(cacheEntryDescriptor)
          return cacheEntryDescriptor.dataSize;
    }
    catch(ex) {}
    try
    {
        cacheEntryDescriptor = ftpCacheSession.openCacheEntry(url, Components.interfaces.nsICache.ACCESS_READ, false);
        if (cacheEntryDescriptor)
            return cacheEntryDescriptor.dataSize;
    }
    catch(ex) {}
    return -1;
}

function setAlt(elem) {
    var altText = document.getElementById("image-alt-text");
    if (elem.hasAttribute("alt")) {
        if (elem.alt != "") {
            altText.value = elem.alt;
            altText.setAttribute("style","font-style:inherit");
        } else {
            altText.value = gMetadataBundle.getString("altTextBlank");
            altText.setAttribute("style","font-style:italic");
        }
    } else {
        altText.value = gMetadataBundle.getString("altTextMissing");
        altText.setAttribute("style","font-style:italic");
    }

}
