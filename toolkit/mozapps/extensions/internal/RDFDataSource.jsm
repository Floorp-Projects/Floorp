 /* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module creates a new API for accessing and modifying RDF graphs. The
 * goal is to be able to serialise the graph in a human readable form. Also
 * if the graph was originally loaded from an RDF/XML the serialisation should
 * closely match the original with any new data closely following the existing
 * layout. The output should always be compatible with Mozilla's RDF parser.
 *
 * This is all achieved by using a DOM Document to hold the current state of the
 * graph in XML form. This can be initially loaded and parsed from disk or
 * a blank document used for an empty graph. As assertions are added to the
 * graph, appropriate DOM nodes are added to the document to represent them
 * along with any necessary whitespace to properly layout the XML.
 *
 * In general the order of adding assertions to the graph will impact the form
 * the serialisation takes. If a resource is first added as the object of an
 * assertion then it will eventually be serialised inside the assertion's
 * property element. If a resource is first added as the subject of an assertion
 * then it will be serialised at the top level of the XML.
 */

const NS_XML = "http://www.w3.org/XML/1998/namespace";
const NS_XMLNS = "http://www.w3.org/2000/xmlns/";
const NS_RDF = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const NS_NC = "http://home.netscape.com/NC-rdf#";

/* eslint prefer-template: 1 */

function raw(strings) {
  return strings.raw[0].replace(/\s+/, "");
}

// Copied from http://www.w3.org/TR/2000/REC-xml-20001006#CharClasses
const XML_LETTER = raw`
  \u0041-\u005A\u0061-\u007A\u00C0-\u00D6\u00D8-\u00F6
  \u00F8-\u00FF\u0100-\u0131\u0134-\u013E\u0141-\u0148
  \u014A-\u017E\u0180-\u01C3\u01CD-\u01F0\u01F4-\u01F5
  \u01FA-\u0217\u0250-\u02A8\u02BB-\u02C1\u0386\u0388-\u038A
  \u038C\u038E-\u03A1\u03A3-\u03CE\u03D0-\u03D6\u03DA\u03DC
  \u03DE\u03E0\u03E2-\u03F3\u0401-\u040C\u040E-\u044F
  \u0451-\u045C\u045E-\u0481\u0490-\u04C4\u04C7-\u04C8
  \u04CB-\u04CC\u04D0-\u04EB\u04EE-\u04F5\u04F8-\u04F9
  \u0531-\u0556\u0559\u0561-\u0586\u05D0-\u05EA\u05F0-\u05F2
  \u0621-\u063A\u0641-\u064A\u0671-\u06B7\u06BA-\u06BE
  \u06C0-\u06CE\u06D0-\u06D3\u06D5\u06E5-\u06E6\u0905-\u0939
  \u093D\u0958-\u0961\u0985-\u098C\u098F-\u0990\u0993-\u09A8
  \u09AA-\u09B0\u09B2\u09B6-\u09B9\u09DC-\u09DD\u09DF-\u09E1
  \u09F0-\u09F1\u0A05-\u0A0A\u0A0F-\u0A10\u0A13-\u0A28
  \u0A2A-\u0A30\u0A32-\u0A33\u0A35-\u0A36\u0A38-\u0A39
  \u0A59-\u0A5C\u0A5E\u0A72-\u0A74\u0A85-\u0A8B\u0A8D
  \u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2-\u0AB3
  \u0AB5-\u0AB9\u0ABD\u0AE0\u0B05-\u0B0C\u0B0F-\u0B10
  \u0B13-\u0B28\u0B2A-\u0B30\u0B32-\u0B33\u0B36-\u0B39
  \u0B3D\u0B5C-\u0B5D\u0B5F-\u0B61\u0B85-\u0B8A\u0B8E-\u0B90
  \u0B92-\u0B95\u0B99-\u0B9A\u0B9C\u0B9E-\u0B9F\u0BA3-\u0BA4
  \u0BA8-\u0BAA\u0BAE-\u0BB5\u0BB7-\u0BB9\u0C05-\u0C0C
  \u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C33\u0C35-\u0C39
  \u0C60-\u0C61\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8
  \u0CAA-\u0CB3\u0CB5-\u0CB9\u0CDE\u0CE0-\u0CE1\u0D05-\u0D0C
  \u0D0E-\u0D10\u0D12-\u0D28\u0D2A-\u0D39\u0D60-\u0D61
  \u0E01-\u0E2E\u0E30\u0E32-\u0E33\u0E40-\u0E45\u0E81-\u0E82
  \u0E84\u0E87-\u0E88\u0E8A\u0E8D\u0E94-\u0E97\u0E99-\u0E9F
  \u0EA1-\u0EA3\u0EA5\u0EA7\u0EAA-\u0EAB\u0EAD-\u0EAE\u0EB0
  \u0EB2-\u0EB3\u0EBD\u0EC0-\u0EC4\u0F40-\u0F47\u0F49-\u0F69
  \u10A0-\u10C5\u10D0-\u10F6\u1100\u1102-\u1103\u1105-\u1107
  \u1109\u110B-\u110C\u110E-\u1112\u113C\u113E\u1140\u114C
  \u114E\u1150\u1154-\u1155\u1159\u115F-\u1161\u1163\u1165
  \u1167\u1169\u116D-\u116E\u1172-\u1173\u1175\u119E\u11A8
  \u11AB\u11AE-\u11AF\u11B7-\u11B8\u11BA\u11BC-\u11C2\u11EB
  \u11F0\u11F9\u1E00-\u1E9B\u1EA0-\u1EF9\u1F00-\u1F15
  \u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57
  \u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC
  \u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB
  \u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u2126\u212A-\u212B
  \u212E\u2180-\u2182\u3041-\u3094\u30A1-\u30FA\u3105-\u312C
  \uAC00-\uD7A3\u4E00-\u9FA5\u3007\u3021-\u3029
`;
const XML_DIGIT = raw`
  \u0030-\u0039\u0660-\u0669\u06F0-\u06F9\u0966-\u096F
  \u09E6-\u09EF\u0A66-\u0A6F\u0AE6-\u0AEF\u0B66-\u0B6F
  \u0BE7-\u0BEF\u0C66-\u0C6F\u0CE6-\u0CEF\u0D66-\u0D6F
  \u0E50-\u0E59\u0ED0-\u0ED9\u0F20-\u0F29
`;
const XML_COMBINING = raw`
  \u0300-\u0345\u0360-\u0361\u0483-\u0486\u0591-\u05A1
  \u05A3-\u05B9\u05BB-\u05BD\u05BF\u05C1-\u05C2\u05C4
  \u064B-\u0652\u0670\u06D6-\u06DC\u06DD-\u06DF\u06E0-\u06E4
  \u06E7-\u06E8\u06EA-\u06ED\u0901-\u0903\u093C\u093E-\u094C
  \u094D\u0951-\u0954\u0962-\u0963\u0981-\u0983\u09BC\u09BE
  \u09BF\u09C0-\u09C4\u09C7-\u09C8\u09CB-\u09CD\u09D7
  \u09E2-\u09E3\u0A02\u0A3C\u0A3E\u0A3F\u0A40-\u0A42
  \u0A47-\u0A48\u0A4B-\u0A4D\u0A70-\u0A71\u0A81-\u0A83
  \u0ABC\u0ABE-\u0AC5\u0AC7-\u0AC9\u0ACB-\u0ACD\u0B01-\u0B03
  \u0B3C\u0B3E-\u0B43\u0B47-\u0B48\u0B4B-\u0B4D\u0B56-\u0B57
  \u0B82-\u0B83\u0BBE-\u0BC2\u0BC6-\u0BC8\u0BCA-\u0BCD\u0BD7
  \u0C01-\u0C03\u0C3E-\u0C44\u0C46-\u0C48\u0C4A-\u0C4D
  \u0C55-\u0C56\u0C82-\u0C83\u0CBE-\u0CC4\u0CC6-\u0CC8
  \u0CCA-\u0CCD\u0CD5-\u0CD6\u0D02-\u0D03\u0D3E-\u0D43
  \u0D46-\u0D48\u0D4A-\u0D4D\u0D57\u0E31\u0E34-\u0E3A
  \u0E47-\u0E4E\u0EB1\u0EB4-\u0EB9\u0EBB-\u0EBC\u0EC8-\u0ECD
  \u0F18-\u0F19\u0F35\u0F37\u0F39\u0F3E\u0F3F\u0F71-\u0F84
  \u0F86-\u0F8B\u0F90-\u0F95\u0F97\u0F99-\u0FAD\u0FB1-\u0FB7
  \u0FB9\u20D0-\u20DC\u20E1\u302A-\u302F\u3099\u309A
`;
const XML_EXTENDER = raw`
  \u00B7\u02D0\u02D1\u0387\u0640\u0E46\u0EC6\u3005
  \u3031-\u3035\u309D-\u309E\u30FC-\u30FE
`;
const XML_NCNAMECHAR = String.raw`${XML_LETTER}${XML_DIGIT}\.\-_${XML_COMBINING}${XML_EXTENDER}`;
const XML_NCNAME = new RegExp(`^[${XML_LETTER}_][${XML_NCNAMECHAR}]*$`);

