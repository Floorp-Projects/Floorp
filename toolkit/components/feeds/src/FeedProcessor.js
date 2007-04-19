/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Robert Sayre.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <beng@google.com>
 *   Myk Melez <myk@mozilla.org>
 *   Michael Ventnor <m.ventnor@gmail.com>
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

function LOG(str) {
  dump("*** " + str + "\n");
}

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

const IO_CONTRACTID = "@mozilla.org/network/io-service;1"
const BAG_CONTRACTID = "@mozilla.org/hash-property-bag;1"
const ARRAY_CONTRACTID = "@mozilla.org/array;1";
const SAX_CONTRACTID = "@mozilla.org/saxparser/xmlreader;1";
const UNESCAPE_CONTRACTID = "@mozilla.org/feed-unescapehtml;1";

var gIoService = Cc[IO_CONTRACTID].getService(Ci.nsIIOService);
var gUnescapeHTML = Cc[UNESCAPE_CONTRACTID].
                    getService(Ci.nsIScriptableUnescapeHTML);

const XMLNS = "http://www.w3.org/XML/1998/namespace";
const RSS090NS = "http://my.netscape.com/rdf/simple/0.9/";
const WAIROLE_NS = "http://www.w3.org/2005/01/wai-rdf/GUIRoleTaxonomy#";

/***** Some general utils *****/
function strToURI(link, base) {
  var base = base || null;
  try {
    return gIoService.newURI(link, null, base);
  }
  catch(e) {
    return null;
  }
}

function isArray(a) {
  return isObject(a) && a.constructor == Array;
}

function isObject(a) {
  return (a && typeof a == "object") || isFunction(a);
}

function isFunction(a) {
  return typeof a == "function";
}

function isIID(a, iid) {
  var rv = false;
  try {
    a.QueryInterface(iid);
    rv = true;
  }
  catch(e) {
  }
  return rv;
}

function isIArray(a) {
  return isIID(a, Ci.nsIArray);
}

function isIFeedContainer(a) {
  return isIID(a, Ci.nsIFeedContainer);
}

function stripTags(someHTML) {
  return someHTML.replace(/<[^>]+>/g,"");
}

/**
 * Searches through an array of links and returns a JS array 
 * of matching property bags.
 */
const IANA_URI = "http://www.iana.org/assignments/relation/";
function findAtomLinks(rel, links) {
  var rvLinks = [];
  for (var i = 0; i < links.length; ++i) {
    var linkElement = links.queryElementAt(i, Ci.nsIPropertyBag2);
    // atom:link MUST have @href
    if (bagHasKey(linkElement, "href")) {
      var relAttribute = null;
      if (bagHasKey(linkElement, "rel"))
        relAttribute = linkElement.getPropertyAsAString("rel")
      if ((!relAttribute && rel == "alternate") || relAttribute == rel) {
        rvLinks.push(linkElement);
        continue;
      }
      // catch relations specified by IANA URI 
      if (relAttribute == IANA_URI + rel) {
        rvLinks.push(linkElement);
      }
    }
  }
  return rvLinks;
}

