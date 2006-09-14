/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is this file as it was released on
 * January 3, 2001.
 *
 * The Initial Developer of the Original Code is Jonas Sicking.
 * Portions created by Jonas Sicking are Copyright (C) 2000
 * Jonas Sicking.  All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable
 * instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 */

const XLinkNS = "http://www.w3.org/1999/xlink";
const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const XMLNS = "http://www.w3.org/XML/1998/namespace";
const XHTMLNS = "http://www.w3.org/1999/xhtml";
var gMetadataBundle;
var nodeView;
var htmlMode;

function onLoad()
{
    gMetadataBundle = document.getElementById("bundle_metadata");
    showMetadataFor(window.arguments[0]);

    nodeView = window.arguments[0].ownerDocument.defaultView;
}

function showMetadataFor(elem)
{
    while (elem && elem.nodeType != Node.ELEMENT_NODE)
        elem = elem.parentNode;

    if (!elem) {
        alert(gMetadataBundle.getString("unableToShowProps"));
        window.close();
    }

    if (elem.ownerDocument.getElementsByName && !elem.ownerDocument.namespaceURI)
        htmlMode = true;
    else
        htmlMode = false;

    showDocInfo(elem);
    showLinkInfo(elem);
    showImageInfo(elem);
    showInsDelInfo(elem);
    showQuoteInfo(elem);
    showMiscInfo(elem);
}


function showDocInfo(elem)
{
    setInfo("document-url", elem.ownerDocument.location.href);

    if (elem.ownerDocument.title)
        setInfo("document-title", elem.ownerDocument.title);
    else
        setInfo("document-title", "");

}


function showImageInfo(elem)
{
    var img;
    var imgType;   // "img" = <img>
                   // "object" = <object>
                   // "input" = <input type=image>
                   // "background" = css background (to be added later)

    if (isHTMLElement(elem,"img")) {
        img = elem;
        imgType = "img";

    } else if (isHTMLElement(elem,"object") &&
               elem.type.substring(0,6) == "image/" &&
               elem.data) {
        img = elem;
        imgType = "object";

    } else if (isHTMLElement(elem,"input") &&
               elem.type.toUpperCase() == "IMAGE") {
        img = elem;
        imgType = "input";

    } else if (isHTMLElement(elem,"area") || isHTMLElement(elem,"a")) {

        // Clicked in image map?
        var map = elem;
        while (map && map.nodeType == Node.ELEMENT_NODE && !isHTMLElement(map,"map") )
            map = map.parentNode;

        if (map && map.nodeType == Node.ELEMENT_NODE)
            img = getImageForMap(map);
    }

    if (img) {
        setInfo("image-url", img.src);
        if (imgType == "img") {
            setInfo("image-desc", getAbsoluteURL(img.longDesc, img));
        } else {
            setInfo("image-desc", "");
        }
    }
    else
        hideNode("sec-image");
}

function showLinkInfo(elem)
{
    var node = elem;
    var linkFound = false;

    while (node && node.nodeType == Node.ELEMENT_NODE && !linkFound) {
        if ((isHTMLElement(node,"a") && node.href != "") ||
            isHTMLElement(node,"area")) {

            linkFound = true;
            setInfo("link-url", node.href);
            setInfo("link-lang", node.getAttribute("hreflang"));
            setInfo("link-type", node.getAttribute("type"));
            setInfo("link-rel", node.getAttribute("rel"));
            setInfo("link-rev", node.getAttribute("rev"));

            target = node.target;
            // this is a workaround for bug 72521
            if (target == "") {
                var baseTags = getHTMLElements(node.ownerDocument,"base");
                for (var i=0; baseTags && i<baseTags.length; i++) {
                    if (baseTags.item(i).target != "") {
                        target = baseTags.item(i).target;
                    }
                }
            }

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
                if (node.ownerDocument != node.ownerDocument.defaultView._content.document)
                    setInfo("link-target", gMetadataBundle.getString("sameFrameText"));
                else
                    setInfo("link-target", gMetadataBundle.getString("sameWindowText"));
                break;
            default:
                setInfo("link-target", "\"" + target + "\"");
            }
        }

        else if (node.getAttributeNS(XLinkNS,"href") != "") {
            linkFound = true;
            setInfo("link-url", getAbsoluteURL(node.getAttributeNS(XLinkNS,"href"),node));
            setInfo("link-lang", "");
            setInfo("link-type", "");
            setInfo("link-rel", "");
            setInfo("link-rev", "");

            switch (node.getAttributeNS(XLinkNS,"show")) {
            case "embed":
                setInfo("link-target", gMetadataBundle.getString("embeddedText"));
                break;
            case "new":
                setInfo("link-target", gMetadataBundle.getString("newWindowText"));
                break;
            case "":
            case "replace":
                if (node.ownerDocument != node.ownerDocument.defaultView._content.document)
                    setInfo("link-target", gMetadataBundle.getString("sameFrameText"));
                else
                    setInfo("link-target", gMetadataBundle.getString("sameWindowText"));
                break;
            default:
                setInfo("link-target", "");
                break;
            }
        }
        node = node.parentNode;
    }

    if (!linkFound)
        hideNode("sec-link");

}