const URI_SUFFIX = /[A-Za-z_][0-9A-Za-z\.\-_]*$/;
const INDENT = /\n([ \t]*)$/;
const RDF_LISTITEM = /^http:\/\/www.w3.org\/1999\/02\/22-rdf-syntax-ns#_\d+$/;

const RDF_NODE_INVALID_TYPES =
        ["RDF", "ID", "about", "bagID", "parseType", "resource", "nodeID",
         "li", "aboutEach", "aboutEachPrefix"];
const RDF_PROPERTY_INVALID_TYPES =
        ["Description", "RDF", "ID", "about", "bagID", "parseType", "resource",
         "nodeID", "aboutEach", "aboutEachPrefix"];

/**
 * Whether to use properly namespaces attributes for rdf:about etc...
 * When on this produces poor output in the event that the rdf namespace is the
 * default namespace, and the parser recognises unnamespaced attributes and
 * most of our rdf examples are unnamespaced so leaving off for the time being.
 */
const USE_RDFNS_ATTR = false;

var EXPORTED_SYMBOLS = ["RDFLiteral", "RDFIntLiteral", "RDFDateLiteral",
                        "RDFBlankNode", "RDFResource", "RDFDataSource"];

Cu.importGlobalProperties(["DOMParser", "Element", "XMLSerializer", "fetch"]);

ChromeUtils.defineModuleGetter(this, "OS",
                               "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");

function isAttr(obj) {
  return obj && typeof obj == "object" && ChromeUtils.getClassName(obj) == "Attr";
}
function isDocument(obj) {
  return obj && typeof obj == "object" && obj.nodeType == Element.DOCUMENT_NODE;
}
function isElement(obj) {
  return Element.isInstance(obj);
}
function isText(obj) {
  return obj && typeof obj == "object" && ChromeUtils.getClassName(obj) == "Text";
}

/**
 * Logs an error message to the error console
 */
function ERROR(str) {
  Cu.reportError(str);
}

function RDF_R(name) {
  return NS_RDF + name;
}

function renameNode(domnode, namespaceURI, qname) {
  if (isElement(domnode)) {
    var newdomnode = domnode.ownerDocument.createElementNS(namespaceURI, qname);
    if ("listCounter" in domnode)
      newdomnode.listCounter = domnode.listCounter;
    domnode.replaceWith(newdomnode);
    while (domnode.firstChild)
      newdomnode.appendChild(domnode.firstChild);
    for (let attr of domnode.attributes) {
      domnode.removeAttributeNode(attr);
      newdomnode.setAttributeNode(attr);
    }
    return newdomnode;
  } else if (isAttr(domnode)) {
    if (domnode.ownerElement.hasAttribute(namespaceURI, qname))
      throw new Error("attribute already exists");
    var attr = domnode.ownerDocument.createAttributeNS(namespaceURI, qname);
    attr.value = domnode.value;
    domnode.ownerElement.setAttributeNode(attr);
    domnode.ownerElement.removeAttributeNode(domnode);
    return attr;
  }
  throw new Error("cannot rename node of this type");
}

function predicateOrder(a, b) {
  return a.getPredicate().localeCompare(b.getPredicate());
}

/**
 * Returns either an rdf namespaced attribute or an un-namespaced attribute
 * value. Returns null if neither exists,
 */
function getRDFAttribute(element, name) {
  if (element.hasAttributeNS(NS_RDF, name))
    return element.getAttributeNS(NS_RDF, name);
  if (element.hasAttribute(name))
    return element.getAttribute(name);
  return undefined;
}

/**
 * Represents an assertion in the datasource
 */