function xmlEscape(s) {
  s = s.replace(/&/g, "&amp;");
  s = s.replace(/>/g, "&gt;");
  s = s.replace(/</g, "&lt;");
  s = s.replace(/"/g, "&quot;");
  s = s.replace(/'/g, "&apos;");
  return s;
}

function arrayContains(array, element) {
  for (var i = 0; i < array.length; ++i) {
    if (array[i] == element) {
      return true;
    }
  }
  return false;
}

// XXX add hasKey to nsIPropertyBag
function bagHasKey(bag, key) {
  try {
    bag.getProperty(key);
    return true;
  }
  catch (e) {
    return false;
  }
}

function makePropGetter(key) {
  return function FeedPropGetter(bag) {
    try {
      return value = bag.getProperty(key);
    }
    catch(e) {
    }
    return null;
  }
}



/**
 * XXX Thunderbird's W3C-DTF function
 *
 * Converts a W3C-DTF (subset of ISO 8601) date string to an IETF date
 * string.  W3C-DTF is described in this note:
 * http://www.w3.org/TR/NOTE-datetime IETF is obtained via the Date
 * object's toUTCString() method.  The object's toString() method is
 * insufficient because it spells out timezones on Win32
 * (f.e. "Pacific Standard Time" instead of "PST"), which Mail doesn't
 * grok.  For info, see
 * http://lxr.mozilla.org/mozilla/source/js/src/jsdate.c#1526.
 */
const HOURS_TO_MINUTES = 60;
const MINUTES_TO_SECONDS = 60;
const SECONDS_TO_MILLISECONDS = 1000;
const MINUTES_TO_MILLISECONDS = MINUTES_TO_SECONDS * SECONDS_TO_MILLISECONDS;
const HOURS_TO_MILLISECONDS = HOURS_TO_MINUTES * MINUTES_TO_MILLISECONDS;
function W3CToIETFDate(dateString) {

  var parts = dateString.match(/(\d\d\d\d)(-(\d\d))?(-(\d\d))?(T(\d\d):(\d\d)(:(\d\d)(\.(\d+))?)?(Z|([+-])(\d\d):(\d\d))?)?/);

  // Here's an example of a W3C-DTF date string and what .match returns for it.
  // 
  // date: 2003-05-30T11:18:50.345-08:00
  // date.match returns array values:
  //
  //   0: 2003-05-30T11:18:50-08:00,
  //   1: 2003,
  //   2: -05,
  //   3: 05,
  //   4: -30,
  //   5: 30,
  //   6: T11:18:50-08:00,
  //   7: 11,
  //   8: 18,
  //   9: :50,
  //   10: 50,
  //   11: .345,
  //   12: 345,
  //   13: -08:00,
  //   14: -,
  //   15: 08,
  //   16: 00

  // Create a Date object from the date parts.  Note that the Date
  // object apparently can't deal with empty string parameters in lieu
  // of numbers, so optional values (like hours, minutes, seconds, and
  // milliseconds) must be forced to be numbers.
  var date = new Date(parts[1], parts[3] - 1, parts[5], parts[7] || 0,
                      parts[8] || 0, parts[10] || 0, parts[12] || 0);

  // We now have a value that the Date object thinks is in the local
  // timezone but which actually represents the date/time in the
  // remote timezone (f.e. the value was "10:00 EST", and we have
  // converted it to "10:00 PST" instead of "07:00 PST").  We need to
  // correct that.  To do so, we're going to add the offset between
  // the remote timezone and UTC (to convert the value to UTC), then
  // add the offset between UTC and the local timezone //(to convert
  // the value to the local timezone).

  // Ironically, W3C-DTF gives us the offset between UTC and the
  // remote timezone rather than the other way around, while the
  // getTimezoneOffset() method of a Date object gives us the offset
  // between the local timezone and UTC rather than the other way
  // around.  Both of these are the additive inverse (i.e. -x for x)
  // of what we want, so we have to invert them to use them by
  // multipying by -1 (f.e. if "the offset between UTC and the remote
  // timezone" is -5 hours, then "the offset between the remote
  // timezone and UTC" is -5*-1 = 5 hours).

  // Note that if the timezone portion of the date/time string is
  // absent (which violates W3C-DTF, although ISO 8601 allows it), we
  // assume the value to be in UTC.

  // The offset between the remote timezone and UTC in milliseconds.
  var remoteToUTCOffset = 0;
  if (parts[13] && parts[13] != "Z") {
    var direction = (parts[14] == "+" ? 1 : -1);
    if (parts[15])
      remoteToUTCOffset += direction * parts[15] * HOURS_TO_MILLISECONDS;
    if (parts[16])
      remoteToUTCOffset += direction * parts[16] * MINUTES_TO_MILLISECONDS;
  }
  remoteToUTCOffset = remoteToUTCOffset * -1; // invert it

  // The offset between UTC and the local timezone in milliseconds.
  var UTCToLocalOffset = date.getTimezoneOffset() * MINUTES_TO_MILLISECONDS;
  UTCToLocalOffset = UTCToLocalOffset * -1; // invert it
  date.setTime(date.getTime() + remoteToUTCOffset + UTCToLocalOffset);

  return date.toUTCString();
}

const RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
// namespace map
var gNamespaces = {
  "http://webns.net/mvcb/":"admin",
  "http://backend.userland.com/rss":"",
  "http://blogs.law.harvard.edu/tech/rss":"",
  "http://www.w3.org/2005/Atom":"atom",
  "http://purl.org/atom/ns#":"atom03",
  "http://purl.org/rss/1.0/modules/content/":"content",
  "http://purl.org/dc/elements/1.1/":"dc",
  "http://purl.org/dc/terms/":"dcterms",
  "http://www.w3.org/1999/02/22-rdf-syntax-ns#":"rdf",
  "http://purl.org/rss/1.0/":"rss1",
  "http://my.netscape.com/rdf/simple/0.9/":"rss1",
  "http://wellformedweb.org/CommentAPI/":"wfw",                              
  "http://purl.org/rss/1.0/modules/wiki/":"wiki", 
  "http://www.w3.org/XML/1998/namespace":"xml"
}

// We allow a very small set of namespaces in XHTML content,
// for attributes only
var gAllowedXHTMLNamespaces = {
  "http://www.w3.org/XML/1998/namespace":"xml",
  "http://www.w3.org/TR/xhtml2":"xhtml2",
  "http://www.w3.org/2005/07/aaa":"aaa",
  // if someone ns qualifies XHTML, we have to prefix it to avoid an
  // attribute collision.
  "http://www.w3.org/1999/xhtml":"xhtml"
}

function FeedResult() {}
FeedResult.prototype = {
  bozo: false,
  doc: null,
  version: null,
  headers: null,
  uri: null,
  stylesheet: null,

  registerExtensionPrefix: function FR_registerExtensionPrefix(ns, prefix) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  QueryInterface: function FR_QI(iid) {
    if (iid.equals(Ci.nsIFeedResult) ||
        iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NOINTERFACE;
  },
}  

function Feed() {
  this.subtitle = null;
  this.title = null;
  this.items = Cc[ARRAY_CONTRACTID].createInstance(Ci.nsIMutableArray);
  this.link = null;
  this.id = null;
  this.generator = null;
  this.authors = Cc[ARRAY_CONTRACTID].createInstance(Ci.nsIMutableArray);
  this.contributors = Cc[ARRAY_CONTRACTID].createInstance(Ci.nsIMutableArray);
  this.baseURI = null;
}

Feed.prototype = {
  searchLists: {
    title: ["title", "rss1:title", "atom03:title", "atom:title"],
    subtitle: ["description","dc:description","rss1:description",
               "atom03:tagline","atom:subtitle"],
    items: ["items","atom03_entries","entries"],
    id: ["atom:id","rdf:about"],
    generator: ["generator"],
    authors : ["authors"],
    contributors: ["contributors"],
    title: ["title","rss1:title", "atom03:title","atom:title"],
    link:  [["link",strToURI],["rss1:link",strToURI]],
    categories: ["categories", "dc:subject"],
    rights: ["atom03:rights","atom:rights"],
    cloud: ["cloud"],
    image: ["image", "rss1:image", "atom:logo"],
    textInput: ["textInput", "rss1:textinput"],
    skipDays: ["skipDays"],
    skipHours: ["skipHours"],
    updated: ["pubDate", "lastBuildDate", "atom03:modified", "dc:date",
              "dcterms:modified", "atom:updated"]
  },

  normalize: function Feed_normalize() {
    fieldsToObj(this, this.searchLists);
    if (this.skipDays)
      this.skipDays = this.skipDays.getProperty("days");
    if (this.skipHours)
      this.skipHours = this.skipHours.getProperty("hours");

    if (this.updated)
      this.updated = dateParse(this.updated);

    // Assign Atom link if needed
    if (bagHasKey(this.fields, "links"))
      this._atomLinksToURI();

    // Resolve relative image links
    if (this.image && bagHasKey(this.image, "url"))
      this._resolveImageLink();

    this._resetBagMembersToRawText([this.searchLists.subtitle, 
                                    this.searchLists.title]);
  },

  _atomLinksToURI: function Feed_linkToURI() {
    var links = this.fields.getPropertyAsInterface("links", Ci.nsIArray);
    var alternates = findAtomLinks("alternate", links);
    if (alternates.length > 0) {
      var href = alternates[0].getPropertyAsAString("href");
      var base;
      if (bagHasKey(alternates[0], "xml:base"))
        base = alternates[0].getPropertyAsAString("xml:base");
      this.link = this._resolveURI(href, base);
    }
  },

  _resolveImageLink: function Feed_resolveImageLink() {
    var base;
    if (bagHasKey(this.image, "xml:base"))
      base = this.image.getPropertyAsAString("xml:base");
    var url = this._resolveURI(this.image.getPropertyAsAString("url"), base);
    if (url)
      this.image.setPropertyAsAString("url", url.spec);
  },

  _resolveURI: function Feed_resolveURI(linkSpec, baseSpec) {
    var uri = null;
    try {
      var base = baseSpec ? strToURI(baseSpec, this.baseURI) : this.baseURI;
      uri = strToURI(linkSpec, base);
    }
    catch(e) {
      LOG(e);
    }

    return uri;
  },

  // reset the bag to raw contents, not text constructs
  _resetBagMembersToRawText: function Feed_resetBagMembers(fieldLists) {
    for (var i=0; i<fieldLists.length; i++) {      
      for (var j=0; j<fieldLists[i].length; j++) {
        if (bagHasKey(this.fields, fieldLists[i][j])) {
          var textConstruct = this.fields.getProperty(fieldLists[i][j]);
          this.fields.setPropertyAsAString(fieldLists[i][j],
                                           textConstruct.text);
        }
      }
    }
  },
   
  QueryInterface: function Feed_QI(iid) {
    if (iid.equals(Ci.nsIFeed) ||
        iid.equals(Ci.nsIFeedContainer) ||
        iid.equals(Ci.nsISupports))
    return this;
    throw Cr.NS_ERROR_NOINTERFACE;
  }
}

function Entry() {
  this.summary = null;
  this.content = null;
  this.title = null;
  this.fields = Cc["@mozilla.org/hash-property-bag;1"].
    createInstance(Ci.nsIWritablePropertyBag2);
  this.link = null;
  this.id = null;
  this.baseURI = null;
  this.updated = null;
  this.published = null;
  this.authors = Cc[ARRAY_CONTRACTID].createInstance(Ci.nsIMutableArray);
  this.contributors = Cc[ARRAY_CONTRACTID].createInstance(Ci.nsIMutableArray);
}
  
Entry.prototype = {
  fields: null,
  enclosures: null,
  mediaContent: null,
  
  searchLists: {
    title: ["title", "rss1:title", "atom03:title", "atom:title"],
    link: [["link",strToURI],["rss1:link",strToURI]],
    id: [["guid", makePropGetter("guid")], "rdf:about",
         "atom03:id", "atom:id"],
    authors : ["authors"],
    contributors: ["contributors"],
    summary: ["description", "rss1:description", "dc:description",
              "atom03:summary", "atom:summary"],
    content: ["content:encoded","atom03:content","atom:content"],
    rights: ["atom03:rights","atom:rights"],
    published: ["atom03:issued", "dcterms:issued", "atom:published"],
    updated: ["pubDate", "atom03:modified", "dc:date", "dcterms:modified",
              "atom:updated"]
  },
  
  normalize: function Entry_normalize() {
    fieldsToObj(this, this.searchLists);
 
    // Assign Atom link if needed
    if (bagHasKey(this.fields, "links"))
      this._atomLinksToURI();
 
    // The link might be a guid w/ permalink=true
    if (!this.link && bagHasKey(this.fields, "guid")) {
      var guid = this.fields.getProperty("guid");
      var isPermaLink = true;
      
      if (bagHasKey(guid, "isPermaLink"))
        isPermaLink = guid.getProperty("isPermaLink").toLowerCase() != "false";
      
      if (guid && isPermaLink)
        this.link = strToURI(guid.getProperty("guid"));
    }

    if (this.updated)
      this.updated = dateParse(this.updated);
    if (this.published)
      this.published = dateParse(this.published);

    this._resetBagMembersToRawText([this.searchLists.content, 
                                    this.searchLists.summary, 
                                    this.searchLists.title]);
  },
  
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIFeedEntry) ||
        iid.equals(Ci.nsIFeedContainer) ||
        iid.equals(Ci.nsISupports))
    return this;

    throw Cr.NS_ERROR_NOINTERFACE;
  }
}

