/* -*- Mode: Java; tab-width: 4; c-basic-offset: 4; -*-
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

/*

   Script for the registry viewer window

*/

var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

var Registry;
var REGISTRY_NAMESPACE_URI = 'urn:mozilla-registry:'
var REGISTRY_VALUE_PREFIX = REGISTRY_NAMESPACE_URI + 'value:';
var kRegistry_Subkeys = RDF.GetResource(REGISTRY_NAMESPACE_URI + 'subkeys');

function debug(msg)
{
    //dump(msg + '\n');
}

function OnLoad()
{
    Registry = Components.classes['component://netscape/registry-viewer'].createInstance();
    Registry = Registry.QueryInterface(Components.interfaces.nsIRegistryDataSource);

    Registry.openWellKnownRegistry(1); // application component registry

    Registry = Registry.QueryInterface(Components.interfaces.nsIRDFDataSource);

    var tree = document.getElementById('tree');
    tree.database.AddDataSource(Registry);

    tree.setAttribute('ref', 'urn:mozilla-registry:key:/');
}  

function OnSelect(event)
{
  var tree = event.target;
  var items = tree.selectedItems;

  var properties = document.getElementById('properties');
  if (properties.firstChild) {
      properties.removeChild(properties.firstChild);
  }

  if (items.length == 1) {
      // Exactly one item is selected. Show as much information as we
      // can about it.
      var table = document.createElement('html:table');

      debug('selected item = ' + items[0].getAttribute('id'));
      var uri = items[0].getAttribute('id');

      var source = RDF.GetResource(uri);
      var arcs = Registry.ArcLabelsOut(source);
      while (arcs.HasMoreElements()) {
          var property = arcs.GetNext().QueryInterface(Components.interfaces.nsIRDFResource);
          if (property == kRegistry_Subkeys)
              continue;

          var propstr = property.Value.substr(REGISTRY_VALUE_PREFIX.length);
          debug('propstr = ' + propstr);

          var target = Registry.GetTarget(source, property, true);
          var targetstr;

          var literal;
          if (literal = target.QueryInterface(Components.interfaces.nsIRDFLiteral)) {
              targetstr = literal.Value;
          }
          else if (literal = target.QueryInterface(Components.interfaces.nsIRDFInt)) {
              targetstr = literal.Value;
          }
          else {
              // Hmm. Not sure!
          }
          
          debug('targetstr = ' + targetstr);

          var tr = document.createElement('html:tr');
          table.appendChild(tr);

          var td1 = document.createElement('html:td');
          td1.appendChild(document.createTextNode(propstr));
          tr.appendChild(td1);

          var td2 = document.createElement('html:td');
          td2.appendChild(document.createTextNode(targetstr));
          tr.appendChild(td2);
      }

      properties.appendChild(table);
  }
}