class RDFAssertion {
  constructor(subject, predicate, object) {
    if (!(subject instanceof RDFSubject))
      throw new Error("subject must be an RDFSubject");

    if (typeof(predicate) != "string")
      throw new Error("predicate must be a string URI");

    if (!(object instanceof RDFLiteral) && !(object instanceof RDFSubject))
      throw new Error("object must be a concrete RDFNode");

    if (object instanceof RDFSubject && object._ds != subject._ds)
      throw new Error("object must be from the same datasource as subject");

    // The subject on this assertion, an RDFSubject
    this._subject = subject;
    // The predicate, a string
    this._predicate = predicate;
    // The object, an RDFNode
    this._object = object;
    // The datasource this assertion exists in
    this._ds = this._subject._ds;
    // Marks that _DOMnode is the subject's element
    this._isSubjectElement = false;
    // The DOM node that represents this assertion. Could be a property element,
    // a property attribute or the subject's element for rdf:type
    this._DOMNode = null;
  }

  /**
   * Adds content to _DOMnode to store this assertion in the DOM document.
   */
  _applyToDOMNode() {
    if (this._object instanceof RDFLiteral)
      this._object._applyToDOMNode(this._ds, this._DOMnode);
    else
      this._object._addReferenceToElement(this._DOMnode);
  }

  /**
   * Returns the DOM Element linked to the subject that this assertion is
   * attached to.
   */
  _getSubjectElement() {
    if (isAttr(this._DOMnode))
      return this._DOMnode.ownerElement;
    if (this._isSubjectElement)
      return this._DOMnode;
    return this._DOMnode.parentNode;
  }

  getSubject() {
    return this._subject;
  }

  getPredicate() {
    return this._predicate;
  }

  getObject() {
    return this._object;
  }
}

class RDFNode {
  equals(rdfnode) {
    return (rdfnode.constructor === this.constructor &&
            rdfnode._value == this._value);
  }
}

/**
 * A simple literal value
 */
class RDFLiteral extends RDFNode {
  constructor(value) {
    super();
    this._value = value;
  }

  /**
   * This stores the value of the literal in the given DOM node
   */
  _applyToDOMNode(ds, domnode) {
    if (isElement(domnode))
      domnode.textContent = this._value;
    else if (isAttr(domnode))
      domnode.value = this._value;
    else
      throw new Error("cannot use this node for a literal");
  }

  getValue() {
    return this._value;
  }
}

/**
 * A literal that is integer typed.
 */
class RDFIntLiteral extends RDFLiteral {
  constructor(value) {
    super(parseInt(value));
  }

  /**
   * This stores the value of the literal in the given DOM node
   */
  _applyToDOMNode(ds, domnode) {
    if (!isElement(domnode))
      throw new Error("cannot use this node for a literal");

    RDFLiteral.prototype._applyToDOMNode.call(this, ds, domnode);
    var prefix = ds._resolvePrefix(domnode, `${NS_NC}parseType`);
    domnode.setAttributeNS(prefix.namespaceURI, prefix.qname, "Integer");
  }
}

/**
 * A literal that represents a date.
 */
class RDFDateLiteral extends RDFLiteral {
  constructor(value) {
    if (!(value instanceof Date))
      throw new Error("RDFDateLiteral must be constructed with a Date object");

    super(value);
  }

  /**
   * This stores the value of the literal in the given DOM node
   */
  _applyToDOMNode(ds, domnode) {
    if (!isElement(domnode))
      throw new Error("cannot use this node for a literal");

    domnode.textContent = this._value.getTime();
    var prefix = ds._resolvePrefix(domnode, `${NS_NC}parseType`);
    domnode.setAttributeNS(prefix.namespaceURI, prefix.qname, "Date");
  }
}

/**
 * This is an RDF node that can be a subject so a resource or a blank node
 */
class RDFSubject extends RDFNode {
  constructor(ds) {
    super();
    // A lookup of the assertions with this as the subject. Keyed on predicate
    this._assertions = {};
    // A lookup of the assertions with this as the object. Keyed on predicate
    this._backwards = {};
    // The datasource this subject belongs to
    this._ds = ds;
    // The DOM elements in the document that represent this subject. Array of Element
    this._elements = [];
  }

  /**
   * Creates a new Element in the document for holding assertions about this
   * subject. The URI controls what tagname to use.
   */
  _createElement(uri) {
    // Seek an appropriate reference to this node to add this node under
    var parent = null;
    for (var p in this._backwards) {
      for (let back of this._backwards[p]) {
        // Don't add under an rdf:type
        if (back.getPredicate() == RDF_R("type"))
          continue;
        // The assertion already has a child node, probably one of ours
        if (back._DOMnode.firstChild)
          continue;
        parent = back._DOMnode;
        var element = this._ds._addElement(parent, uri);
        this._removeReferenceFromElement(parent);
        break;
      }
      if (parent)
        break;
    }

    // No back assertions that are sensible to use
    if (!parent)
      element = this._ds._addElement(this._ds._document.documentElement, uri);

    element.listCounter = 1;
    this._applyToElement(element);
    this._elements.push(element);
    return element;
  }

  /**
   * When a DOM node representing this subject is removed from the document
   * we must remove the node and recreate any child assertions elsewhere.
   */
  _removeElement(element) {
    var pos = this._elements.indexOf(element);
    if (pos < 0)
      throw new Error("invalid element");
    this._elements.splice(pos, 1);
    if (element.parentNode != element.ownerDocument.documentElement)
      this._addReferenceToElement(element.parentNode);
    this._ds._removeElement(element);

    // Find all the assertions that are represented here and create new
    // nodes for them.
    for (var predicate in this._assertions) {
      for (let assertion of this._assertions[predicate]) {
        if (assertion._getSubjectElement() == element)
          this._createDOMNodeForAssertion(assertion);
      }
    }
  }

