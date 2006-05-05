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

var gIoService = Cc[IO_CONTRACTID].getService(Ci.nsIIOService);

/***** Some general utils *****/
function strToURI(link, base) {
  var base = base || null;
  return gIoService.newURI(link, null, base);
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

// namespace map
var gNamespaces = {
  "http://www.w3.org/2005/Atom":"atom",
  "http://purl.org/rss/1.0/modules/content/":"content",
  "http://purl.org/dc/elements/1.1/":"dc",
  "http://www.w3.org/1999/02/22-rdf-syntax-ns#":"rdf",
  "http://purl.org/rss/1.0/":"rss1",
  "http://wellformedweb.org/CommentAPI/":"wfw",                              
  "http://purl.org/rss/1.0/modules/wiki/":"wiki" 
}

// lets us know to ignore extraneous attributes like 
// <id foo:bar="baz">http://example.org</id>
var gKnownTextElements = ["title","link","description","language","copyright",
                          "managingEditor","webMaster","pubDate",
                          "lastBuildDate","docs","ttl","rating",
                          "rss1:title","rss1:link","rss1:description",
                          "rss1:url","rss1:name","dc:creator", "dc:subject",
                          "dc:description", "dc:publisher", "dc:contributor",
                          "dc:date", "dc:type", "dc:format", "dc:identifier",
                          "dc:source","dc:language","dc:relation",
                          "dc:coverage","dc:rights", "atom:id", "atom:name",
                          "atom:uri", "atom:content", "atom:email",
                          "atom:logo", "atom:published", "atom:updated", 
                          "wfw:comment", "wfw:commentRss", "wiki:version", 
                          "wiki:status", "wiki:importance","wiki:diff", 
                          "wiki:history","content:encoded",  "atom:icon"];

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
  this._sub = null;
  this.items = [];
}
Feed.prototype = {
  subtitle: function Feed_subtitle(doStripTags) {
    return doStripTags ? stripTags(this._sub) : this._sub;
  },

  searchLists: {
    _sub: ["description","dc:description","rss1:description","atom:subtitle"],
    items: ["items","entries"],
    title: ["title","rss1:title","atom:title"],
    link:  [["link",strToURI],["rss1:link",strToURI]],
    categories: ["categories", "dc:subject"],
    cloud: ["cloud"],
    image: ["image", "rss1:image"],
    textInput: ["textInput", "rss1:textinput"],
    skipDays: ["skipDays"],
    skipHours: ["skipHours"]
  },

  normalize: function Feed_normalize() {
    fieldsToObj(this, this.searchLists);
    if (this.skipDays) {
      this.skipDays = this.skipDays.getProperty("days");
    }
    if (this.skipHours) {
      this.skipHours = this.skipHours.getProperty("hours");
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
  this._summary = null;
  this._content = null;
  this.fields = Cc["@mozilla.org/hash-property-bag;1"].
                createInstance(Ci.nsIWritablePropertyBag2);
}

Entry.prototype = {
  fields: null,
  summary: function Entry_summary(doStripTags) {
    if (this._summary == null)
      return null;
    return doStripTags ? stripTags(this._summary) : this._summary;
  },
  content: function Entry_content(doStripTags) {
    if (this._content == null)
      return null;
    return doStripTags ? stripTags(this._content) : this._content;
  },
  enclosures: null,
  mediaContent: null,

  searchLists: {
    title: ["title","rss1:title","atom:title"],
    link: [["link",strToURI],["rss1:link",strToURI]],
    _summary: ["description", "rss1:description",
               "dc:description", "atom:summary"],
    _content: ["content:encoded", "atom:content"],
  },

  normalize: function Feed_normalize() {
    fieldsToObj(this, this.searchLists);
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIFeedEntry) ||
        iid.equals(Ci.nsIFeedContainer) ||
        iid.equals(Ci.nsISupports))
    return this;
    
    throw Cr.NS_ERROR_NOINTERFACE;
  }
}

// TextConstruct represents and element that could contain (X)HTML
function TextConstruct() {
  this.lang = null;
  this.base = null;
  this.type = "text";
  this.text = "";
}