Entry.prototype._atomLinksToURI = Feed.prototype._atomLinksToURI;
Entry.prototype._resolveURI = Feed.prototype._resolveURI;
Entry.prototype._resetBagMembersToRawText = 
   Feed.prototype._resetBagMembersToRawText;

// TextConstruct represents and element that could contain (X)HTML
function TextConstruct() {
  this.lang = null;
  this.base = null;
  this.type = "text";
  this.text = null;
}

TextConstruct.prototype = {
  plainText: function TC_plainText() {
    if (this.type != "text") {
      return gUnescapeHTML.unescape(stripTags(this.text));
    }
    return this.text;
  },

  createDocumentFragment: function TC_createDocumentFragment(element) {
    if (this.type == "text") {
      var doc = element.ownerDocument;
      var docFragment = doc.createDocumentFragment();
      var node = doc.createTextNode(this.text);
      docFragment.appendChild(node);
      return docFragment;
    }
    var isXML;
    if (this.type == "xhtml")
      isXML = true
    else if (this.type == "html")
      isXML = false;
    else
      return null;

    return gUnescapeHTML.parseFragment(this.text, isXML, this.base, element);
  },
 
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIFeedTextConstruct) ||
        iid.equals(Ci.nsISupports))
    return this;

    throw Cr.NS_ERROR_NOINTERFACE;
  }
}

// Generator represents the software that produced the feed
function Generator() {
  this.lang = null;
  this.agent = null;
  this.version = null;
  this.uri = null;

  // nsIFeedElementBase
  this._attributes = null;
  this.baseURI = null;
}

Generator.prototype = {

  get attributes() {
    return this._attributes;
  },

  set attributes(value) {
    this._attributes = value;
    this.version = this._attributes.getValueFromName("","version");
    var uriAttribute = this._attributes.getValueFromName("","uri") ||
                       this._attributes.getValueFromName("","url");
    this.uri = strToURI(uriAttribute, this.baseURI);

    // RSS1
    uriAttribute = this._attributes.getValueFromName(RDF_NS,"resource");
    if (uriAttribute) {
      this.agent = uriAttribute;
      this.uri = strToURI(uriAttribute, this.baseURI);
    }
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIFeedGenerator) ||
        iid.equals(Ci.nsIFeedElementBase) ||
        iid.equals(Ci.nsISupports))
    return this;

    throw Cr.NS_ERROR_NOINTERFACE;
  }
}

function Person() {
  this.name = null;
  this.uri = null;
  this.email = null;

  // nsIFeedElementBase
  this.attributes = null;
  this.baseURI = null;
}

Person.prototype = {
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIFeedPerson) ||
        iid.equals(Ci.nsIFeedElementBase) ||
        iid.equals(Ci.nsISupports))
    return this;

    throw Cr.NS_ERROR_NOINTERFACE;
  }
}

/** 
 * Map a list of fields into properties on a container.
 *
 * @param container An nsIFeedContainer
 * @param fields A list of fields to search for. List members can
 *               be a list, in which case the second member is 
 *               transformation function (like parseInt).
 */
function fieldsToObj(container, fields) {
  var props,prop,field,searchList;
  for (var key in fields) {
    searchList = fields[key];
    for (var i=0; i < searchList.length; ++i) {
      props = searchList[i];
      prop = null;
      field = isArray(props) ? props[0] : props;
      try {
        prop = container.fields.getProperty(field);
      } 
      catch(e) { 
      }
      if (prop) {
        prop = isArray(props) ? props[1](prop) : prop;
        container[key] = prop;
      }
    }
  }
}

/**
 * Lower cases an element's localName property
 * @param   element A DOM element.
 *
 * @returns The lower case localName property of the specified element
 */
function LC(element) {
  return element.localName.toLowerCase();
}

// TODO move these post-processor functions
// create a generator element
function atomGenerator(s, generator) {
  generator.QueryInterface(Ci.nsIFeedGenerator);
  generator.agent = trimString(s);
  return generator;
}

// post-process atom:logo to create an RSS2-like structure
function atomLogo(s, logo) {
  logo.setPropertyAsAString("url", trimString(s));
}

// post-process an RSS category, map it to the Atom fields.
function rssCatTerm(s, cat) {
  // add slash handling?
  cat.setPropertyAsAString("term", trimString(s));
  return cat;
} 

// post-process a GUID 
function rssGuid(s, guid) {
  guid.setPropertyAsAString("guid", trimString(s));
  return guid;
}

// post-process an RSS author element
//
// It can contain a field like this:
// 
//  <author>lawyer@boyer.net (Lawyer Boyer)</author>
//
// or, delightfully, a field like this:
//
//  <dc:creator>Simon St.Laurent (mailto:simonstl@simonstl.com)</dc:creator>
//
// We want to split this up and assign it to corresponding Atom
// fields.
//
function rssAuthor(s,author) {
  author.QueryInterface(Ci.nsIFeedPerson);
  // check for RSS2 string format
  var chars = trimString(s);
  var matches = chars.match(/(.*)\((.*)\)/);
  var emailCheck = 
    /^([a-zA-Z0-9_\.\-])+\@(([a-zA-Z0-9\-])+\.)+([a-zA-Z0-9]{2,4})+$/;
  if (matches) {
    var match1 = trimString(matches[1]);
    var match2 = trimString(matches[2]);
    if (match2.indexOf("mailto:") == 0)
      match2 = match2.substring(7);
    if (emailCheck.test(match1)) {
      author.email = match1;
      author.name = match2;
    }
    else if (emailCheck.test(match2)) {
      author.email = match2;
      author.name = match1;
    }
    else {
      // put it back together
      author.name = match1 + " (" + match2 + ")";
    }
  }
  else {
    author.name = chars;
    if (chars.indexOf('@'))
      author.email = chars;
  }
  return author;
}

//
// skipHours and skipDays map to arrays, so we need to change the
// string to an nsISupports in order to stick it in there.
//
function rssArrayElement(s) {
  var str = Cc["@mozilla.org/supports-string;1"].
              createInstance(Ci.nsISupportsString);
  str.data = s;
  str.QueryInterface(Ci.nsISupportsString);
  return str;
}

/***** Some feed utils from TBird *****/

/**
 * Tests a RFC822 date against a regex.
 * @param aDateStr A string to test as an RFC822 date.
 *
 * @returns A boolean indicating whether the string is a valid RFC822 date.
 */
function isValidRFC822Date(aDateStr) {
  var regex = new RegExp(RFC822_RE);
  return regex.test(aDateStr);
}

/**
 * Removes leading and trailing whitespace from a string.
 * @param s The string to trim.
 *
 * @returns A new string with whitespace stripped.
 */
function trimString(s) {
  return(s.replace(/^\s+/, "").replace(/\s+$/, ""));
}

// Regular expression matching RFC822 dates 
const RFC822_RE = "^((Mon|Tue|Wed|Thu|Fri|Sat|Sun)([a-z]+)?,? *)?\\d\\d?"
+ " +(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)([a-z]+)?"
+ " +\\d\\d(\\d\\d)? +\\d?\\d:\\d\\d(:\\d\\d)?"
+ " +([+-]?\\d\\d\\d\\d|GMT|UT[C]?|(E|C|M|P)(ST|DT)|[A-IK-Z])$";

/**
 * XXX -- need to decide what this should return. 
 * XXX -- Is there a Date class usable from C++?
 *
 * Tries tries parsing various date formats.
 * @param dateString
 *        A string that is supposedly an RFC822 or RFC3339 date.
 * @returns A Date.toString XXX--fixme
 */