  /**
   * Creates a DOM node to represent the assertion in the document. If the
   * assertion has rdf:type as the predicate then an attempt will be made to
   * create a typed subject Element, otherwise a new property Element is
   * created. For list items an attempt is made to find an appropriate container
   * that an rdf:li element can be added to.
   */
  _createDOMNodeForAssertion(assertion) {
    let elements;
    if (RDF_LISTITEM.test(assertion.getPredicate())) {
      // Find all the containers
      elements = this._elements.filter(function(element) {
        return (element.namespaceURI == NS_RDF && (element.localName == "Seq" ||
                                                   element.localName == "Bag" ||
                                                   element.localName == "Alt"));
      });
      if (elements.length > 0) {
        // Look for one whose listCounter matches the item we want to add
        var item = parseInt(assertion.getPredicate().substring(NS_RDF.length + 1));
        for (let element of elements) {
          if (element.listCounter == item) {
            assertion._DOMnode = this._ds._addElement(element, RDF_R("li"));
            assertion._applyToDOMNode();
            element.listCounter++;
            return;
          }
        }
        // No good container to add to, shove in the first real container
        assertion._DOMnode = this._ds._addElement(elements[0], assertion.getPredicate());
        assertion._applyToDOMNode();
        return;
      }
      // TODO No containers, this will end up in a non-container for now
    } else if (assertion.getPredicate() == RDF_R("type")) {
      // Try renaming an existing rdf:Description
      for (let element of this.elements) {
        if (element.namespaceURI == NS_RDF &&
            element.localName == "Description") {
          try {
            var prefix = this._ds._resolvePrefix(element.parentNode, assertion.getObject().getURI());
            element = renameNode(element, prefix.namespaceURI, prefix.qname);
            assertion._DOMnode = element;
            assertion._isSubjectElement = true;
            return;
          } catch (e) {
            // If the type cannot be sensibly turned into a prefix then just set
            // as a regular property
          }
        }
      }
    }

    // Filter out all the containers
    elements = this._elements.filter(function(element) {
      return (element.namespaceURI != NS_RDF || (element.localName != "Seq" &&
                                                 element.localName != "Bag" &&
                                                 element.localName != "Alt"));
    });
    if (elements.length == 0) {
      // Create a new node of the right type
      if (assertion.getPredicate() == RDF_R("type")) {
        try {
          assertion._DOMnode = this._createElement(assertion.getObject().getURI());
          assertion._isSubjectElement = true;
          return;
        } catch (e) {
          // If the type cannot be sensibly turned into a prefix then just set
          // as a regular property
        }
      }
      elements[0] = this._createElement(RDF_R("Description"));
    }
    assertion._DOMnode = this._ds._addElement(elements[0], assertion.getPredicate());
    assertion._applyToDOMNode();
  }

  /**
   * Removes the DOM node representing the assertion.
   */
  _removeDOMNodeForAssertion(assertion) {
    if (isAttr(assertion._DOMnode)) {
      var parent = assertion._DOMnode.ownerElement;
      parent.removeAttributeNode(assertion._DOMnode);
    } else if (assertion._isSubjectElement) {
      var domnode = renameNode(assertion._DOMnode, NS_RDF, "Description");
      if (domnode != assertion._DOMnode) {
        var pos = this._elements.indexOf(assertion._DOMnode);
        this._elements.splice(pos, 1, domnode);
      }
      parent = domnode;
    } else {
      var object = assertion.getObject();
      if (object instanceof RDFSubject && assertion._DOMnode.firstChild) {
        // Object is a subject that has an Element inside this assertion's node.
        for (let element of object._elements) {
          if (element.parentNode == assertion._DOMnode) {
            object._removeElement(element);
            break;
          }
        }
      }
      parent = assertion._DOMnode.parentNode;
      if (assertion._DOMnode.namespaceURI == NS_RDF &&
          assertion._DOMnode.localName == "li")
      parent.listCounter--;
      this._ds._removeElement(assertion._DOMnode);
    }

    // If there are no assertions left using the assertion's containing dom node
    // then remove it from the document.
    // TODO could do with a quick lookup list for assertions attached to a node
    for (var p in this._assertions) {
      for (let assertion of this._assertions[p]) {
        if (assertion._getSubjectElement() == parent)
          return;
      }
    }
    // No assertions left in this element.
    this._removeElement(parent);
  }

  /**
   * Parses the given Element from the DOM document
   */
  _parseElement(element) {
    this._elements.push(element);

    // There might be an inferred rdf:type assertion in the element name
    if (element.namespaceURI != NS_RDF ||
        element.localName != "Description") {
      if (element.namespaceURI == NS_RDF && element.localName == "li")
        throw new Error("rdf:li is not a valid type for a subject node");
      var assertion = new RDFAssertion(this, RDF_R("type"),
                                       this._ds.getResource(element.namespaceURI + element.localName));
      assertion._DOMnode = element;
      assertion._isSubjectElement = true;
      this._addAssertion(assertion);
    }

    // Certain attributes can be literal properties
    for (let attr of element.attributes) {
      if (attr.namespaceURI == NS_XML || attr.namespaceURI == NS_XMLNS ||
          attr.nodeName == "xmlns")
        continue;
      if ((attr.namespaceURI == NS_RDF || !attr.namespaceURI) &&
          (["nodeID", "about", "resource", "ID", "parseType"].includes(attr.localName)))
        continue;
      var object = null;
      if (attr.namespaceURI == NS_RDF) {
        if (attr.localName == "type")
          object = this._ds.getResource(attr.nodeValue);
        else if (attr.localName == "li")
          throw new Error("rdf:li is not allowed as a property attribute");
        else if (attr.localName == "aboutEach")
          throw new Error("rdf:aboutEach is deprecated");
        else if (attr.localName == "aboutEachPrefix")
          throw new Error("rdf:aboutEachPrefix is deprecated");
        else if (attr.localName == "aboutEach")
          throw new Error("rdf:aboutEach is deprecated");
        else if (attr.localName == "bagID")
          throw new Error("rdf:bagID is deprecated");
      }
      if (!object)
        object = new RDFLiteral(attr.nodeValue);
      assertion = new RDFAssertion(this, attr.namespaceURI + attr.localName, object);
      assertion._DOMnode = attr;
      this._addAssertion(assertion);
    }

    var child = element.firstChild;
    element.listCounter = 1;
    while (child) {
      if (isText(child) && /\S/.test(child.nodeValue)) {
        ERROR(`Text ${child.nodeValue} is not allowed in a subject node`);
        throw new Error("subject nodes cannot contain text content");
      } else if (isElement(child)) {
        object = null;
        var predicate = child.namespaceURI + child.localName;
        if (child.namespaceURI == NS_RDF) {
          if (RDF_PROPERTY_INVALID_TYPES.includes(child.localName) &&
              !child.localName.match(/^_\d+$/))
            throw new Error(`${child.nodeName} is an invalid property`);
          if (child.localName == "li") {
            predicate = RDF_R(`_${element.listCounter}`);
            element.listCounter++;
          }
        }

        // Check for and bail out on unknown attributes on the property element
        for (let attr of child.attributes) {
          // Ignore XML namespaced attributes
          if (attr.namespaceURI == NS_XML)
            continue;
          // These are reserved by XML for future use
          if (attr.localName.substring(0, 3).toLowerCase() == "xml")
            continue;
          // We can handle these RDF attributes
          if ((!attr.namespaceURI || attr.namespaceURI == NS_RDF) &&
              ["resource", "nodeID"].includes(attr.localName))
            continue;
          // This is a special attribute we handle for compatibility with Mozilla RDF
          if (attr.namespaceURI == NS_NC &&
              attr.localName == "parseType")
            continue;
          throw new Error(`Attribute ${attr.nodeName} is not supported`);
        }

        var parseType = child.getAttributeNS(NS_NC, "parseType");
        if (parseType && parseType != "Date" && parseType != "Integer") {
          ERROR(`parseType ${parseType} is not supported`);
          throw new Error("unsupported parseType");
        }

        var resource = getRDFAttribute(child, "resource");
        var nodeID = getRDFAttribute(child, "nodeID");
        if ((resource && (nodeID || parseType)) ||
            (nodeID && (resource || parseType))) {
          ERROR("Cannot use more than one of parseType, resource and nodeID on a single node");
          throw new Error("Invalid rdf assertion");
        }

        if (resource !== undefined) {
          var base = Services.io.newURI(element.baseURI);
          object = this._ds.getResource(base.resolve(resource));
        } else if (nodeID !== undefined) {
          if (!nodeID.match(XML_NCNAME))
            throw new Error("rdf:nodeID must be a valid XML name");
          object = this._ds.getBlankNode(nodeID);
        } else {
          var hasText = false;
          var childElement = null;
          var subchild = child.firstChild;
          while (subchild) {
            if (isText(subchild) && /\S/.test(subchild.nodeValue)) {
              hasText = true;
            } else if (isElement(subchild)) {
              if (childElement) {
                new Error(`Multiple object elements found in ${child.nodeName}`);
              }
              childElement = subchild;
            }
            subchild = subchild.nextSibling;
          }

          if ((resource || nodeID) && (hasText || childElement)) {
            ERROR("Assertion references a resource so should not contain additional contents");
            throw new Error("assertion cannot contain multiple objects");
          }

          if (hasText && childElement) {
            ERROR(`Both literal and resource objects found in ${child.nodeName}`);
            throw new Error("assertion cannot contain multiple objects");
          }

          if (childElement) {
            if (parseType) {
              ERROR("Cannot specify a parseType for an assertion with resource object");
              throw new Error("parseType is not valid in this context");
            }
            object = this._ds._getSubjectForElement(childElement);
            object._parseElement(childElement);
          } else if (parseType == "Integer")
            object = new RDFIntLiteral(child.textContent);
          else if (parseType == "Date")
            object = new RDFDateLiteral(new Date(child.textContent));
          else
            object = new RDFLiteral(child.textContent);
        }

        assertion = new RDFAssertion(this, predicate, object);
        this._addAssertion(assertion);
        assertion._DOMnode = child;
      }
      child = child.nextSibling;
    }
  }

