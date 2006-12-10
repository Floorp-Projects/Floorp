function updateDocumentSourceMaps(source) {
  const nsIDOMNode = Components.interfaces.nsIDOMNode;

  const nsISAXXMLReader = Components.interfaces.nsISAXXMLReader;
  const saxReader = Components.classes["@mozilla.org/saxparser/xmlreader;1"]
                              .createInstance(nsISAXXMLReader);
  try {
    saxReader.setFeature("http://xml.org/sax/features/namespace-prefixes", true);
    saxReader.setFeature("http://xml.org/sax/features/namespace", true);
  }
  catch (e) {
    // do nothing, we'll accept it as it is.
  }
  var parseErrorLog = [];

  /* XXX ajvincent Because throwing an exception doesn't stop parsing, we need
   * to record errors and handle them after the parsing is finished.
   */
  function do_parse_check(aCondition, aMsg) {
    if (!aCondition)
      parseErrorLog[parseErrorLog.length] = aMsg;
  }

  var contentHandler = {
    startDocument: function startDocument() {
    },

    endDocument: function endDocument() {
    },

    handleAttributes: function handleAttributes(aAttributes) {
      for (var i = 0; i < aAttributes.length; i++) {
        var attrNamespaceURI = aAttributes.getURI(i);
        var attrLocalName = aAttributes.getLocalName(i);
        var attrNodeName = aAttributes.getQName(i);
        var value = aAttributes.getValue(i);
        do_parse_check(attrLocalName, "Missing attribute local name");
        do_parse_check(attrNodeName, "Missing attribute node name");
      }
    },

    startElement: function startElement(aNamespaceURI, aLocalName, aNodeName, aAttributes) {
      do_parse_check(aLocalName, "Missing element local name (startElement)");
      do_parse_check(aNodeName, "Missing element node name (startElement)");
      do_parse_check(aAttributes, "Missing element attributes");
      this.handleAttributes(aAttributes);
    },

    endElement: function endElement(aNamespaceURI, aLocalName, aNodeName) {
      do_parse_check(aLocalName, "Missing element local name (endElement)");
      do_parse_check(aNodeName, "Missing element node name (endElement)");
    },

    inCDataSection: false,

    characters: function characters(aData) {
    },

    processingInstruction: function processingInstruction(aTarget, aData) {
      do_parse_check(aTarget, "Missing processing instruction target");
    },

    ignorableWhitespace: function ignorableWhitespace(aWhitespace) {
    },

    startPrefixMapping: function startPrefixMapping(aPrefix, aURI) {
    },

    endPrefixMapping: function endPrefixMapping(aPrefix) {
    }
  };

  var lexicalHandler = {
    comment: function comment(aContents) {
    },

    startDTD: function startDTD(aName, aPublicId, aSystemId) {
      do_parse_check(aName, "Missing DTD name");
    },

    endDTD: function endDTD() {
    },

    startCDATA: function startCDATA() {
    },

    endCDATA: function endCDATA() {
    },

    startEntity: function startEntity(aName) {
      do_parse_check(aName, "Missing entity name (startEntity)");
    },

    endEntity: function endEntity(aName) {
      do_parse_check(aName, "Missing entity name (endEntity)");
    }
  };

  var dtdHandler = {
    notationDecl: function notationDecl(aName, aPublicId, aSystemId) {
      do_parse_check(aName, "Missing notation name");
    },

    unparsedEntityDecl:
    function unparsedEntityDecl(aName, aPublicId, aSystemId, aNotationName) {
      do_parse_check(aName, "Missing entity name (unparsedEntityDecl)");
    }
  };

  var errorHandler = {
    error: function error(aLocator, aError) {
      do_parse_check(!aError, "XML error");
    },

    fatalError: function fatalError(aLocator, aError) {
      do_parse_check(!aError, "XML fatal error");
    },

    ignorableWarning: function ignorableWarning(aLocator, aError) {
      do_parse_check(!aError, "XML ignorable warning");
    }
  };

  saxReader.contentHandler = contentHandler;
  saxReader.lexicalHandler = lexicalHandler;
  saxReader.dtdHandler     = dtdHandler;
  saxReader.errorHandler   = errorHandler;

  saxReader.parseFromString(source, "application/xml");

  // Just in case it leaks.
  saxReader.contentHandler = null;
  saxReader.lexicalHandler = null;
  saxReader.dtdHandler     = null;
  saxReader.errorHandler   = null;

  return parseErrorLog;
}

function do_check_true_with_dump(aCondition, aParseLog) {
  if (!aCondition) {
    dump(aParseLog.join("\n"));
  }
  do_check_true(aCondition);
}

function run_test() {
  var src;
  src = "<!DOCTYPE foo>\n<!-- all your foo are belong to bar -->";
  src += "<foo id='foo'>\n<?foo wooly bully?>\nfoo";
  src += "<![CDATA[foo fighters]]></foo>\n";
  var parseErrorLog = updateDocumentSourceMaps(src);

  if (parseErrorLog.length > 0) {
    dump(parseErrorLog.join("\n"));
  }
  do_check_true_with_dump(parseErrorLog.length == 0, parseErrorLog);

  // End tag isn't well-formed.
  src = "<!DOCTYPE foo>\n<!-- all your foo are belong to bar -->";
  src += "<foo id='foo'>\n<?foo wooly bully?>\nfoo";
  src += "<![CDATA[foo fighters]]></foo\n";

  parseErrorLog = updateDocumentSourceMaps(src);

  do_check_true_with_dump(parseErrorLog.length == 1 && parseErrorLog[0] == "XML fatal error", parseErrorLog);
}
