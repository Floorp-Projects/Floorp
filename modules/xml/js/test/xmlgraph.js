prefix = "";

xml = new XMLGraph();
xml.parseFile("moz.rdf");

function dumpobj(o, name) {
    print(prefix + "dumping " + name);
    prefix += "  ";
    if (typeof(o) != "object") {
	print(prefix + typeof(o) + " " + o + " is not an object!\n");
	return;
    }
    for (i in o) {
	if (typeof(o[i]) == "object") 
	    dumpobj(o[i], i);
	else
	    print(prefix + i + "=" + o[i]);
    }
    prefix = prefix.substring(2);
    print(prefix + "done " + name);
}

dumpobj(xml.graph, "RDF:RDF");
