prefix = "";

function startElement(name, attr) {
    print(prefix + 'starting "' + name + '"')
    prefix += "  ";
    for (i in attr) {
	print(prefix + i + '=' + attr[i]);
    }
}

function endElement(name) {
    prefix = prefix.substring(2);
    print(prefix + 'ending "' + name + '"');
}

function characterData(cdata) {
    print(prefix + 'CDATA: "' + cdata + '"');
}

function processingInstruction(target, data) {
    print(prefix + 'processing insn for "' + target + '": "' + data + '"');
}

xml = new XMLParser();
xml.startElement = startElement;
xml.endElement = endElement;
xml.characterData = null; // characterData;

xml.parseFile("moz.rdf");
