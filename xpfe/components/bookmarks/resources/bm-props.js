/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

  Script for the bookmarks properties window

*/

var NC_NAMESPACE_URI = "http://home.netscape.com/NC-rdf#";

// XXX MAKE SURE that the "url" field is LAST!
// This is important for what happens if/when the URL itself is changed.
// Ask rjc@netscape.com if you want to know why exactly this is.

// This is the set of fields that are visible in the window.
var Fields     = ["name", "shortcut", "description", "url"];

// ...and this is a parallel array that contains the RDF properties
// that they are associated with.
var Properties = [NC_NAMESPACE_URI + "Name",
                  NC_NAMESPACE_URI + "ShortcutURL",
                  NC_NAMESPACE_URI + "Description",
                  NC_NAMESPACE_URI + "URL"];

var RDF = Components.classes["component://netscape/rdf/rdf-service"].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

var RDFC = Components.classes["component://netscape/rdf/container-utils"].getService();
RDFC = RDFC.QueryInterface(Components.interfaces.nsIRDFContainerUtils);

var Bookmarks = RDF.GetDataSource("rdf:bookmarks");


function Init()
{
  // Initialize the properties panel by copying the values from the
  // RDF graph into the fields on screen.

  for (var i = 0; i < Fields.length; ++i) {
    var field = document.getElementById(Fields[i]);

    var value = Bookmarks.GetTarget(RDF.GetResource(BookmarkURL),
                                    RDF.GetResource(Properties[i]),
                                    true);

    if (value) value = value.QueryInterface(Components.interfaces.nsIRDFLiteral);
    if (value) value = value.Value;

    dump("field '" + Fields[i] + "' <== '" + value + "'\n");

    if (value) field.value = value;
  }

  if (RDFC.IsContainer(Bookmarks, RDF.GetResource(BookmarkURL))) {
    // If it's a folder, it has no URL.
    dump("disabling url field for folder\n");
    document.getElementById("url").disabled = true;
    // If it's a folder, it has no Shortcut URL.
    dump("disabling shortcut url field for folder\n");
    document.getElementById("shortcut").disabled = true;
  }
}


function Commit() {
  // Grovel through the fields to see if any of the values have
  // changed. If so, update the RDF graph and force them to be saved
  // to disk.
  var changed = false;

  for (var i = 0; i < Fields.length; ++i) {
    var field = document.getElementById(Fields[i]);

    // Get the new value as a literal, using 'null' if the value is
    // empty.
    var newvalue = field.value;
    dump("field value = " + newvalue + "\n");

    newvalue = (newvalue != '') ? RDF.GetLiteral(newvalue) : null;

    var oldvalue = Bookmarks.GetTarget(RDF.GetResource(BookmarkURL),
                                       RDF.GetResource(Properties[i]),
                                       true);

    if (oldvalue) oldvalue = oldvalue.QueryInterface(Components.interfaces.nsIRDFLiteral);

    if (oldvalue != newvalue) {
      dump("replacing value for " + Fields[i] + "\n");
      dump("  oldvalue = " + oldvalue + "\n");
      dump("  newvalue = " + newvalue + "\n");

      if (oldvalue && !newvalue) {
        Bookmarks.Unassert(RDF.GetResource(BookmarkURL),
                           RDF.GetResource(Properties[i]),
                           oldvalue);
      }
      else if (!oldvalue && newvalue) {
        Bookmarks.Assert(RDF.GetResource(BookmarkURL),
                         RDF.GetResource(Properties[i]),
                         newvalue,
                         true);
      }
      else if (oldvalue && newvalue) {
        Bookmarks.Change(RDF.GetResource(BookmarkURL),
                         RDF.GetResource(Properties[i]),
                         oldvalue,
                         newvalue);
      }

      changed = true;
    }
  }

  if (changed) {
    dump("re-writing bookmarks.html\n");
    var remote = Bookmarks.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
    remote.Flush();
  }

  window.close();
}

function Cancel() {
  // Ignore any changes.
  window.close();
}
