ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

function updateDocumentSourceMaps(src) {
  const nsISAXXMLReader = Ci.nsISAXXMLReader;
  const saxReader = Cc["@mozilla.org/saxparser/xmlreader;1"]
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

    characters: function characters(aData) {
    },

    processingInstruction: function processingInstruction(aTarget, aData) {
      do_parse_check(aTarget, "Missing processing instruction target");
    }
  };

  var errorHandler = {
    error: function error(aError) {
      do_parse_check(!aError, "XML error");
    },

    fatalError: function fatalError(aError) {
      do_parse_check(!aError, "XML fatal error");
    },

    ignorableWarning: function ignorableWarning(aError) {
      do_parse_check(!aError, "XML ignorable warning");
    }
  };

  saxReader.contentHandler = contentHandler;
  saxReader.errorHandler   = errorHandler;

  let type = "application/xml";
  let uri = NetUtil.newURI("http://example.org/");

  let sStream = Cc["@mozilla.org/io/string-input-stream;1"]
               .createInstance(Ci.nsIStringInputStream);
  sStream.setData(src, src.length);
  var bStream = Cc["@mozilla.org/network/buffered-input-stream;1"]
                .createInstance(Ci.nsIBufferedInputStream);
  bStream.init(sStream, 4096);

  let channel = Cc["@mozilla.org/network/input-stream-channel;1"].
    createInstance(Ci.nsIInputStreamChannel);
  channel.setURI(uri);
  channel.contentStream = bStream;
  channel.QueryInterface(Ci.nsIChannel);
  channel.contentType = type;

  saxReader.parseAsync(null, uri);
  saxReader.onStartRequest(channel, uri);

  let pos = 0;
  let count = bStream.available();
  while (count > 0) {
    saxReader.onDataAvailable(channel, null, bStream, pos, count);
    pos += count;
    count = bStream.available();
  }
  saxReader.onStopRequest(channel, null, Cr.NS_OK);

  // Just in case it leaks.
  saxReader.contentHandler = null;
  saxReader.errorHandler   = null;

  return parseErrorLog;
}

function do_check_true_with_dump(aCondition, aParseLog) {
  if (!aCondition) {
    dump(aParseLog.join("\n"));
  }
  Assert.ok(aCondition);
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
