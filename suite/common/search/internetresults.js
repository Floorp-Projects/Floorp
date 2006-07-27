/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Robert John Churchill.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): 
 *    Robert John Churchill   <rjc@netscape.com> (Original Author)
 *    Ben Goodger             <ben@netscape.com>
 *    Daniel Matejka          <danm@netscape.com>
 *    Eric Pollmann           <pollmann@netscape.com>
 *    Ray Whitmer             <rayw@netscape.com>
 *    Peter Annema            <disttsc@bart.nl>
 *    Blake Ross              <blakeross@telocity.com>
 *    Joe Hewitt              <hewitt@netscape.com>
 *    Jan Varga               <varga@utcruk.sk>
 *    Karsten Duesterloh      <kd-moz@tprac.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

function searchResultsOpenURL(event)
{
  var tree = document.getElementById("resultsList");
  var node = tree.contentView.getItemAtIndex(tree.currentIndex);
  
  var url = node.id;
  var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
  if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
  if (rdf)
  {
    var ds = rdf.GetDataSource("rdf:internetsearch");
    if (ds)
    {
      var src = rdf.GetResource(url, true);
      var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
      var target = ds.GetTarget(src, prop, true);
      if (target) target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
      if (target) target = target.Value;
      if (target) url = target;
    }
  }

  // Ignore "NC:" urls.
  if (url.substring(0, 3) == "NC:")
    return false;

  if ("loadURI" in top)
    top.loadURI(url);
  else
    top.content.location.href = url;

  return true;
}


// Workaround for bug 196057 (double onload event): accept only the first onload event
// ( This workaround will fix bug 147068 (duplicate search results).
//   Without this fix, xpfe\components\search\src\nsInternetSearchService.cpp will
//   crash when removing the same tree node twice. )
// If bug 196057 should be fixed eventually, this workaround does no harm -
// nevertheless it should be removed then
var gbProcessOnloadEvent = true;

function onLoadInternetResults()
{
  if (gbProcessOnloadEvent)
  { // forbid other onload events
    gbProcessOnloadEvent = false;

    // clear any previous results on load
    var iSearch = Components.classes["@mozilla.org/rdf/datasource;1?name=internetsearch"]
                            .getService(Components.interfaces.nsIInternetSearchService);
    iSearch.ClearResultSearchSites();

    // the search URI is passed in as a parameter, so get it and them root the results list
    var searchURI = top.content.location.href;
    if (searchURI) {
      const lastSearchURIPref = "browser.search.lastMultipleSearchURI";
      var offset = searchURI.indexOf("?");
      if (offset > 0) {
        nsPreferences.setUnicharPref(lastSearchURIPref, searchURI); // evil
        searchURI = searchURI.substr(offset+1);
        loadResultsList(searchURI);
      }
      else {
        searchURI = nsPreferences.copyUnicharPref(lastSearchURIPref, "");
        offset = searchURI.indexOf("?");
        if (offset > 0) {
          nsPreferences.setUnicharPref(lastSearchURIPref, searchURI); // evil
          searchURI = searchURI.substr(offset+1);
          loadResultsList(searchURI);
        }
      }
    }
  }
  return true;
}

function loadResultsList( aSearchURL )
{
  var resultsTree = document.getElementById( "resultsList" );
  if (!resultsTree) return false;
  resultsTree.setAttribute("ref", decodeURI(aSearchURL));
  return true;
}



function doEngineClick( event, aNode )
{
  event.target.checked = true;

  var html = null;

  var resultsTree = document.getElementById("resultsList");
  var contentArea = document.getElementById("content");
  var splitter = document.getElementById("results-splitter");
  var engineURI = aNode.id;
  if (engineURI == "allEngines") {
    resultsTree.removeAttribute("hidden");
    splitter.removeAttribute("hidden");
  }
  else
  {
    resultsTree.setAttribute("hidden", "true");
    splitter.setAttribute("hidden", "true");
    try
    {
      var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
      if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
      if (rdf)
      {
        var internetSearchStore = rdf.GetDataSource("rdf:internetsearch");
        if (internetSearchStore)
        {
          var src = rdf.GetResource(engineURI, true);
          var htmlProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#HTML", true);
          html = internetSearchStore.GetTarget(src, htmlProperty, true);
          if ( html ) html = html.QueryInterface(Components.interfaces.nsIRDFLiteral);
          if ( html ) html = html.Value;
        }
      }
    }
    catch(ex)
    {
    }
  }

  if ( html )
  {
    var doc = frames[0].document;
    if (doc)
    {
        doc.open("text/html", "replace");
        doc.writeln( html );
        doc.close();
    }
  }
  else
    frames[0].document.location = "chrome://communicator/locale/search/default.htm";
}



function doResultClick(node)
{
  var theID = node.id;
  if (!theID) return(false);

  try
  {
    var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
    if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
    if (rdf)
    {
      var internetSearchStore = rdf.GetDataSource("rdf:internetsearch");
      if (internetSearchStore)
      {
        var src = rdf.GetResource(theID, true);
        var urlProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
        var bannerProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#Banner", true);
        var htmlProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#HTML", true);

        var url = internetSearchStore.GetTarget(src, urlProperty, true);
        if (url)  url = url.QueryInterface(Components.interfaces.nsIRDFLiteral);
        if (url)  url = url.Value;
        if (url)
        {
          var statusNode = document.getElementById("status-button");
          if (statusNode)
          {
            statusNode.label = url;
          }
        }

        var banner = internetSearchStore.GetTarget(src, bannerProperty, true);
        if (banner) banner = banner.QueryInterface(Components.interfaces.nsIRDFLiteral);
        if (banner) banner = banner.Value;

        var target = internetSearchStore.GetTarget(src, htmlProperty, true);
        if (target) target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
        if (target) target = target.Value;
        if (target)
        {
          var text = "<HTML><HEAD><TITLE>Search</TITLE><BASE TARGET='_top'></HEAD><BODY><FONT POINT-SIZE='9'>";

          if (banner)
            text += banner + "</A><BR>"; // add a </A> and a <BR> just in case
          text += target;
          text += "</FONT></BODY></HTML>"

          var doc = frames[0].document;
          doc.open("text/html", "replace");
          doc.writeln(text);
          doc.close();
        }
      }
    }
  }
  catch(ex)
  {
  }
  return(true);
}

function listSelect(event)
{
  var tree = document.getElementById("resultsList");
  if (tree.view.selection.count != 1)
    return false;
  var selection = tree.contentView.getItemAtIndex(tree.currentIndex);
  return doResultClick(selection);
}

function listClick(event)
{ // left double click opens URL
  if (event.detail == 2 && event.button == 0)
    searchResultsOpenURL(event);
  return true; // always allow further click processing
}