  /**
   * Adds a new assertion to the internal hashes. Should be called for every
   * new assertion parsed or created programmatically.
   */
  _addAssertion(assertion) {
    var predicate = assertion.getPredicate();
    if (predicate in this._assertions)
      this._assertions[predicate].push(assertion);
    else
      this._assertions[predicate] = [ assertion ];

    var object = assertion.getObject();
    if (object instanceof RDFSubject) {
      // Create reverse assertion
      if (predicate in object._backwards)
        object._backwards[predicate].push(assertion);
      else
        object._backwards[predicate] = [ assertion ];
    }
  }

  /**
   * Removes an assertion from the internal hashes. Should be called for all
   * assertions that are programatically deleted.
   */
  _removeAssertion(assertion) {
    var predicate = assertion.getPredicate();
    if (predicate in this._assertions) {
      var pos = this._assertions[predicate].indexOf(assertion);
      if (pos >= 0)
        this._assertions[predicate].splice(pos, 1);
      if (this._assertions[predicate].length == 0)
        delete this._assertions[predicate];
    }

    var object = assertion.getObject();
    if (object instanceof RDFSubject) {
      // Delete reverse assertion
      if (predicate in object._backwards) {
        pos = object._backwards[predicate].indexOf(assertion);
        if (pos >= 0)
          object._backwards[predicate].splice(pos, 1);
        if (object._backwards[predicate].length == 0)
          delete object._backwards[predicate];
      }
    }
  }

  /**
   * Returns the ordinal assertions from this subject in order.
   */
  _getChildAssertions() {
    var assertions = [];
    for (var i in this._assertions) {
      if (RDF_LISTITEM.test(i))
        assertions.push(...this._assertions[i]);
    }
    assertions.sort(predicateOrder);
    return assertions;
  }

  /**
   * Compares this to another rdf node
   */
  equals(rdfnode) {
    // subjects are created by the datasource so no two objects ever correspond
    // to the same one.
    return this === rdfnode;
  }

  /**
   * Adds a new assertion with this as the subject
   */
  assert(predicate, object) {
    if (predicate == RDF_R("type") && !(object instanceof RDFResource))
      throw new Error("rdf:type must be an RDFResource");

    var assertion = new RDFAssertion(this, predicate, object);
    this._createDOMNodeForAssertion(assertion);
    this._addAssertion(assertion);
  }

  /**
   * Removes an assertion matching the predicate and node given, if such an
   * assertion exists.
   */
  unassert(predicate, object) {
    if (!(predicate in this._assertions))
      return;

    for (let assertion of this._assertions[predicate]) {
      if (assertion.getObject().equals(object)) {
        this._removeAssertion(assertion);
        this._removeDOMNodeForAssertion(assertion);
        return;
      }
    }
  }

  /**
   * Returns an array of all the predicates that exist in assertions from this
   * subject.
   */
  getPredicates() {
    return Object.keys(this._assertions);
  }

  /**
   * Returns all objects in assertions with this subject and the given predicate.
   */
  getObjects(predicate) {
    if (predicate in this._assertions)
      return Array.from(this._assertions[predicate],
                        i => i.getObject());

    return [];
  }

  /**
   * Returns all of the ordinal children of this subject in order.
   */
  getChildren() {
    return Array.from(this._getChildAssertions(),
                      i => i.getObject());
  }

  /**
   * Removes the child at the given index. This is the index based on the
   * children returned from getChildren. Forces a reordering of the later
   * children.
   */
  removeChildAt(pos) {
    if (pos < 0)
      throw new Error("no such child");
    var assertions = this._getChildAssertions();
    if (pos >= assertions.length)
      throw new Error("no such child");
    for (var i = pos; i < assertions.length; i++) {
      this._removeAssertion(assertions[i]);
      this._removeDOMNodeForAssertion(assertions[i]);
    }
    var index = 1;
    if (pos > 0)
      index = parseInt(assertions[pos - 1].getPredicate().substring(NS_RDF.length + 1)) + 1;
    for (let i = pos + 1; i < assertions.length; i++) {
      assertions[i]._predicate = RDF_R(`_${index}`);
      this._addAssertion(assertions[i]);
      this._createDOMNodeForAssertion(assertions[i]);
      index++;
    }
  }