function dateParse(dateString) {
  var date = trimString(dateString);

  if (date.search(/^\d\d\d\d/) != -1) //Could be a ISO8601/W3C date
    return W3CToIETFDate(dateString);

  if (isValidRFC822Date(date))
    return date; 
  
  if (!isNaN(parseInt(date, 10))) { 
    //It's an integer, so maybe it's a timestamp
    var d = new Date(parseInt(date, 10) * 1000);
    var now = new Date();
    var yeardiff = now.getFullYear() - d.getFullYear();
    if ((yeardiff >= 0) && (yeardiff < 3)) {
      // it's quite likely the correct date. 3 years is an arbitrary cutoff,
      // but this is an invalid date format, and there's no way to verify
      // its correctness.
      return d.toString();
    }
  }
  // Can't help.
  return null;
} 


const XHTML_NS = "http://www.w3.org/1999/xhtml";

// The XHTMLHandler handles inline XHTML found in things like atom:summary
function XHTMLHandler(processor, isAtom, waiPrefixes) {
  this._buf = "";
  this._processor = processor;
  this._depth = 0;
  this._isAtom = isAtom;
  // a stack of lists tracking in-scope namespaces
  this._inScopeNS = [];
  this._waiPrefixes = waiPrefixes;
}

// The fidelity can be improved here, to allow handling of stuff like
// SVG and MathML. XXX
XHTMLHandler.prototype = {

   // look back up at the declared namespaces 
   // we always use the same prefixes for our safe stuff
  _isInScope: function XH__isInScope(ns) {
    for (var i in this._inScopeNS) {
      for (var uri in this._inScopeNS[i]) {
        if (this._inScopeNS[i][uri] == ns)
          return true;
      }
    }
    return false;
  },

  startDocument: function XH_startDocument() {
  },
  endDocument: function XH_endDocument() {
  },
  startElement: function XH_startElement(uri, localName, qName, attributes) {
    ++this._depth;
    this._inScopeNS.push([]);

    // RFC4287 requires XHTML to be wrapped in a div that is *not* part of 
    // the content. This prevents people from screwing up namespaces, but
    // we need to skip it here.
    if (this._isAtom && this._depth == 1 && localName == "div")
      return;

    // If it's an XHTML element, record it. Otherwise, it's ignored.
    if (uri == XHTML_NS) {
      this._buf += "<" + localName;
      var uri;
      for (var i=0; i < attributes.length; ++i) {
        uri = attributes.getURI(i);
        // XHTML attributes aren't in a namespace
        if (uri == "") { 
          this._buf += (" " + attributes.getLocalName(i) + "='" +
                        xmlEscape(attributes.getValue(i)) + "'");
        } else {
          // write a small set of allowed attribute namespaces
          var prefix = gAllowedXHTMLNamespaces[uri];
          if (prefix != null) {
            // The attribute value we'll attempt to write
            var attributeValue = xmlEscape(attributes.getValue(i));

            // More QName abuse from W3C
            var rolePrefix = "";
            if (attributes.getLocalName(i) == "role") {
              for (var aPrefix in this._waiPrefixes) {
                if (attributeValue.indexOf(aPrefix + ":") == 0) {     
                  // Now, due to the terrible layer mismatch 
                  // that is QNames in content, we have to see
                  // if the attribute value clashes with our 
                  // namespace declarations.
                  var isCollision = false;
                  for (var uriKey in gAllowedXHTMLNamespaces) {
                    if (gAllowedXHTMLNamespaces[uriKey] == aPrefix)
                      isCollision = true;
                  }
                  
                  if (isCollision) {
                    rolePrefix = aPrefix + i;
                    attributeValue = 
                      rolePrefix + ":" + 
                      attributeValue.substring(aPrefix.length + 1);
                  } else {
                    rolePrefix = aPrefix;
                  }

                  break;
                }
              }

              if (rolePrefix)
                this._buf += (" xmlns:" + rolePrefix + 
                              "='" + WAIROLE_NS + "'");
            }

            // it's an allowed attribute NS.            
            // write the attribute
            this._buf += (" " + prefix + ":" + 
                          attributes.getLocalName(i) + 
                          "='" + attributeValue + "'");

            // write an xmlns declaration if necessary
            if (prefix != "xml" && !this._isInScope(uri)) {
              this._inScopeNS[this._inScopeNS.length - 1].push(uri);
              this._buf += " xmlns:" + prefix + "='" + uri + "'";
            }
          }
        }
      }
      this._buf += ">";
    }
  },
  endElement: function XH_endElement(uri, localName, qName) {
    --this._depth;
    this._inScopeNS.pop();

    // We need to skip outer divs in Atom. See comment in startElement.
    if (this._isAtom && this._depth == 0 && localName == "div")
      return;

    // When we peek too far, go back to the main processor
    if (this._depth < 0) {
      this._processor.returnFromXHTMLHandler(trimString(this._buf),
                                             uri, localName, qName);
      return;
    }
    // If it's an XHTML element, record it. Otherwise, it's ignored.
    if (uri == XHTML_NS) {
      this._buf += "</" + localName + ">";
    }
  },
  characters: function XH_characters(data) {
    this._buf += xmlEscape(data);
  },
  startPrefixMapping: function XH_startPrefixMapping(prefix, uri) {
    if (prefix && uri == WAIROLE_NS) 
      this._waiPrefixes[prefix] = WAIROLE_NS;
  },
  endPrefixMapping: function FP_endPrefixMapping(prefix) {
    if (prefix)
      delete this._waiPrefixes[prefix];
  },
  processingInstruction: function XH_processingInstruction() {
  }, 
}

/**
 * The ExtensionHandler deals with elements we haven't explicitly
 * added to our transition table in the FeedProcessor.
 */
function ExtensionHandler(processor) {
  this._buf = "";
  this._depth = 0;
  this._hasChildElements = false;

  // The FeedProcessor
  this._processor = processor;

  // Fields of the outermost extension element.
  this._localName = null;
  this._uri = null;
  this._qName = null;
  this._attrs = null;
}

ExtensionHandler.prototype = {
  startDocument: function EH_startDocument() {
  },
  endDocument: function EH_endDocument() {
  },
  startElement: function EH_startElement(uri, localName, qName, attrs) {
    ++this._depth;
    var prefix = gNamespaces[uri] ? gNamespaces[uri] + ":" : "";
    var key =  prefix + localName;
    
    if (this._depth == 1) {
      this._uri = uri;
      this._localName = localName;
      this._qName = qName;
      this._attrs = attrs;
    }
    
    // if we descend into another element, we won't send text
    this._hasChildElements = (this._depth > 1);
    
  },
  endElement: function EH_endElement(uri, localName, qName) {
    --this._depth;
    if (this._depth == 0) {
      var text = this._hasChildElements ? null : trimString(this._buf);
      this._processor.returnFromExtHandler(this._uri, this._localName, 
                                           text, this._attrs);
    }
  },
  characters: function EH_characters(data) {
    if (!this._hasChildElements)
      this._buf += data;
  },
  startPrefixMapping: function EH_startPrefixMapping() {
  },
  endPrefixMapping: function EH_endPrefixMapping() {
  },
  processingInstruction: function EH_processingInstruction() {
  }, 
};


/**
 * ElementInfo is a simple container object that describes
 * some characteristics of a feed element. For example, it
 * says whether an element can be expected to appear more
 * than once inside a given entry or feed.
 */ 
function ElementInfo(fieldName, containerClass, closeFunc, isArray) {
  this.fieldName = fieldName;
  this.containerClass = containerClass;
  this.closeFunc = closeFunc;
  this.isArray = isArray;
  this.isWrapper = false;
}

/**
 * FeedElementInfo represents a feed element, usually the root.
 */
function FeedElementInfo(fieldName, feedVersion) {
  this.isWrapper = false;
  this.fieldName = fieldName;
  this.feedVersion = feedVersion;
}

/**
 * Some feed formats include vestigial wrapper elements that we don't
 * want to include in our object model, but we do need to keep track
 * of during parsing.
 */
function WrapperElementInfo(fieldName) {
  this.isWrapper = true;
  this.fieldName = fieldName;
}