function showInsDelInfo(elem)
{
    var node = elem;

    while (node && node.nodeType == Node.ELEMENT_NODE &&
           !isHTMLElement(node,"ins") && !isHTMLElement(node,"del")) {
        node = node.parentNode;
    }

    if (node && node.nodeType == Node.ELEMENT_NODE &&
        (node.cite || node.dateTime)) {
        setInfo("insdel-cite", getAbsoluteURL(node.cite, node));
        setInfo("insdel-date", node.dateTime);
    } else {
        hideNode("sec-insdel");
    }
}


function showQuoteInfo(elem)
{
    var node = elem;

    while (node && node.nodeType == Node.ELEMENT_NODE &&
           !isHTMLElement(node,"q") && !isHTMLElement(node,"blockquote")) {
        node = node.parentNode;
    }

    if (node && node.nodeType == Node.ELEMENT_NODE && node.cite) {
        setInfo("quote-cite", getAbsoluteURL(node.cite, node));
    } else {
        hideNode("sec-quote");
    }
}


function showMiscInfo(elem)
{
    var miscFound = false;

    // language
    var node = elem;
    while (node && node.nodeType == Node.ELEMENT_NODE &&
           !(isHTMLElement(node,"") && node.lang) &&
           !node.getAttributeNS(XMLNS, "lang")) {
        node = node.parentNode;
    }

    if (node && node.nodeType == Node.ELEMENT_NODE) {
        miscFound = true;
        if (isHTMLElement(node,"") && node.lang)
            setInfo("misc-lang", node.lang);
        else
            setInfo("misc-lang", node.getAttributeNS(XMLNS, "lang"));
    } else {
        setInfo("misc-lang", "");
    }

    // title (same as tooltip)
    var node = elem;
    while (node && node.nodeType == Node.ELEMENT_NODE &&
           !(isHTMLElement(node,"") && node.title)) {
        node = node.parentNode;
    }

    if (node && node.nodeType == Node.ELEMENT_NODE) {
        miscFound = true;
        setInfo("misc-title", node.title);
    } else {
        setInfo("misc-title", "");
    }

    // table summary
    // tables could be nested so we walk up until we find one with summary
    var node = elem;
    while (node && node.nodeType == Node.ELEMENT_NODE &&
           !(isHTMLElement(node,"table") && node.summary)) {
        node = node.parentNode;
    }

    if (node && node.nodeType == Node.ELEMENT_NODE) {
        miscFound = true;
        setInfo("misc-tblsummary", node.summary);
    } else {
        setInfo("misc-tblsummary", "");
    }

    if (!miscFound)
        hideNode("sec-misc");
}


/*
 * Set text of node id to value
 * if value="" the node with specified id is hidden.
 * Node should be have one of these forms
 * <xul:text id="id-text" value=""/>
 * <xul:html id="id-text"/>
 */
function setInfo(id, value)
{
    if (value == "") {
        hideNode(id);
        return;
    }

    var node = document.getElementById(id+"-text");

    if (node.namespaceURI == XULNS && node.localName == "text") {
        node.setAttribute("value",value);

    } else if (node.namespaceURI == XULNS && node.localName == "html") {
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

// opens the link contained in the node's "value" attribute.
function openLink(node)
{
    var url = node.getAttribute("value");
    nodeView._content.document.location = url;
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

    var list = getHTMLElements(map.ownerDocument, "object");
    for (var i=0; i < list.length; i++) {
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

    var URL = Components.classes["@mozilla.org/network/standard-url;1"].createInstance(Components.interfaces["nsIURL"]);

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
    URL.spec = doc.location.href;
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
        return !name || node.tagName.toLowerCase() == name;

    return (!name || node.localName == name) && node.namespaceURI == XHTMLNS;
}