  /**
   * Removes the child with the given object. It is unspecified which child is
   * removed if the object features more than once.
   */
  removeChild(object) {
    var assertions = this._getChildAssertions();
    for (var pos = 0; pos < assertions.length; pos++) {
      if (assertions[pos].getObject().equals(object)) {
        for (var i = pos; i < assertions.length; i++) {
          this._removeAssertion(assertions[i]);
          this._removeDOMNodeForAssertion(assertions[i]);
        }
        var index = 1;
        if (pos > 0)
          index = parseInt(assertions[pos - 1].getPredicate().substring(NS_RDF.length + 1)) + 1;
        for (let i = pos + 1; i < assertions.length; i++) {
          assertions[i]._predicate = RDF_R(`_${index}`);
          this._addAssertion(assertions[i]);
          this._createDOMNodeForAssertion(assertions[i]);
          index++;
        }
        return;
      }
    }
    throw new Error("no such child");
  }

  /**
   * Adds a new ordinal child to this subject.
   */
  addChild(object) {
    var max = 0;
    for (var i in this._assertions) {
      if (RDF_LISTITEM.test(i))
        max = Math.max(max, parseInt(i.substring(NS_RDF.length + 1)));
    }
    max++;
    this.assert(RDF_R(`_${max}`), object);
  }

  /**
   * This reorders the child assertions to remove duplicates and gaps in the
   * sequence. Generally this will move all children to be under the same
   * container element and all represented as an rdf:li
   */
  reorderChildren() {
    var assertions = this._getChildAssertions();
    for (let assertion of assertions) {
      this._removeAssertion(assertion);
      this._removeDOMNodeForAssertion(assertion);
    }
    var index = 1;
    for (let assertion of assertions) {
      assertion._predicate = RDF_R(`_${index}`);
      this._addAssertion(assertion);
      this._createDOMNodeForAssertion(assertion);
      index++;
    }
  }

  /**
   * Returns the type of this subject or null if there is no specified type.
   */
  getType() {
    var type = this.getProperty(RDF_R("type"));
    if (type && type instanceof RDFResource)
      return type.getURI();
    return null;
  }

  /**
   * Tests if a property exists for the given predicate.
   */
  hasProperty(predicate) {
    return (predicate in this._assertions);
  }

  /**
   * Retrieves the first property value for the given predicate.
   */
  getProperty(predicate) {
    if (predicate in this._assertions)
      return this._assertions[predicate][0].getObject();
    return null;
  }

  /**
   * Sets the property value for the given predicate, clearing any existing
   * values.
   */
  setProperty(predicate, object) {
    // TODO optimise by replacing the first assertion and clearing the rest
    this.clearProperty(predicate);
    this.assert(predicate, object);
  }

  /**
   * Clears any existing properties for the given predicate.
   */
  clearProperty(predicate) {
    if (!(predicate in this._assertions))
      return;

    var assertions = this._assertions[predicate];
    while (assertions.length > 0) {
      var assertion = assertions[0];
      this._removeAssertion(assertion);
      this._removeDOMNodeForAssertion(assertion);
    }
  }
}

/**
 * Creates a new RDFResource for the datasource. Private.
 */
class RDFResource extends RDFSubject {
  constructor(ds, uri) {
    if (!(ds instanceof RDFDataSource))
      throw new Error("datasource must be an RDFDataSource");

    if (!uri)
      throw new Error("An RDFResource requires a non-null uri");

    super(ds);
    // This is the uri that the resource represents.
    this._uri = uri;
  }

  /**
   * Sets attributes on the DOM element to mark it as representing this resource
   */
  _applyToElement(element) {
    if (USE_RDFNS_ATTR) {
      var prefix = this._ds._resolvePrefix(element, RDF_R("about"));
      element.setAttributeNS(prefix.namespaceURI, prefix.qname, this._uri);
    } else {
      element.setAttribute("about", this._uri);
    }
  }

  /**
   * Adds a reference to this resource to the given property Element.
   */
  _addReferenceToElement(element) {
    if (USE_RDFNS_ATTR) {
      var prefix = this._ds._resolvePrefix(element, RDF_R("resource"));
      element.setAttributeNS(prefix.namespaceURI, prefix.qname, this._uri);
    } else {
      element.setAttribute("resource", this._uri);
    }
  }

    /**
     * Removes any reference to this resource from the given property Element.
     */
    _removeReferenceFromElement(element) {
      if (element.hasAttributeNS(NS_RDF, "resource"))
        element.removeAttributeNS(NS_RDF, "resource");
      if (element.hasAttribute("resource"))
        element.removeAttribute("resource");
    }

  getURI() {
    return this._uri;
  }
}

/**
 * Creates a new blank node. Private.
 */
class RDFBlankNode extends RDFSubject {
  constructor(ds, nodeID) {
    if (!(ds instanceof RDFDataSource))
      throw new Error("datasource must be an RDFDataSource");

    super(ds);
    // The nodeID of this node. May be null if there is no ID.
    this._nodeID = nodeID;
  }

  /**
   * Sets attributes on the DOM element to mark it as representing this node
   */
  _applyToElement(element) {
    if (!this._nodeID)
      return;
    if (USE_RDFNS_ATTR) {
      var prefix = this._ds._resolvePrefix(element, RDF_R("nodeID"));
      element.setAttributeNS(prefix.namespaceURI, prefix.qname, this._nodeID);
    } else {
      element.setAttribute("nodeID", this._nodeID);
    }
  }

  /**
   * Creates a new Element in the document for holding assertions about this
   * subject. The URI controls what tagname to use.
   */
  _createNewElement(uri) {
    // If there are already nodes representing this in the document then we need
    // a nodeID to match them
    if (!this._nodeID && this._elements.length > 0) {
      this._ds._createNodeID(this);
      for (let element of this._elements)
        this._applyToElement(element);
    }

    return super._createNewElement.call(uri);
  }