/***** The Processor *****/
function FeedProcessor() {
  this._reader = Cc[SAX_CONTRACTID].createInstance(Ci.nsISAXXMLReader);
  this._buf =  "";
  this._feed = Cc[BAG_CONTRACTID].createInstance(Ci.nsIWritablePropertyBag2);
  this._handlerStack = [];
  this._xmlBaseStack = []; // sparse array keyed to nesting depth
  this._depth = 0;
  this._state = "START";
  this._result = null;
  this._extensionHandler = null;
  this._xhtmlHandler = null;
  
  // http://www.w3.org/WAI/PF/GUI/ uses QNames in content :(
  this._waiPrefixes = {};

  // The nsIFeedResultListener waiting for the parse results
  this.listener = null;

  // These elements can contain (X)HTML or plain text.
  // We keep a table here that contains their default treatment
  this._textConstructs = {"atom:title":"text",
                          "atom:summary":"text",
                          "atom:rights":"text",
                          "atom:content":"text",
                          "atom:subtitle":"text",
                          "description":"html",
                          "rss1:description":"html",
                          "dc:description":"html",
                          "content:encoded":"html",
                          "title":"text",
                          "rss1:title":"text",
                          "atom03:title":"text",
                          "atom03:tagline":"text",
                          "atom03:summary":"text",
                          "atom03:content":"text"};
  this._stack = [];

  this._trans = {   
    "START": {
      //If we hit a root RSS element, treat as RSS2.
      "rss": new FeedElementInfo("RSS2", "rss2"),

      // If we hit an RDF element, if could be RSS1, but we can't
      // verify that until we hit a rss1:channel element.
      "rdf:RDF": new WrapperElementInfo("RDF"),

      // If we hit a Atom 1.0 element, treat as Atom 1.0.
      "atom:feed": new FeedElementInfo("Atom", "atom"),

      // Treat as Atom 0.3
      "atom03:feed": new FeedElementInfo("Atom03", "atom03"),
    },
    
    /********* RSS2 **********/
    "IN_RSS2": {
      "channel": new WrapperElementInfo("channel")
    },

    "IN_CHANNEL": {
      "item": new ElementInfo("items", Cc[ENTRY_CONTRACTID], null, true),
      "managingEditor": new ElementInfo("authors", Cc[PERSON_CONTRACTID],
                                        rssAuthor, true),
      "dc:creator": new ElementInfo("authors", Cc[PERSON_CONTRACTID],
                                    rssAuthor, true),
      "dc:author": new ElementInfo("authors", Cc[PERSON_CONTRACTID],
                                   rssAuthor, true),
      "dc:contributor": new ElementInfo("contributors", Cc[PERSON_CONTRACTID],
                                         rssAuthor, true),
      "category": new ElementInfo("categories", null, rssCatTerm, true),
      "cloud": new ElementInfo("cloud", null, null, false),
      "image": new ElementInfo("image", null, null, false),
      "textInput": new ElementInfo("textInput", null, null, false),
      "skipDays": new ElementInfo("skipDays", null, null, false),
      "skipHours": new ElementInfo("skipHours", null, null, false),
      "generator": new ElementInfo("generator", Cc[GENERATOR_CONTRACTID],
                                   atomGenerator, false),
    },

    "IN_ITEMS": {
      "author": new ElementInfo("authors", Cc[PERSON_CONTRACTID],
                                rssAuthor, true),
      "dc:creator": new ElementInfo("authors", Cc[PERSON_CONTRACTID],
                                    rssAuthor, true),
      "dc:author": new ElementInfo("authors", Cc[PERSON_CONTRACTID],
                                   rssAuthor, true),
      "dc:contributor": new ElementInfo("contributors", Cc[PERSON_CONTRACTID],
                                         rssAuthor, true),
      "category": new ElementInfo("categories", null, rssCatTerm, true),
      "enclosure": new ElementInfo("enclosure", null, null, true),
      "guid": new ElementInfo("guid", null, rssGuid, false)
    },

    "IN_SKIPDAYS": {
      "day": new ElementInfo("days", null, rssArrayElement, true)
    },

    "IN_SKIPHOURS":{
      "hour": new ElementInfo("hours", null, rssArrayElement, true)
    },

    /********* RSS1 **********/
    "IN_RDF": {
      // If we hit a rss1:channel, we can verify that we have RSS1
      "rss1:channel": new FeedElementInfo("rdf_channel", "rss1"),
      "rss1:image": new ElementInfo("image", null, null, false),
      "rss1:textinput": new ElementInfo("textInput", null, null, false),
      "rss1:item": new ElementInfo("items", Cc[ENTRY_CONTRACTID], null, true),
    },

    "IN_RDF_CHANNEL": {
      "admin:generatorAgent": new ElementInfo("generator",
                                              Cc[GENERATOR_CONTRACTID],
                                              null, false),
      "dc:creator": new ElementInfo("authors", Cc[PERSON_CONTRACTID],
                                    rssAuthor, true),
      "dc:author": new ElementInfo("authors", Cc[PERSON_CONTRACTID],
                                   rssAuthor, true),
      "dc:contributor": new ElementInfo("contributors", Cc[PERSON_CONTRACTID],
                                         rssAuthor, true),
    },

    /********* ATOM 1.0 **********/
    "IN_ATOM": {
      "atom:author": new ElementInfo("authors", Cc[PERSON_CONTRACTID],
                                     null, true),
      "atom:generator": new ElementInfo("generator", Cc[GENERATOR_CONTRACTID],
                                        atomGenerator, false),
      "atom:contributor": new ElementInfo("contributors",  Cc[PERSON_CONTRACTID],
                                          null, true),
      "atom:link": new ElementInfo("links", null, null, true),
      "atom:logo": new ElementInfo("atom:logo", null, atomLogo, false),
      "atom:entry": new ElementInfo("entries", Cc[ENTRY_CONTRACTID],
                                    null, true)
    },

    "IN_ENTRIES": {
      "atom:author": new ElementInfo("authors", Cc[PERSON_CONTRACTID],
                                     null, true),
      "atom:contributor": new ElementInfo("contributors", Cc[PERSON_CONTRACTID],
                                          null, true),
      "atom:link": new ElementInfo("links", null, null, true),
    },

    /********* ATOM 0.3 **********/
    "IN_ATOM03": {
      "atom03:author": new ElementInfo("authors", Cc[PERSON_CONTRACTID],
                                       null, true),
      "atom03:contributor": new ElementInfo("contributors",
                                            Cc[PERSON_CONTRACTID],
                                            null, true),
      "atom03:link": new ElementInfo("links", null, null, true),
      "atom03:entry": new ElementInfo("atom03_entries", Cc[ENTRY_CONTRACTID],
                                      null, true),
      "atom03:generator": new ElementInfo("generator", Cc[GENERATOR_CONTRACTID],
                                          atomGenerator, false),
    },

    "IN_ATOM03_ENTRIES": {
      "atom03:author": new ElementInfo("authors", Cc[PERSON_CONTRACTID],
                                       null, true),
      "atom03:contributor": new ElementInfo("contributors",
                                            Cc[PERSON_CONTRACTID],
                                            null, true),
      "atom03:link": new ElementInfo("links", null, null, true),
      "atom03:entry": new ElementInfo("atom03_entries", Cc[ENTRY_CONTRACTID],
                                      null, true)
    }
  }
}

