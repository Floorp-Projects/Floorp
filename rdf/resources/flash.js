// -*- Mode: Java -*-

// the rdf service
var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

// the current profile directory
// XXX obviously, this shouldn't be hard-coded
var profiledir = 'resource:/res/rdf/';

// the location of the flash registry.
var flashdb = profiledir + 'flash-registry.rdf';

function Init()
{
    // Initialize the Flash panel.
    dump('Init!\n');

    var tree = document.getElementById('tree');

    // Install all the datasources named in the Flash Registry into
    // the tree control. Datasources are listed as members of the
    // NC:FlashDataSources sequence, and are loaded in the order that
    // they appear in that sequence.
    var registry = RDF.GetDataSource(flashdb);

    // Create a 'container' wrapper around the NC:FlashDataSources
    // resource so we can use some utility routines that make access a
    // bit easier.
    var flashdatasources = Components.classes['component://netscape/rdf/container'].createInstance();
    flashdatasources = flashdatasources.QueryInterface(Components.interfaces.nsIRDFContainer);

    flashdatasources.Init(registry, RDF.GetResource('NC:FlashDataSources'));

    // Now enumerate all of the flash datasources.
    var enumerator = flashdatasources.GetElements();
    while (enumerator.HasMoreElements()) {
        var service = enumerator.GetNext();
        service = service.QueryInterface(Components.interfaces.nsIRDFResource);

        dump('adding "' + service.Value + '" to the tree\n');
        try {
            // create a new RDF/XML datasource. This will,
            // unfortunately, force it to be read synchronously the
            // first time around.
            var flashservice = RDF.GetDataSource(service.Value);
            flashservice = flashservice.QueryInterface(Components.interfaces.nsIRDFXMLDataSource);
            // Add it to the tree control's composite datasource.
            tree.database.AddDataSource(flashservice);

            var pollInterval =
                registry.GetTarget(service,
                                   RDF.GetResource('http://home.netscape.com/NC-rdf#poll-interval'),
                                   true);

            dump('poll interval = "' + pollInterval + '"\n');
            if (pollInterval)
                pollInterval = pollInterval.QueryInterface(Components.interfaces.nsIRDFLiteral);

            if (pollInterval)
                pollInterval = pollInterval.Value;

            if (pollInterval) {
                dump(service.Value + ': setting poll interval to ' + pollInterval + 'sec.\n');
                Schedule(service.Value, pollInterval);
            }
        }
        catch (ex) {
            dump('an exception occurred trying to load "' + service.Value + '":\n');
            dump(ex + '\n');
        }
    }

    // Install all of the stylesheets in the Flash Registry into the
    // panel.

    // TODO

    // XXX hack to force the tree to rebuild
    var treebody = document.getElementById('NC:FlashRoot');
    treebody.setAttribute('id', 'NC:FlashRoot');
}


function Reload(url, pollInterval)
{
    // Reload the specified datasource and reschedule.
    dump('Reload(' + url + ', ' + pollInterval + ')\n');

    var datasource = RDF.GetDataSource(url);
    datasource = datasource.QueryInterface(Components.interfaces.nsIRDFXMLDataSource);

    // Reload, asynchronously.
    datasource.Open(false);

    // Reschedule
    Schedule(url, pollInterval);
}

function Schedule(url, pollInterval)
{
    setTimeout('Reload("' + url + '", ' + pollInterval + ')', pollInterval * 1000);
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

setTimeout('Boot()', 0);
