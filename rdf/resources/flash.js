// -*- Mode: Java -*-

// the rdf service
var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

// the current profile directory
// XXX obviously, this shouldn't be hard-coded
profiledir = 'file:///D|/tmp/flash/';

// the location of the flash registry.
flashdb = profiledir + 'flash-registry.rdf';


function Init()
{
    dump('Init!\n');

    var tree = document.getElementById('tree');

    // Install all the datasources named in the Flash Registry into
    // the tree control. Datasources are listed as members of the
    // NC:FlashDataSources sequence, and are loaded in the order that
    // they appear in that sequence.
    var registry = RDF.GetDataSource(flashdb);

    var flashdatasources = Components.classes['component://netscape/rdf/container'].createInstance();
    flashdatasources = flashdatasources.QueryInterface(Components.interfaces.nsIRDFContainer);

    flashdatasources.Init(registry, RDF.GetResource('NC:FlashDataSources'));

    var enumerator = flashdatasources.GetElements();
    while (enumerator.HasMoreElements()) {
        var service = enumerator.GetNext();
        service = service.QueryInterface(Components.interfaces.nsIRDFResource);

        dump('adding "' + service.Value + '" to the tree\n');

        // XXX should be in a "try" block b/c this could fail if a
        // datasource URI is bogus.
        var datasource = RDF.GetDataSource(service.Value);
        tree.database.AddDataSource(datasource);
    }

    // Install all of the stylesheets in the Flash Registry into the
    // panel.

    // TODO

    // XXX hack to force the tree to rebuild
    var treebody = document.getElementById('NC:FlashRoot');
    treebody.setAttribute('id', 'NC:FlashRoot');
}

function OpenURL(node)
{
    dump("open-url(" + node + ")\n");
}


// To get around "window.onload" not working in viewer.
function Boot()
{
    var tree = document.getElementById('tree');
    if (tree == null) {
        setTimeout(Boot, 0);
    }
    else {
        Init();
    }
}

setTimeout(Boot, 0);