// See startElement for a long description of how feeds are processed.
FeedProcessor.prototype = { 
  
  // Set ourselves as the SAX handler, and set the base URI
  _init: function FP_init(uri) {
    this._reader.contentHandler = this;
    this._reader.errorHandler = this;
    this._result = Cc[FR_CONTRACTID].createInstance(Ci.nsIFeedResult);
    if (uri) {
      this._result.uri = uri;
      this._reader.baseURI = uri;
      this._xmlBaseStack[0] = uri;
    }
  },

  // This function is called once we figure out what type of feed
  // we're dealing with. Some feed types require digging a bit further
  // than the root.
  _docVerified: function FP_docVerified(version) {
    this._result.doc = Cc[FEED_CONTRACTID].createInstance(Ci.nsIFeed);
    this._result.doc.baseURI = 
      this._xmlBaseStack[this._xmlBaseStack.length - 1];
    this._result.doc.fields = this._feed;
    this._result.version = version;
  },

  // When we're done with the feed, let the listener know what
  // happened.
  _sendResult: function FP_sendResult() {
    try {
      // Can be null when a non-feed is fed to us
      if (this._result.doc)
        this._result.doc.normalize();
    }
    catch (e) {
      LOG("FIXME: " + e);
    }

    try {
      if (this.listener != null)
        this.listener.handleResult(this._result);
    }
    finally {
      this._result = null;
    }
  },

  // Parsing functions
  parseFromStream: function FP_parseFromStream(stream, uri) {
    this._init(uri);
    this._reader.parseFromStream(stream, null, stream.available(), 
                                 "application/xml");
    this._reader = null;
  },

  parseFromString: function FP_parseFromString(inputString, uri) {
    this._init(uri);
    this._reader.parseFromString(inputString, "application/xml");
    this._reader = null;
  },

  parseAsync: function FP_parseAsync(requestObserver, uri) {
    this._init(uri);
    this._reader.parseAsync(requestObserver);
  },

  // nsIStreamListener 

  // The XMLReader will throw sensible exceptions if these get called
  // out of order.
  onStartRequest: function FP_onStartRequest(request, context) {
    this._reader.onStartRequest(request, context);
  },

  onStopRequest: function FP_onStopRequest(request, context, statusCode) {
    try {
      this._reader.onStopRequest(request, context, statusCode);
    }
    finally {
      this._reader = null;
    }
  },

  onDataAvailable:
  function FP_onDataAvailable(request, context, inputStream, offset, count) {
    this._reader.onDataAvailable(request, context, inputStream, offset, count);
  },

  // nsISAXErrorHandler

  // We only care about fatal errors. When this happens, we may have
  // parsed through the feed metadata and some number of entries. The
  // listener can still show some of that data if it wants, and we'll
  // set the bozo bit to indicate we were unable to parse all the way
  // through.
  fatalError: function FP_reportError() {
    this._result.bozo = true;
    //XXX need to QI to FeedProgressListener
    this._sendResult();
  },

  // nsISAXContentHandler

  startDocument: function FP_startDocument() {
    //LOG("----------");
  },

  endDocument: function FP_endDocument() {
    this._sendResult();
  },

  // The transitions defined above identify elements that contain more
  // than just text. For example RSS items contain many fields, and so
  // do Atom authors. The only commonly used elements that contain
  // mixed content are Atom Text Constructs of type="xhtml", which we
  // delegate to another handler for cleaning. That leaves a couple
  // different types of elements to deal with: those that should occur
  // only once, such as title elements, and those that can occur
  // multiple times, such as the RSS category element and the Atom
  // link element. Most of the RSS1/DC elements can occur multiple
  // times in theory, but in practice, the only ones that do have
  // analogues in Atom. 
  //
  // Some elements are also groups of attributes or sub-elements,
  // while others are simple text fields. For the most part, we don't
  // have to pay explicit attention to the simple text elements,
  // unless we want to post-process the resulting string to transform
  // it into some richer object like a Date or URI.
  //
  // Elements that have more sophisticated content models still end up
  // being dictionaries, whether they are based on attributes like RSS
  // cloud, sub-elements like Atom author, or even items and
  // entries. These elements are treated as "containers". It's
  // theoretically possible for a container to have an attribute with 
  // the same universal name as a sub-element, but none of the feed
  // formats allow this by default, and I don't of any extension that
  // works this way.
  //
  startElement: function FP_startElement(uri, localName, qName, attributes) {
    this._buf = "";
    ++this._depth;
    var elementInfo;

    //LOG("<" + localName + ">");

    // Check for xml:base
    var base = attributes.getValueFromName(XMLNS, "base");
    if (base) {
      this._xmlBaseStack[this._depth] =
        strToURI(base, this._xmlBaseStack[this._xmlBaseStack.length - 1]);
    }

    // To identify the element we're dealing with, we look up the
    // namespace URI in our gNamespaces dictionary, which will give us
    // a "canonical" prefix for a namespace URI. For example, this
    // allows Dublin Core "creator" elements to be consistently mapped
    // to "dc:creator", for easy field access by consumer code. This
    // strategy also happens to shorten up our state table.
    var key =  this._prefixForNS(uri) + localName;

    // Check to see if we need to hand this off to our XHTML handler.
    // The elements we're dealing with will look like this:
    // 
    // <title type="xhtml">
    //   <div xmlns="http://www.w3.org/1999/xhtml">
    //     A title with <b>bold</b> and <i>italics</i>.
    //   </div>
    // </title>
    //
    // When it returns in returnFromXHTMLHandler, the handler should
    // give us back a string like this: 
    // 
    //    "A title with <b>bold</b> and <i>italics</i>."
    //
    // The Atom spec explicitly says the div is not part of the content,
    // and explicitly allows whitespace collapsing.
    // 
    if ((this._result.version == "atom" || this._result.version == "atom03") &&
        this._textConstructs[key] != null) {
      var type = attributes.getValueFromName("","type");
      if (type != null && type.indexOf("xhtml") >= 0) {
        this._xhtmlHandler = 
          new XHTMLHandler(this, (this._result.version == "atom"), 
                           this._waiPrefixes);
        this._reader.contentHandler = this._xhtmlHandler;
        return;
      }
    }

    // Check our current state, and see if that state has a defined
    // transition. For example, this._trans["atom:entry"]["atom:author"]
    // will have one, and it tells us to add an item to our authors array.
    if (this._trans[this._state] && this._trans[this._state][key]) {
      elementInfo = this._trans[this._state][key];
    }
    else {
      // If we don't have a transition, hand off to extension handler
      this._extensionHandler = new ExtensionHandler(this);
      this._reader.contentHandler = this._extensionHandler;
      this._extensionHandler.startElement(uri, localName, qName, attributes);
      return;
    }
      
    // This distinguishes wrappers like 'channel' from elements
    // we'd actually like to do something with (which will test true).
    this._handlerStack[this._depth] = elementInfo; 
    if (elementInfo.isWrapper) {
      this._state = "IN_" + elementInfo.fieldName.toUpperCase();
      this._stack.push([this._feed, this._state]);
    } 
    else if (elementInfo.feedVersion) {
      this._state = "IN_" + elementInfo.fieldName.toUpperCase();

      // Check for the older RSS2 variants
      if (elementInfo.feedVersion == "rss2")
        elementInfo.feedVersion = this._findRSSVersion(attributes);
      else if (uri == RSS090NS)
        elementInfo.feedVersion = "rss090";

      this._docVerified(elementInfo.feedVersion);
      this._stack.push([this._feed, this._state]);
      this._mapAttributes(this._feed, attributes);
    }
    else {
      this._state = this._processComplexElement(elementInfo, attributes);
    }
  },

  // In the endElement handler, we decrement the stack and look
  // for cleanup/transition functions to execute. The second part
  // of the state transition works as above in startElement, but
  // the state we're looking for is prefixed with an underscore
  // to distinguish endElement events from startElement events.
  endElement:  function FP_endElement(uri, localName, qName) {
    var elementInfo = this._handlerStack[this._depth];
    //LOG("</" + localName + ">");
    if (elementInfo && !elementInfo.isWrapper)
      this._closeComplexElement(elementInfo);
  
    // cut down xml:base context
    if (this._xmlBaseStack.length == this._depth + 1)
      this._xmlBaseStack = this._xmlBaseStack.slice(0, this._depth);

    // our new state is whatever is at the top of the stack now
    if (this._stack.length > 0)
      this._state = this._stack[this._stack.length - 1][1];
    this._handlerStack = this._handlerStack.slice(0, this._depth);
    --this._depth;
  },

  // Buffer up character data. The buffer is cleared with every
  // opening element.
  characters: function FP_characters(data) {
    this._buf += data;
  },
  // TODO: It would be nice to check new prefixes here, and if they
  // don't conflict with the ones we've defined, throw them in a 
  // dictionary to check.
  startPrefixMapping: function FP_startPrefixMapping(prefix, uri) {
    // Thanks for QNames in content, W3C
    // This will even be a perf hit for every single feed
    // http://www.w3.org/WAI/PF/GUI/
    if (prefix && uri == WAIROLE_NS) 
      this._waiPrefixes[prefix] = WAIROLE_NS;
  },
  
  endPrefixMapping: function FP_endPrefixMapping(prefix) {
    if (prefix)
      delete this._waiPrefixes[prefix];
  },
  
  processingInstruction: function FP_processingInstruction(target, data) {
    if (target == "xml-stylesheet") {
      var hrefAttribute = data.match(/href=[\"\'](.*?)[\"\']/);
      if (hrefAttribute && hrefAttribute.length == 2) 
        this._result.stylesheet = gIoService.newURI(hrefAttribute[1], null,
                                                    this._result.uri);
    }
  },

  // end of nsISAXContentHandler

  // Handle our more complicated elements--those that contain
  // attributes and child elements.
  _processComplexElement:
  function FP__processComplexElement(elementInfo, attributes) {
    var obj, key, prefix;

    // If the container is an entry/item, it'll need to have its 
    // more esoteric properties put in the 'fields' property bag.
    if (elementInfo.containerClass == Cc[ENTRY_CONTRACTID]) {
      obj = elementInfo.containerClass.createInstance(Ci.nsIFeedEntry);
      obj.baseURI = this._xmlBaseStack[this._xmlBaseStack.length - 1];
      this._mapAttributes(obj.fields, attributes);
    }
    else if (elementInfo.containerClass) {
      obj = elementInfo.containerClass.createInstance(Ci.nsIFeedElementBase);
      obj.baseURI = this._xmlBaseStack[this._xmlBaseStack.length - 1];
      obj.attributes = attributes; // just set the SAX attributes
    }
    else {
      obj = Cc[BAG_CONTRACTID].createInstance(Ci.nsIWritablePropertyBag2);
      this._mapAttributes(obj, attributes);
    }

    // We should have a container/propertyBag that's had its
    // attributes processed. Now we need to attach it to its
    // container.
    var newProp;

    // First we'll see what's on top of the stack.
    var container = this._stack[this._stack.length - 1][0];

    // Check to see if it has the property
    var prop;
    try {
      prop = container.getProperty(elementInfo.fieldName);
    }
    catch(e) {
    }
    
    if (elementInfo.isArray) {
      if (!prop) {
        container.setPropertyAsInterface(elementInfo.fieldName,
                                         Cc[ARRAY_CONTRACTID].
                                         createInstance(Ci.nsIMutableArray));
      }

      newProp = container.getProperty(elementInfo.fieldName);
      // XXX This QI should not be necessary, but XPConnect seems to fly
      // off the handle in the browser, and loses track of the interface
      // on large files. Bug 335638.
      newProp.QueryInterface(Ci.nsIMutableArray);
      newProp.appendElement(obj,false);
      
      // If new object is an nsIFeedContainer, we want to deal with
      // its member nsIPropertyBag instead.
      if (isIFeedContainer(obj))
        newProp = obj.fields; 

    }
    else {
      // If it doesn't, set it.
      if (!prop) {
        container.setPropertyAsInterface(elementInfo.fieldName,obj);
      }
      newProp = container.getProperty(elementInfo.fieldName);
    }
    
    // make our new state name, and push the property onto the stack
    var newState = "IN_" + elementInfo.fieldName.toUpperCase();
    this._stack.push([newProp, newState, obj]);
    return newState;
  },

  // Sometimes we need reconcile the element content with the object
  // model for a given feed. We use helper functions to do the
  // munging, but we need to identify array types here, so the munging
  // happens only to the last element of an array.
  _closeComplexElement: function FP__closeComplexElement(elementInfo) {
    var stateTuple = this._stack.pop();
    var container = stateTuple[0];
    var containerParent = stateTuple[2];
    var element = null;
    var isArray = isIArray(container);

    // If it's an array and we have to post-process,
    // grab the last element
    if (isArray)
      element = container.queryElementAt(container.length - 1, Ci.nsISupports);
    else
      element = container;

    // Run the post-processing function if there is one.
    if (elementInfo.closeFunc)
      element = elementInfo.closeFunc(this._buf, element);

    // If an nsIFeedContainer was on top of the stack,
    // we need to normalize it
    if (elementInfo.containerClass == Cc[ENTRY_CONTRACTID])
      containerParent.normalize();

    // If it's an array, re-set the last element
    if (isArray)
      container.replaceElementAt(element, container.length - 1, false);
  },
  
  _prefixForNS: function FP_prefixForNS(uri) {
    if (!uri)
      return "";
    var prefix = gNamespaces[uri];
    if (prefix)
      return prefix + ":";
    if (uri.toLowerCase().indexOf("http://backend.userland.com") == 0)
      return "";
    else
      return null;
  },

  _mapAttributes: function FP__mapAttributes(bag, attributes) {
    // Cycle through the attributes, and set our properties using the
    // prefix:localNames we find in our namespace dictionary.
    for (var i = 0; i < attributes.length; ++i) {
      var key = this._prefixForNS(attributes.getURI(i)) + attributes.getLocalName(i);
      var val = attributes.getValue(i);
      bag.setPropertyAsAString(key, val);
    }
  },

  // Only for RSS2esque formats
  _findRSSVersion: function FP__findRSSVersion(attributes) {
    var versionAttr = trimString(attributes.getValueFromName("", "version"));
    var versions = { "0.91":"rss091",
                     "0.92":"rss092",
                     "0.93":"rss093",
                     "0.94":"rss094" }
    if (versions[versionAttr])
      return versions[versionAttr];
    if (versionAttr.substr(0,2) != "2.")
      return "rssUnknown";
    return "rss2";
  },

  // unknown element values are returned here. See startElement above
  // for how this works.
  returnFromExtHandler:
  function FP_returnExt(uri, localName, chars, attributes) {
    --this._depth;

    // take control of the SAX events
    this._reader.contentHandler = this;
    if (localName == null && chars == null)
      return;

    // we don't take random elements inside rdf:RDF
    if (this._state == "IN_RDF")
      return;
    
    // Grab the top of the stack
    var top = this._stack[this._stack.length - 1];
    if (!top) 
      return;

    var container = top[0];
    // Grab the last element if it's an array
    if (isIArray(container)) {
      var contract = this._handlerStack[this._depth].containerClass;
      // check if it's something specific, but not an entry
      if (contract && contract != Cc[ENTRY_CONTRACTID]) {
        var el = container.queryElementAt(container.length - 1, 
                                          Ci.nsIFeedElementBase);
        // XXX there must be a way to flatten these interfaces
        if (contract == Cc[PERSON_CONTRACTID]) 
          el.QueryInterface(Ci.nsIFeedPerson);
        else
          return; // don't know about this interface

        var propName = localName;
        var prefix = gNamespaces[uri];

        // synonyms
        if ((uri == "" || 
             prefix &&
             ((prefix.indexOf("atom") > -1) ||
              (prefix.indexOf("rss") > -1))) && 
            (propName == "url" || propName == "href"))
          propName = "uri";
        
        try {
          if (el[propName] !== "undefined") {
            var propValue = chars;
            // convert URI-bearing values to an nsIURI
            if (propName == "uri") {
              var base = this._xmlBaseStack[this._xmlBaseStack.length - 1];
              propValue = strToURI(chars, base);
            }
            el[propName] = propValue;
          }
        }
        catch(e) {
          // ignore XPConnect errors
        }
        // the rest of the function deals with entry- and feed-level stuff
        return; 
      } 
      else {
        container = container.queryElementAt(container.length - 1, 
                                             Ci.nsIWritablePropertyBag2);
      }
    }
    
    // Make the buffer our new property
    var propName = this._prefixForNS(uri) + localName;

    // But, it could be something containing HTML. If so,
    // we need to know about that.
    if (this._textConstructs[propName] != null &&
        this._handlerStack[this._depth].containerClass !== null) {
      var newProp = Cc[TEXTCONSTRUCT_CONTRACTID].
                    createInstance(Ci.nsIFeedTextConstruct);
      newProp.text = chars;
      // Look up the default type in our table
      var type = this._textConstructs[propName];
      var typeAttribute = attributes.getValueFromName("","type");
      if (this._result.version == "atom" && typeAttribute != null) {
        type = typeAttribute;
      }
      else if (this._result.version == "atom03" && typeAttribute != null) {
        if (typeAttribute.toLowerCase().indexOf("xhtml") >= 0) {
          type = "xhtml";
        }
        else if (typeAttribute.toLowerCase().indexOf("html") >= 0) {
          type = "html";
        }
        else if (typeAttribute.toLowerCase().indexOf("text") >= 0) {
          type = "text";
        }
      }
      
      // If it's rss feed-level description, it's not supposed to have html
      if (this._result.version.indexOf("rss") >= 0 &&
          this._handlerStack[this._depth].containerClass != ENTRY_CONTRACTID) {
        type = "text";
      }
      newProp.type = type;
      newProp.base = this._xmlBaseStack[this._xmlBaseStack.length - 1];
      container.setPropertyAsInterface(propName, newProp);
    }
    else {
      container.setPropertyAsAString(propName, chars);
    }
  },

  // Sometimes, we'll hand off SAX handling duties to an XHTMLHandler
  // (see above) that will scrape out non-XHTML stuff, normalize
  // namespaces, and remove the wrapper div from Atom 1.0. When the
  // XHTMLHandler is done, it'll callback here.
  returnFromXHTMLHandler:
  function FP_returnFromXHTMLHandler(chars, uri, localName, qName) {
    // retake control of the SAX content events
    this._reader.contentHandler = this;

    // Grab the top of the stack
    var top = this._stack[this._stack.length - 1];
    if (!top) 
      return;
    var container = top[0];

    // Assign the property
    var newProp =  newProp = Cc[TEXTCONSTRUCT_CONTRACTID].
                   createInstance(Ci.nsIFeedTextConstruct);
    newProp.text = chars;
    newProp.type = "xhtml";
    newProp.base = this._xmlBaseStack[this._xmlBaseStack.length - 1];
    container.setPropertyAsInterface(this._prefixForNS(uri) + localName,
                                     newProp);
    
    // XHTML will cause us to peek too far. The XHTML handler will
    // send us an end element to call. RFC4287-valid feeds allow a
    // more graceful way to handle this. Unfortunately, we can't count
    // on compliance at this point.
    this.endElement(uri, localName, qName);
  },

  // nsISupports
  QueryInterface: function FP_QueryInterface(iid) {
    if (iid.equals(Ci.nsIFeedProcessor) ||
        iid.equals(Ci.nsISAXContentHandler) ||
        iid.equals(Ci.nsISAXErrorHandler) ||
        iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsIRequestObserver) ||
        iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NOINTERFACE;
  },
}

const FP_CONTRACTID = "@mozilla.org/feed-processor;1";
const FP_CLASSID = Components.ID("{26acb1f0-28fc-43bc-867a-a46aabc85dd4}");
const FP_CLASSNAME = "Feed Processor";
const FR_CONTRACTID = "@mozilla.org/feed-result;1";
const FR_CLASSID = Components.ID("{072a5c3d-30c6-4f07-b87f-9f63d51403f2}");
const FR_CLASSNAME = "Feed Result";
const FEED_CONTRACTID = "@mozilla.org/feed;1";
const FEED_CLASSID = Components.ID("{5d0cfa97-69dd-4e5e-ac84-f253162e8f9a}");
const FEED_CLASSNAME = "Feed";
const ENTRY_CONTRACTID = "@mozilla.org/feed-entry;1";
const ENTRY_CLASSID = Components.ID("{8e4444ff-8e99-4bdd-aa7f-fb3c1c77319f}");
const ENTRY_CLASSNAME = "Feed Entry";
const TEXTCONSTRUCT_CONTRACTID = "@mozilla.org/feed-textconstruct;1";
const TEXTCONSTRUCT_CLASSID =
  Components.ID("{b992ddcd-3899-4320-9909-924b3e72c922}");
const TEXTCONSTRUCT_CLASSNAME = "Feed Text Construct";
const GENERATOR_CONTRACTID = "@mozilla.org/feed-generator;1";
const GENERATOR_CLASSID =
  Components.ID("{414af362-9ad8-4296-898e-62247f25a20e}");
const GENERATOR_CLASSNAME = "Feed Generator";
const PERSON_CONTRACTID = "@mozilla.org/feed-person;1";
const PERSON_CLASSID = Components.ID("{95c963b7-20b2-11db-92f6-001422106990}");
const PERSON_CLASSNAME = "Feed Person";

function GenericComponentFactory(ctor) {
  this._ctor = ctor;
}

GenericComponentFactory.prototype = {

  _ctor: null,

  // nsIFactory
  createInstance: function(outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return (new this._ctor()).QueryInterface(iid);
  },

  // nsISupports
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIFactory) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

};

var Module = {
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIModule) || 
        iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  getClassObject: function(cm, cid, iid) {
    if (!iid.equals(Ci.nsIFactory))
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;

    if (cid.equals(FP_CLASSID))
      return new GenericComponentFactory(FeedProcessor);
    if (cid.equals(FR_CLASSID))
      return new GenericComponentFactory(FeedResult);
    if (cid.equals(FEED_CLASSID))
      return new GenericComponentFactory(Feed);
    if (cid.equals(ENTRY_CLASSID))
      return new GenericComponentFactory(Entry);
    if (cid.equals(TEXTCONSTRUCT_CLASSID))
      return new GenericComponentFactory(TextConstruct);
    if (cid.equals(GENERATOR_CLASSID))
      return new GenericComponentFactory(Generator);
    if (cid.equals(PERSON_CLASSID))
      return new GenericComponentFactory(Person);

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  registerSelf: function(cm, file, location, type) {
    var cr = cm.QueryInterface(Ci.nsIComponentRegistrar);
    // Feed Processor
    cr.registerFactoryLocation(FP_CLASSID, FP_CLASSNAME,
      FP_CONTRACTID, file, location, type);
    // Feed Result
    cr.registerFactoryLocation(FR_CLASSID, FR_CLASSNAME,
      FR_CONTRACTID, file, location, type);
    // Feed
    cr.registerFactoryLocation(FEED_CLASSID, FEED_CLASSNAME,
      FEED_CONTRACTID, file, location, type);
    // Entry
    cr.registerFactoryLocation(ENTRY_CLASSID, ENTRY_CLASSNAME,
      ENTRY_CONTRACTID, file, location, type);
    // Text Construct
    cr.registerFactoryLocation(TEXTCONSTRUCT_CLASSID, TEXTCONSTRUCT_CLASSNAME,
      TEXTCONSTRUCT_CONTRACTID, file, location, type);
    // Generator
    cr.registerFactoryLocation(GENERATOR_CLASSID, GENERATOR_CLASSNAME,
      GENERATOR_CONTRACTID, file, location, type);
    // Person
    cr.registerFactoryLocation(PERSON_CLASSID, PERSON_CLASSNAME,
      PERSON_CONTRACTID, file, location, type);
  },

  unregisterSelf: function(cm, location, type) {
    var cr = cm.QueryInterface(Ci.nsIComponentRegistrar);
    // Feed Processor
    cr.unregisterFactoryLocation(FP_CLASSID, location);
    // Feed Result
    cr.unregisterFactoryLocation(FR_CLASSID, location);
    // Feed
    cr.unregisterFactoryLocation(FEED_CLASSID, location);
    // Entry
    cr.unregisterFactoryLocation(ENTRY_CLASSID, location);
    // Text Construct
    cr.unregisterFactoryLocation(TEXTCONSTRUCT_CLASSID, location);
    // Generator
    cr.unregisterFactoryLocation(GENERATOR_CLASSID, location);
    // Person
    cr.unregisterFactoryLocation(PERSON_CLASSID, location);
  },

  canUnload: function(cm) {
    return true;
  },
};

function NSGetModule(cm, file) {
  return Module;
}
