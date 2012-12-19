function noop() {}

function run_test() {
  var evts;

  var contentHandler = {
    attrs: null,
    startDocument: function() {
      evts.push("startDocument");
    },
    endDocument: noop,

    startElement: function startElement() {
      evts.push("startElement");
    },

    endElement: noop,
    characters: noop,
    processingInstruction: noop,
    ignorableWhitespace: noop,
    startPrefixMapping: noop,
    endPrefixMapping: noop
  };

  function XMLDeclHandler(version, encoding, standalone) {
    evts.splice(evts.length, 0, version, encoding, standalone);
  }

  const nsISAXXMLReader = Components.interfaces.nsISAXXMLReader;
  var saxReader = Components.classes["@mozilla.org/saxparser/xmlreader;1"]
                            .createInstance(nsISAXXMLReader);
  saxReader.contentHandler = contentHandler;
  saxReader.declarationHandler = XMLDeclHandler;

  evts = [];
  saxReader.parseFromString("<root/>", "application/xml");
  do_check_eq(evts.length, 2);
  do_check_eq(evts[0], "startDocument");
  do_check_eq(evts[1], "startElement");

  evts = [];
  saxReader.parseFromString("<?xml version='1.0'?><root/>", "application/xml");
  do_check_eq(evts.length, 5);
  do_check_eq(evts[0], "startDocument");
  do_check_eq(evts[1], "1.0");
  do_check_eq(evts[2], "");
  do_check_false(evts[3]);
  do_check_eq(evts[4], "startElement");

  evts = [];
  saxReader.parseFromString("<?xml version='1.0' encoding='UTF-8'?><root/>", "application/xml");
  do_check_eq(evts.length, 5);
  do_check_eq(evts[0], "startDocument");
  do_check_eq(evts[1], "1.0");
  do_check_eq(evts[2], "UTF-8");
  do_check_false(evts[3]);
  do_check_eq(evts[4], "startElement");

  evts = [];
  saxReader.parseFromString("<?xml version='1.0' standalone='yes'?><root/>", "application/xml");
  do_check_eq(evts.length, 5);
  do_check_eq(evts[0], "startDocument");
  do_check_eq(evts[1], "1.0");
  do_check_eq(evts[2], "");
  do_check_true(evts[3]);
  do_check_eq(evts[4], "startElement");

  evts = [];
  saxReader.parseFromString("<?xml version='1.0' encoding='UTF-8' standalone='yes'?><root/>", "application/xml");
  do_check_eq(evts.length, 5);
  do_check_eq(evts[0], "startDocument");
  do_check_eq(evts[1], "1.0");
  do_check_eq(evts[2], "UTF-8");
  do_check_true(evts[3]);
  do_check_eq(evts[4], "startElement");

  evts = [];
  // Not well-formed
  saxReader.parseFromString("<?xml encoding='UTF-8'?><root/>", "application/xml");
  do_check_eq(evts.length, 1);
  do_check_eq(evts[0], "startDocument");  
}
