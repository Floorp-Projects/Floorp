/* -*- Mode: Java -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corp. Portions created by Netscape Communications
 * Corp. are Copyright (C) 1999 Netscape Communications Corp. All
 * Rights Reserved.
 * 
 * Contributor(s): Stephen Lamm <slamm@netscape.com>
 */ 

// the rdf service
var RDF = '@mozilla.org/rdf/rdf-service;1'
RDF = Components.classes[RDF].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

var NC = "http://home.netscape.com/NC-rdf#";

var sidebarObj = new Object;
var customizeObj = new Object;

function Init()
{
  customizeObj.id = window.arguments[0];
  customizeObj.url = window.arguments[1];
  sidebarObj.datasource_uri = window.arguments[2];
  sidebarObj.resource = window.arguments[3];

  sidebarObj.datasource = RDF.GetDataSource(sidebarObj.datasource_uri);

  var customize_frame = document.getElementById('customize_frame');
  customize_frame.setAttribute('src', customizeObj.url);
}

// Use an assertion to pass a "refresh" event to all the sidebars.
// They use observers to watch for this assertion (in sidebarOverlay.js).
function RefreshPanel() {
  var sb_resource = RDF.GetResource(sidebarObj.resource);
  var refresh_resource = RDF.GetResource(NC + "refresh_panel");
  var panel_resource = RDF.GetLiteral(customizeObj.id);

  sidebarObj.datasource.Assert(sb_resource,
                               refresh_resource,
                               panel_resource,
                               true);
  sidebarObj.datasource.Unassert(sb_resource,
                                 refresh_resource,
                                 panel_resource);
}

