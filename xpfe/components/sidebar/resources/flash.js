/* -*- Mode: Java; tab-width: 4; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// the rdf service
var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

// the current profile directory
// XXX obviously, this shouldn't be hard-coded
var profiledir = 'chrome://sidebar/content/';

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
    var registry;
    try {
        // First try to construct a new one and load it
        // synchronously. nsIRDFService::GetDataSource() loads RDF/XML
        // asynchronously by default.
        registry = Components.classes['component://netscape/rdf/datasource?name=xml-datasource'].createInstance();
        registry = registry.QueryInterface(Components.interfaces.nsIRDFDataSource);

        var remote = registry.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
        remote.Init(flashdb); // this will throw if it's already been opened and registered.

        // read it in synchronously.
        remote.Refresh(true);
    }
    catch (ex) {
        // if we get here, then the RDF/XML has been opened and read
        // once. We just need to grab the datasource.
        registry = RDF.GetDataSource(flashdb);
    }

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
            // Create a new RDF/XML datasource. If this is a 'remote'
            // datasource (e.g., a 'http:' URL), it will be loaded
            // asynchronously.
            var flashservice = RDF.GetDataSource(service.Value);
            flashservice = flashservice.QueryInterface(Components.interfaces.nsIRDFDataSource);

            // Add it to the tree control's composite datasource.
            tree.database.AddDataSource(flashservice);

            var pollInterval =
                registry.GetTarget(service,
                                   RDF.GetResource('http://home.netscape.com/NC-rdf#poll-interval'),
                                   true);

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
    tree.setAttribute('ref', 'NC:FlashRoot');
}


function Reload(url, pollInterval)
{
    // Reload the specified datasource and reschedule.
    dump('Reload(' + url + ', ' + pollInterval + ')\n');

    var datasource = RDF.GetDataSource(url);
    datasource = datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);

    // Reload, asynchronously.
    datasource.Refresh(false);

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