  /**
   * Adds a reference to this node to the given property Element.
   */
  _addReferenceToElement(element) {
    if (this._elements.length > 0 && !this._nodeID) {
      // In document elsewhere already
      // Create a node ID and update the other nodes referencing
      this._ds._createNodeID(this);
      for (let element of this._elements)
        this._applyToElement(element);
    }

    if (this._nodeID) {
      if (USE_RDFNS_ATTR) {
        let prefix = this._ds._resolvePrefix(element, RDF_R("nodeID"));
        element.setAttributeNS(prefix.namespaceURI, prefix.qname, this._nodeID);
      } else {
        element.setAttribute("nodeID", this._nodeID);
      }
    } else {
      // Add the empty blank node, this is generally right since further
      // assertions will be added to fill this out
      var newelement = this._ds._addElement(element, RDF_R("Description"));
      newelement.listCounter = 1;
      this._elements.push(newelement);
    }
  }

    /**
     * Removes any reference to this node from the given property Element.
     */
    _removeReferenceFromElement(element) {
      if (element.hasAttributeNS(NS_RDF, "nodeID"))
        element.removeAttributeNS(NS_RDF, "nodeID");
      if (element.hasAttribute("nodeID"))
        element.removeAttribute("nodeID");
    }

  getNodeID() {
    return this._nodeID;
  }
}

/**
 * Creates a new RDFDataSource from the given document. The document will be
 * changed as assertions are added and removed to the RDF. Pass a null document
 * to start with an empty graph.
 */
class RDFDataSource {
  constructor(document) {
    // All known resources, indexed on URI
    this._resources = {};
    // All blank nodes
    this._allBlankNodes = [];
    // All blank nodes with IDs, indexed on ID
    this._blankNodes = {};
    // Suggested prefixes to use for namespaces, index is prefix, value is namespaceURI.
    this._prefixes = {
      rdf: NS_RDF,
      NC: NS_NC
    };

    if (!document) {
      // Creating a document through xpcom leaves out the xml prolog so just parse
      // something small
      var parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                   createInstance(Ci.nsIDOMParser);
      var doctext = `<?xml version="1.0"?>\n<rdf:RDF xmlns:rdf="${NS_RDF}"/>\n`;
      document = parser.parseFromString(doctext, "text/xml");
    }
    // The underlying DOM document for this datasource
    this._document = document;
    this._parseDocument();
  }

  static loadFromString(text) {
    let parser = new DOMParser();
    let document = parser.parseFromString(text, "application/xml");

    return new this(document);
  }

  static loadFromBuffer(buffer) {
    let parser = new DOMParser();
    let document = parser.parseFromBuffer(new Uint8Array(buffer), "application/xml");

    return new this(document);
  }

  static async loadFromFile(uri) {
    if (uri instanceof Ci.nsIFile)
      uri = Services.io.newFileURI(uri);
    else if (typeof(uri) == "string")
      uri = Services.io.newURI(uri);

    let resp = await fetch(uri.spec);
    return this.loadFromBuffer(await resp.arrayBuffer());
  }

  get uri() {
    return this._document.documentURI;
  }

  /**
   * Creates a new nodeID for an unnamed blank node. Just node<number>.
   */
  _createNodeID(blanknode) {
    var i = 1;
    while (`node${i}` in this._blankNodes)
      i++;
    blanknode._nodeID = `node${i}`;
    this._blankNodes[blanknode._nodeID] = blanknode;
  }

  /**
   * Returns an rdf subject for the given DOM Element. If the subject has not
   * been seen before a new one is created.
   */
  _getSubjectForElement(element) {
    if (element.namespaceURI == NS_RDF &&
        RDF_NODE_INVALID_TYPES.includes(element.localName))
      throw new Error(`${element.nodeName} is not a valid class for a subject node`);

    var about = getRDFAttribute(element, "about");
    var id = getRDFAttribute(element, "ID");
    var nodeID = getRDFAttribute(element, "nodeID");

    if ((about && (id || nodeID)) ||
        (nodeID && (id || about))) {
      ERROR("More than one of about, ID and nodeID present on the same subject");
      throw new Error("invalid subject in rdf");
    }

    if (about !== undefined) {
      let base = Services.io.newURI(element.baseURI);
      return this.getResource(base.resolve(about));
    }
    if (id !== undefined) {
      if (!id.match(XML_NCNAME))
        throw new Error("rdf:ID must be a valid XML name");
      let base = Services.io.newURI(element.baseURI);
      return this.getResource(base.resolve(`#${id}`));
    }
    if (nodeID !== undefined)
      return this.getBlankNode(nodeID);
    return this.getBlankNode(null);
  }

  /**
   * Parses the document for subjects at the top level.
   */
  _parseDocument() {
    if (!this._document.documentElement) {
      ERROR("No document element in document");
      throw new Error("document contains no root element");
    }

    if (this._document.documentElement.namespaceURI != NS_RDF ||
        this._document.documentElement.localName != "RDF") {
      ERROR(`${this._document.documentElement.nodeName} is not rdf:RDF`);
      throw new Error("document does not appear to be RDF");
    }

    var domnode = this._document.documentElement.firstChild;
    while (domnode) {
      if (isText(domnode) && /\S/.test(domnode.nodeValue)) {
        ERROR("RDF does not allow for text in the root of the document");
        throw new Error("invalid markup in document");
      } else if (isElement(domnode)) {
        var subject = this._getSubjectForElement(domnode);
        subject._parseElement(domnode);
      }
      domnode = domnode.nextSibling;
    }
  }

  /**
   * Works out a sensible namespace prefix to use for the given uri. node should
   * be the parent of where the element is to be inserted, or the node that an
   * attribute is to be added to. This will recursively walk to the top of the
   * document finding an already registered prefix that matches for the uri.
   * If none is found a new prefix is registered.
   * This returns an object with keys namespaceURI, prefix, localName and qname.
   * Pass null or undefined for badPrefixes for the first call.
   */
  _resolvePrefix(domnode, uri, badPrefixes) {
    if (!badPrefixes)
      badPrefixes = [];

    // No known prefix, try to create one from the lookup list
    if (!domnode || isDocument(domnode)) {
      for (let i in this._prefixes) {
        if (badPrefixes.includes(i))
          continue;
        if (this._prefixes[i] == uri.substring(0, this._prefixes[i].length)) {
          var local = uri.substring(this._prefixes[i].length);
          var test = URI_SUFFIX.exec(local);
          // Remaining part of uri is a good XML Name
          if (test && test[0] == local) {
            this._document.documentElement.setAttributeNS(NS_XMLNS, `xmlns:${i}`, this._prefixes[i]);
            return {
              namespaceURI: this._prefixes[i],
              prefix: i,
              localName: local,
              qname: i ? `${i}:${local}` : local
            };
          }
        }
      }

      // No match, make something up
      test = URI_SUFFIX.exec(uri);
      if (test) {
        var namespaceURI = uri.substring(0, uri.length - test[0].length);
        local = test[0];
        let i = 1;
        while (badPrefixes.includes(`NS${i}`))
          i++;
        this._document.documentElement.setAttributeNS(NS_XMLNS, `xmlns:NS${i}`, namespaceURI);
        return {
          namespaceURI,
          prefix: `NS${i}`,
          localName: local,
          qname: `NS${i}:${local}`,
        };
      }
      // There is no end part of this URI that is an XML Name
      throw new Error(`invalid node name: ${uri}`);
    }

    for (let attr of domnode.attributes) {
      // Not a namespace declaration, ignore this attribute
      if (attr.namespaceURI != NS_XMLNS && attr.nodeName != "xmlns")
        continue;

      var prefix = attr.prefix ? attr.localName : "";
      // Seen this prefix before, cannot use it
      if (badPrefixes.includes(prefix))
        continue;

      // Namespace matches the start of the uri
      if (attr.value == uri.substring(0, attr.value.length)) {
        local = uri.substring(attr.value.length);
        test = URI_SUFFIX.exec(local);
        // Remaining part of uri is a good XML Name
        if (test && test[0] == local) {
          return {
            namespaceURI: attr.value,
            prefix,
            localName: local,
            qname: prefix ? `${prefix}:${local}` : local
          };
        }
      }

      badPrefixes.push(prefix);
    }

    // No prefix found here, move up the document
    return this._resolvePrefix(domnode.parentNode, uri, badPrefixes);
  }

