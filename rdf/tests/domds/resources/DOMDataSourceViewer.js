/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

var RDF = Components.classes['@mozilla.org/rdf/rdf-service;1'].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

var domds;

function loadUrl() {
    var urlwidget = document.getElementById("url");
    
    if (!urlwidget)
        url = "resource:/res/samples/rdf/domviewer.html";
    else
        url = urlwidget.value;
    
    dump("Loading " + url + " into " + parent.window.frames["srcdoc"] +"\n");

    var win = window.frames["srcdoc"];
    win.location=url;

}


function setMode(mode, active) {
    
    domds.setMode(mode, active);
}

function refreshTree() {

    dump("refresh..\n");
    var ds = RDF.GetDataSource("rdf:domds");
    domds = ds.QueryInterface(Components.interfaces.nsIDOMDataSource);
    var win = window.frames["srcdoc"];
    domds.setWindow(win);

    var tree = document.getElementById("dataSourceTree");
    tree.clearItemSelection();
    tree.setAttribute("ref","NC:DOMRoot");
}

function onSrcLoaded() {
    var ds = RDF.GetDataSource("rdf:domds");
    domds = ds.QueryInterface(Components.interfaces.nsIDOMDataSource);
}