TextConstruct.prototype = {
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIFeedTextConstruct) ||
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
//
// Handle atom:generator
function atomGenerator(s, generator) {
  generator.setPropertyAsAString("agent", trimString(s));
  return generator;
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
// We want to split this up and assign it to corresponding Atom
// fields.
//
function rssAuthor(s,author) {
  author.QueryInterface(Ci.nsIWritablePropertyBag2);
  var open = s.indexOf("(");
  var close = s.indexOf(")");
  var email = trimString(s.substring(0,open)) || null;
  author.setPropertyAsAString("email", email);
  var name = null; 
  if (open >= 0 && close > open) 
    name = trimString(s.substring(open+1,close));
  author.setPropertyAsAString("name", name);
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
const RFC822_RE = "^(((Mon)|(Tue)|(Wed)|(Thu)|(Fri)|(Sat)|(Sun)), *)?\\d\\d?"
+ " +((Jan)|(Feb)|(Mar)|(Apr)|(May)|(Jun)|(Jul)|(Aug)|(Sep)|(Oct)|(Nov)|(Dec))"
+ " +\\d\\d(\\d\\d)? +\\d\\d:\\d\\d(:\\d\\d)? +(([+-]?\\d\\d\\d\\d)|(UT)|(GMT)"
+ "|(EST)|(EDT)|(CST)|(CDT)|(MST)|(MDT)|(PST)|(PDT)|\\w)$";

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
function XHTMLHandler(processor, isAtom) {
  this._buf = "";
  this._processor = processor;
  this._depth = 0;
  this._isAtom = isAtom;
}

// The fidelity can be improved here, to allow handling of stuff like
// SVG and MathML. XXX
XHTMLHandler.prototype = {
  startDocument: function XH_startDocument() {
  },
  endDocument: function XH_endDocument() {
  },
  startElement: function XH_startElement(uri, localName, qName, attributes) {
    ++this._depth;

    // RFC4287 requires XHTML to be wrapped in a div that is *not* part of 
    // the content. This prevents people from screwing up namespaces, but
    // we need to skip it here.
    if (this._isAtom && this._depth == 1 && localName == "div")
      return;

    // If it's an XHTML element, record it. Otherwise, it's ignored.
    if (uri == XHTML_NS) {
      this._buf += "<" + localName;
      for (var i=0; i < attributes.length; ++i) {
        // XHTML attributes aren't in a namespace
        if (attributes.getURI(i) == "") { 
          this._buf += (" " + attributes.getLocalName(i) + "='" +
                        xmlEscape(attributes.getValue(i)) + "'");
        }
      }
      this._buf += ">";
    }
  },
  endElement: function XH_endElement(uri, localName, qName) {
    --this._depth;
    
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
  startPrefixMapping: function XH_startPrefixMapping() {
  },
  endPrefixMapping: function XH_endPrefixMapping() {
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

  // Tracks whether the content model is something understandable
  this._isSimple = true;

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
    if (attrs.length > 0 && !arrayContains(gKnownTextElements, key)) 
      this._isSimple = false;
    if (this._depth == 1) {
      this._uri = uri;
      this._localName = localName;
      this._qName = qName;
      this._attrs = attrs;
    }
    else {
      this._isSimple = false;
    }
  },
  endElement: function EH_endElement(uri, localName, qName) {
    --this._depth;
    if (this._depth == 0) {
      if (this._isSimple) {
        this._processor.returnFromExtHandler(this._uri, this._localName, 
                                             trimString(this._buf));
      }
      else {
        this._processor.returnFromExtHandler(null,null,null);
      }
    }
  },
  characters: function EH_characters(data) {
    if (this._isSimple)
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
  this._depth = 0;
  this._state = "START";
  this._result = null;
  this._extensionHandler = null;
  this._xhtmlHandler = null;

  // The nsIFeedResultListener waiting for the parse results
  this.listener = null;

  // These elements can contain (X)HTML or plain text.
  this._textConstructs = ["atom:title", "atom:summary", "atom:rights",
                          "atom:content", "atom:subtitle"];
  this._stack = [];

  this._trans = {   
    "START": {
      //If we hit a root RSS element, treat as RSS2.
      "rss": new FeedElementInfo("RSS2", "rss2"),

      // If we hit an RDF element, if could be RSS1, but we can't
      // verify that until we hit a rss1:channel element.
      "rdf:RDF": new WrapperElementInfo("RDF"),

      //If we hit a Atom 1.0 element, treat as Atom 1.0.
      "atom:feed": new FeedElementInfo("Atom", "atom"),
    },
    
    /********* RSS2 **********/
    "IN_RSS2": {
      "channel": new WrapperElementInfo("channel")
    },

    "IN_CHANNEL": {
      "item": new ElementInfo("items", Cc[ENTRY_CONTRACTID], null, true),
      "category": new ElementInfo("categories", null, rssCatTerm, true),
      "cloud": new ElementInfo("cloud", null, null, false),
      "image": new ElementInfo("image", null, null, false),
      "textInput": new ElementInfo("textInput", null, null, false),
      "skipDays": new ElementInfo("skipDays", null, null, false),
      "skipHours": new ElementInfo("skipHours", null, null, false)
    },

    "IN_ITEMS": {
      "author": new ElementInfo("authors", null, rssAuthor, true),
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
      "rss1:item": new ElementInfo("items", Cc[ENTRY_CONTRACTID], null, true)
    },

    /********* ATOM 1.0 **********/
    "IN_ATOM": {
      "atom:author": new ElementInfo("author", null, null, true),
      "atom:generator": new ElementInfo("generator", null,
                                        atomGenerator, false),
      "atom:contributor": new ElementInfo("contributor", null, null, true),
      "atom:link": new ElementInfo("links", null, null, true),
      "atom:entry": new ElementInfo("entries", Cc[ENTRY_CONTRACTID],
                                    null, true)
    },

    "IN_ENTRIES": {
      "atom:author": new ElementInfo("author", null, null, true),
      "atom:contributor": new ElementInfo("contributor", null, null, true),
      "atom:link": new ElementInfo("links", null, null, true),
    },
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
    }
  },

  // This function is called once we figure out what type of feed
  // we're dealing with. Some feed types require digging a bit further
  // than the root.
  _docVerified: function FP_docVerified(version) {
    this._result.doc = Cc[FEED_CONTRACTID].createInstance(Ci.nsIFeed);
    this._result.doc.fields = this._feed;
    this._result.version = version;
  },

  // When we're done with the feed, let the listener know what
  // happened.
  _sendResult: function FP_sendResult() {
    try {
      this._result.doc.normalize();
    }
    catch (e) {
      LOG("FIXME: " + e);
    }
    if (this.listener != null) { 
      this.listener.handleResult(this._result);
    }
  },

  // Parsing functions
  parseFromStream: function FP_parseFromStream(stream, uri) {
    this._init(uri);
    this._reader.parseFromStream(stream, null, stream.available(), 
                                 "application/xml");
  },

  parseFromString: function FP_parseFromString(inputString, uri) {
    this._init(uri);
    this._reader.parseFromString(inputString,"application/xml");
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
    this._reader.onStopRequest(request, context, statusCode);
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

    // To identify the element we're dealing with, we look up the
    // namespace URI in our gNamespaces dictionary, which will give us
    // a "canonical" prefix for a namespace URI. For example, this
    // allows Dublin Core "creator" elements to be consistently mapped
    // to "dc:creator", for easy field access by consumer code. This
    // strategy also happens to shorten up our state table.
    var prefix = gNamespaces[uri] ? gNamespaces[uri] + ":" : "";
    var key =  prefix + localName;

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
    if (this._result.version == "atom" && 
        arrayContains(this._textConstructs, key)) {
      var type = attributes.getValue("","type");
      if (type == "xhtml") {
        this._xhtmlHandler = 
          new XHTMLHandler(this, (this._result.version == "atom"));
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
      this._docVerified(elementInfo.feedVersion);
      this._stack.push([this._feed, this._state]);
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
    var prefix = gNamespaces[uri] ? gNamespaces[uri] + ":" : "";
    var elementInfo = this._handlerStack[this._depth];

    if (elementInfo && !elementInfo.isWrapper)
      this._closeComplexElement(elementInfo);
  
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
  startPrefixMapping: function FP_startPrefixMapping() {
  },
  endPrefixMapping: function FP_endPrefixMapping() {
  },
  processingInstruction: function FP_processingInstruction(target, data) {
    if (target == "xml-stylesheet") {
      var hrefAttribute = data.match(/href=\"(.*?)\"/);
      if (hrefAttribute.length == 2) 
        this._result.stylesheet = gIoService.newURI(hrefAttribute[1], null,
                                                    this._result.uri);
    }
  },

  // end of nsISAXContentHandler

  // Handle our more complicated elements--those that contain
  // attributes and child elements.
  _processComplexElement:
  function FP__processComplexElement(elementInfo, attributes) {
    var obj, props, key, prefix;

    // If the container is a feed or entry, it'll need to have its 
    // more esoteric properties put in the 'fields' property bag, and set its
    // parent.
    if (elementInfo.containerClass) {
      obj = elementInfo.containerClass.createInstance(Ci.nsIFeedEntry);
      // Set the parent property of the entry.
      obj.parent = this._result.doc;
      props = obj.fields;
    }
    else {
      obj = Cc[BAG_CONTRACTID].createInstance(Ci.nsIWritablePropertyBag2);
      props = obj;
    }

    // Cycle through the attributes, and set our properties using the
    // prefix:localNames we find in our namespace dictionary.
    for (var i = 0; i < attributes.length; ++i) {
      prefix = gNamespaces[attributes.getURI(i)] ? 
        gNamespaces[attributes.getURI(i)] + ":" : "";
      key = prefix + attributes.getLocalName(i);
      var val = attributes.getValue(i);
      props.setPropertyAsAString(key, val);
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
    if (elementInfo.containerClass)
      containerParent.normalize();

    // If it's an array, re-set the last element
    if (isArray)
      container.replaceElementAt(element, container.length - 1, false);
  },
  
  // unknown element values are returned here. See startElement above
  // for how this works.
  returnFromExtHandler: function FP_returnExt(uri, localName, chars) {
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
      container = container.queryElementAt(container.length - 1, 
                                           Ci.nsIWritablePropertyBag2);
    }

    // Assign the property
    var prefix = gNamespaces[uri] ? gNamespaces[uri] + ":" : "";
    container.setPropertyAsAString(prefix+localName, chars);
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
    var prefix = gNamespaces[uri] ? gNamespaces[uri] + ":" : "";
    container.setPropertyAsAString(prefix + localName, chars);
    
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
    cr.unregisterFactoryLocation(TEXTCONSTUCT_CLASSID, location);
  },

  canUnload: function(cm) {
    return true;
  },
};

function NSGetModule(cm, file) {
  return Module;
}