  /**
   * Guess the indent level within the given Element. The method looks for
   * elements that are preceeded by whitespace including a newline. The
   * whitespace following the newline is presumed to be the indentation for the
   * element.
   * If the indentation cannot be guessed then it recurses up the document
   * hierarchy until it can guess the indent or until the Document is reached.
   */
  _guessIndent(element) {
    // The indent at document level is 0
    if (!element || isDocument(element))
      return "";

    // Check the text immediately preceeding each child node. One could be
    // a valid indent
    var pretext = "";
    var child = element.firstChild;
    while (child) {
      if (isText(child)) {
        pretext += child.nodeValue;
      } else if (isElement(child)) {
        var result = INDENT.exec(pretext);
        if (result)
          return result[1];
        pretext = "";
      }
      child = child.nextSibling;
    }

    // pretext now contains any trailing text in the element. This can be
    // the indent of the end tag. If so add a little to it.
    result = INDENT.exec(pretext);
    if (result)
      return `${result[1]}  `;

    // Check the text immediately before this node
    pretext = "";
    var sibling = element.previousSibling;
    while (sibling && isText(sibling)) {
      pretext += sibling.nodeValue;
      sibling = sibling.previousSibling;
    }

    // If there is a sensible indent then just add to it.
    result = INDENT.exec(pretext);
    if (result)
      return `${result[1]}  `;

    // Last chance, get the indent level for the tag above and add to it
    return `${this._guessIndent(element.parentNode)}  `;
  }

  _addElement(parent, uri) {
    var prefix = this._resolvePrefix(parent, uri);
    var element = this._document.createElementNS(prefix.namespaceURI, prefix.qname);

    if (parent.lastChild) {
      // We want to insert immediately after the last child element
      var last = parent.lastChild;
      while (last && isText(last))
        last = last.previousSibling;
      // No child elements so insert at the start
      if (!last)
        last = parent.firstChild;
      else
        last = last.nextSibling;

      let indent = this._guessIndent(parent);
      parent.insertBefore(this._document.createTextNode(`\n${indent}`), last);
      parent.insertBefore(element, last);
    } else {
      // No children, must indent our element and the end tag
      let indent = this._guessIndent(parent.parentNode);
      parent.append(`\n${indent}  `, element, `\n${indent}`);
    }
    return element;
  }

  /**
   * Removes the element from its parent. Should also remove surrounding
   * white space as appropriate.
   */
  _removeElement(element) {
    var parent = element.parentNode;
    var sibling = element.previousSibling;
    // Drop any text nodes immediately preceeding the element
    while (sibling && isText(sibling)) {
      var temp = sibling;
      sibling = sibling.previousSibling;
      parent.removeChild(temp);
    }

    sibling = element.nextSibling;
    // Drop the element
    parent.removeChild(element);

    // If the next node after element is now the first child then element was
    // the first child. If there are no other child elements then remove the
    // remaining child nodes.
    if (parent.firstChild == sibling) {
      while (sibling && isText(sibling))
        sibling = sibling.nextSibling;
      if (!sibling) {
        // No other child elements
        while (parent.lastChild)
          parent.removeChild(parent.lastChild);
      }
    }
  }

  /**
   * Requests that a given prefix be used for the namespace where possible.
   * This must be called before any assertions are made using the namespace
   * and the registration will not override any existing prefix used in the
   * document.
   */
  registerPrefix(prefix, namespaceURI) {
    this._prefixes[prefix] = namespaceURI;
  }

  /**
   * Gets a blank node. nodeID may be null and if so a new blank node is created.
   * If a nodeID is given then the blank node with that ID is returned or created.
   */
  getBlankNode(nodeID) {
    if (nodeID && nodeID in this._blankNodes)
      return this._blankNodes[nodeID];

    if (nodeID && !nodeID.match(XML_NCNAME))
      throw new Error("rdf:nodeID must be a valid XML name");

    var rdfnode = new RDFBlankNode(this, nodeID);
    this._allBlankNodes.push(rdfnode);
    if (nodeID)
      this._blankNodes[nodeID] = rdfnode;
    return rdfnode;
  }

  /**
   * Gets all blank nodes
   */
  getAllBlankNodes() {
    return this._allBlankNodes.slice();
  }

  /**
   * Gets the resource for the URI. The resource is created if it has not been
   * used already.
   */
  getResource(uri) {
    if (uri in this._resources)
      return this._resources[uri];

    var resource = new RDFResource(this, uri);
    this._resources[uri] = resource;
    return resource;
  }

  /**
   * Gets all resources that have been used.
   */
  getAllResources() {
    return Object.values(this._resources);
  }

  /**
   * Returns all blank nodes and resources
   */
  getAllSubjects() {
    return [...Object.values(this._resources),
            ...this._allBlankNodes];
  }

  /**
   * Saves the RDF/XML to a string.
   */
  serializeToString() {
    var serializer = new XMLSerializer();
    return serializer.serializeToString(this._document);
  }

  /**
   * Saves the RDF/XML to a file.
   */
  async saveToFile(file) {
    return OS.File.writeAtomic(file, new TextEncoder().encode(this.serializeToString()));
  }
}
