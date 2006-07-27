/* -*- Mode: Java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Robert John Churchill    <rjc@netscape.com>
 *    Blake Ross <BlakeR1234@aol.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */



// global(s)
var bundle = srGetStrBundle("chrome://communicator/locale/search/search-editor.properties");
var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);
var pref = null;
var RDF = null;
var RDFC = null;
var RDFCUtils = null;
var catDS = null;
var internetSearchDS = null;

try
{
  pref = Components.classes["@mozilla.org/preferences;1"].getService();
  if (pref) pref = pref.QueryInterface( Components.interfaces.nsIPref );

  RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
  if (RDF)  RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

  RDFC = Components.classes["@mozilla.org/rdf/container;1"].getService();
  if (RDFC) RDFC = RDFC.QueryInterface(Components.interfaces.nsIRDFContainer);

  RDFCUtils = Components.classes["@mozilla.org/rdf/container-utils;1"].getService();
  if (RDFCUtils)  RDFCUtils = RDFCUtils.QueryInterface(Components.interfaces.nsIRDFContainerUtils);
}
catch(e)
{
  pref = null;
  RDF = null;
  RDFC = null;
  RDFCUtils = null;
}



function debug(msg)
{
  // uncomment for noise
  // dump(msg+"\n");
}



function doLoad()
{
  doSetOKCancel(Commit);

  // adjust category popup
  var internetSearch = Components.classes["@mozilla.org/rdf/datasource;1?name=internetsearch"].getService();
  if (internetSearch) internetSearch = internetSearch.QueryInterface(Components.interfaces.nsIInternetSearchService);
  if (internetSearch)
  {
    internetSearchDS = internetSearch.QueryInterface(Components.interfaces.nsIRDFDataSource);

    catDS = internetSearch.GetCategoryDataSource();
    if (catDS)  catDS = catDS.QueryInterface(Components.interfaces.nsIRDFDataSource);
    if (catDS)
    {
      var categoryList = document.getElementById("categoryList");
      if (categoryList)
      {
        categoryList.database.AddDataSource(catDS);
        var ref = categoryList.getAttribute("ref");
        if (ref)  categoryList.setAttribute("ref", ref);
      }
      var engineList = document.getElementById("engineList");
      if (engineList)
      {
        engineList.database.AddDataSource(catDS);
      }
    }
  }

  // try and determine last category name used
  var lastCategoryName = "";
  try
  {
    var pref = Components.classes["@mozilla.org/preferences;1"].getService();
    if (pref) pref = pref.QueryInterface( Components.interfaces.nsIPref );
    if (pref) lastCategoryName = pref.CopyCharPref( "browser.search.last_search_category" );

    if (lastCategoryName != "")
    {
      // strip off the prefix if necessary
      var prefix="NC:SearchCategory?category=";
      if (lastCategoryName.indexOf(prefix) == 0)
      {
        lastCategoryName = lastCategoryName.substr(prefix.length);
      }
    }

  }
  catch( e )
  {
    debug("Exception in SearchPanelStartup\n");
    lastCategoryName = "";
  }
  debug("\nSearchPanelStartup: lastCategoryName = '" + lastCategoryName + "'\n");

  // select the appropriate category
  var categoryList = document.getElementById( "categoryList" );
  var categoryPopup = document.getElementById( "categoryPopup" );
  if( categoryList && categoryPopup )
  {
    var found = false;
    for( var i = 0; i < categoryPopup.childNodes.length; i++ )
    {
      if( ( lastCategoryName == "" && categoryPopup.childNodes[i].getAttribute("value") == "NC:SearchEngineRoot" ) ||
          ( categoryPopup.childNodes[i].getAttribute("id") == lastCategoryName ) )
      {
        categoryList.selectedItem = categoryPopup.childNodes[i];
        found = true;
        break;
      }
    }
    if (categoryPopup.childNodes.length > 0)
    {
      if (found == false)
      {
        categoryList.selectedItem = categoryPopup.childNodes[0];
      }
      chooseCategory(categoryList.selectedItem);
    }
  }

}



function Commit()
{
  // flush
  if (catDS)
  {
    var flushableDS = catDS.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
    if (flushableDS)
    {
      flushableDS.Flush();
    }
  }

  window.close();
}



function doUnload()
{
    // Get the current window position/size.
    var x = window.screenX;
    var y = window.screenY;
    var h = window.outerHeight;
    var w = window.outerWidth;

    // Store these into the window attributes (for persistence).
    var win = document.getElementById( "search-editor-window" );
    win.setAttribute( "x", x );
    win.setAttribute( "y", y );
    win.setAttribute( "height", h );
    win.setAttribute( "width", w );
}



/* Note: doSort() does NOT support natural order sorting, unless naturalOrderResource is valid,
         in which case we sort ascending on naturalOrderResource
 */
function doSort(sortColName, naturalOrderResource)
{
  var node = document.getElementById(sortColName);
  // determine column resource to sort on
  var sortResource = node.getAttribute('resource');
  if (!sortResource) return(false);

  var sortDirection="ascending";
  var isSortActive = node.getAttribute('sortActive');
  if (isSortActive == "true")
  {
    sortDirection = "ascending";

    var currentDirection = node.getAttribute('sortDirection');
    if (currentDirection == "ascending")
    {
      if (sortResource != naturalOrderResource)
      {
        sortDirection = "descending";
      }
    }
    else if (currentDirection == "descending")
    {
      if (naturalOrderResource != null && naturalOrderResource != "")
      {
        sortResource = naturalOrderResource;
      }
    }
  }

  var isupports = Components.classes["@mozilla.org/xul/xul-sort-service;1"].getService();
  if (!isupports)    return(false);
  var xulSortService = isupports.QueryInterface(Components.interfaces.nsIXULSortService);
  if (!xulSortService)    return(false);
  try
  {
    xulSortService.Sort(node, sortResource, sortDirection);
  }
  catch(ex)
  {
    debug("Exception calling xulSortService.Sort()");
  }
  return(true);
}



function chooseCategory( aNode )
{
  var category = "NC:SearchCategory?category=" + aNode.getAttribute("id");
  debug("chooseCategory: '" + category + "'\n");

  var treeNode = document.getElementById("engineList");
  if (treeNode)
  {
    dump("*** Set search engine list to category=" + category + "\n");
    treeNode.setAttribute( "ref", category );
    treeNode.builder.rebuild();
  }
  return(true);
}



function AddEngine()
{
  var allenginesList = document.getElementById("allengines");
  if (!allenginesList)  return(false);
  var select_list = allenginesList.selectedItems;
  if (!select_list) return(false)
  if (select_list.length < 1) return(false);

  // make sure we have at least one category to add engine into
  var categoryPopup = document.getElementById( "categoryPopup" );
  if (!categoryPopup) return(false);
  if (categoryPopup.childNodes.length < 1)
  {
    if (NewCategory() == false) return(false);
  }

  var engineList = document.getElementById("engineList");
  if (!engineList)  return(false);

  var ref = engineList.getAttribute("ref");
  if ((!ref) || (ref == ""))  return(false);

  var categoryRes = RDF.GetResource(ref);
  if (!categoryRes) return(false);

  RDFCUtils.MakeSeq(catDS, categoryRes);

  RDFC.Init(catDS, categoryRes);

  var urlRes = RDF.GetResource("http://home.netscape.com/NC-rdf#URL");
  if (!urlRes)  return(false);
  var typeRes = RDF.GetResource("http://home.netscape.com/NC-rdf#searchtype");
  if (!typeRes) return(false);
  var engineRes = RDF.GetResource("http://home.netscape.com/NC-rdf#Engine");
  if (!engineRes) return(false);

  for (var x = 0; x < select_list.length; x++)
  {
    var id = select_list[x].getAttribute("id");
    if ((!id) || (id == ""))  return(false);
    var idRes = RDF.GetResource(id);
    if (!idRes) return(false);

    // try and find/use alias to search engine
    var privateEngineFlag = internetSearchDS.HasAssertion(idRes, typeRes, engineRes, true);
    if (privateEngineFlag == true)
    {
      var node = internetSearchDS.GetTarget(idRes, urlRes, true);
      if (node) node = node.QueryInterface(Components.interfaces.nsIRDFLiteral);
      if (node)
      {
        if (node.Value)
        {
          id = node.Value;
          idRes = RDF.GetResource(id);
          if (!idRes) return(false);
        }
      }
    }

    var nodeIndex = RDFC.IndexOf(idRes);
    if (nodeIndex < 1)
    {
      RDFC.AppendElement(idRes);
    }
  }

  return(true);
}



function RemoveEngine()
{
  var engineList = document.getElementById("engineList");
  if (!engineList)  return(false);
  var select_list = engineList.selectedItems;
  if (!select_list) return(false)
  if (select_list.length < 1) return(false);

  var ref = engineList.getAttribute("ref");
  if ((!ref) || (ref == ""))  return(false);
  var categoryRes = RDF.GetResource(ref);
  if (!categoryRes) return(false);

  RDFC.Init(catDS, categoryRes);

  // Note: walk backwards to make deletion easier
  for (var x = select_list.length - 1; x >= 0; x--)
  {
    var id = select_list[x].getAttribute("id");
    if ((!id) || (id == ""))  return(false);
    var idRes = RDF.GetResource(id);
    if (!idRes) return(false);

    var nodeIndex = RDFC.IndexOf(idRes);
    if (nodeIndex > 0)
    {
      RDFC.RemoveElementAt(nodeIndex, true, idRes);
    }
  }

  return(true);
}



function MoveUp()
{
  return  MoveDelta(-1);
}



function MoveDown()
{
  return  MoveDelta(1);
}



function MoveDelta(delta)
{
  var engineList = document.getElementById("engineList");
  if (!engineList)  return(false);
  var select_list = engineList.selectedItems;
  if (!select_list) return(false)
  if (select_list.length != 1)  return(false);

  var ref = engineList.getAttribute("ref");
  if (!ref) return(false);
  var categoryRes = RDF.GetResource(ref);
  if (!categoryRes) return(false);

  var id = select_list[0].id;
  if (!id)  return(false);
  var idRes = RDF.GetResource(id);
  if (!idRes) return(false);

  RDFC.Init(catDS, categoryRes);

  var nodeIndex = RDFC.IndexOf(idRes);
  if (nodeIndex < 1)  return(false);    // how did that happen?

  var numItems = RDFC.GetCount();

  var newPos = -1;
  if (((delta == -1) && (nodeIndex > 1)) ||
    ((delta == 1) && (nodeIndex < numItems)))
  {
    newPos = nodeIndex + delta;
    RDFC.RemoveElementAt(nodeIndex, true, idRes);
  }
  if (newPos > 0)
  {
    RDFC.InsertElementAt(idRes, newPos, true);
  }

  selectItems(engineList, ref, id);

  return(true);
}

function doMoveDirectionEnabling()
{
  var tree = document.getElementById("engineList")
  var selectedItems = tree.selectedItems;
  if (!selectedItems && selectedItems.length != 1)
    return false;
  var ref = tree.getAttribute("ref");
  var categoryResource = RDF.GetResource(ref);
  var elementResource = RDF.GetResource(selectedItems[0].id);
  RDFC.Init(catDS, categoryResource);
  var nodeIndex = RDFC.IndexOf(elementResource);
  var moveUpButton = document.getElementById("up");
  var moveDownButton = document.getElementById("down");
  moveUpButton.removeAttribute("disabled");
  moveDownButton.removeAttribute("disabled");
  if (nodeIndex <= 1)
    moveUpButton.setAttribute("disabled", "true");
  if (nodeIndex >= RDFC.GetCount())
    moveDownButton.setAttribute("disabled", "true");
}


function NewCategory()
{
  var promptStr = bundle.GetStringFromName("NewCategoryPrompt");
  var newTitle = bundle.GetStringFromName("NewCategoryTitle");
  var result = {value:null};
  var confirmed = promptService.prompt(window, newTitle, promptStr, result, null, {value:0});
  if (!confirmed || (!result.value) || result.value == "")      return(false);

  var newName = RDF.GetLiteral(result.value);
  if (!newName) return(false);

  var categoryRes = RDF.GetResource("NC:SearchCategoryRoot");
  if (!categoryRes) return(false);

  RDFC.Init(catDS, categoryRes);

  var randomID = Math.random();
  var categoryID = "NC:SearchCategory?category=urn:search:category:" + randomID.toString();
  var currentCatRes = RDF.GetResource(categoryID);
  if (!currentCatRes) return(false);

  var titleRes = RDF.GetResource("http://home.netscape.com/NC-rdf#title");
  if (!titleRes)  return(false);

  // set the category's name
  catDS.Assert(currentCatRes, titleRes, newName, true);

  // and insert the category
  RDFC.AppendElement(currentCatRes);

  RDFCUtils.MakeSeq(catDS, currentCatRes);

  // try and select the new category
  var categoryList = document.getElementById( "categoryList" );
  var select_list = categoryList.getElementsByAttribute("id", categoryID);
  if (select_list && select_list.length > 0)
  {
    categoryList.selectedItem = select_list[0];
    chooseCategory(categoryList.selectedItem);
  }
  return(true);
}



function RenameCategory()
{
  // make sure we have at least one category
  var categoryPopup = document.getElementById( "categoryPopup" );
  if (!categoryPopup) return(false);
  if (categoryPopup.childNodes.length < 1)  return(false);

  var categoryList = document.getElementById( "categoryList" );
  var currentName = categoryList.selectedItem.getAttribute("label");
  var promptStr = bundle.GetStringFromName("RenameCategoryPrompt");
  var renameTitle = bundle.GetStringFromName("RenameCategoryTitle");
  var result = {value:currentName};
  var confirmed = promptService.prompt(window,renameTitle,promptStr,result,null,{value:0});
  if (!confirmed || (!result.value) || (result.value == "") || result.value == currentName)     return(false);

  var currentCatID = categoryList.selectedItem.getAttribute("id");
  var currentCatRes = RDF.GetResource(currentCatID);
  if (!currentCatRes) return(false);

  var titleRes = RDF.GetResource("http://home.netscape.com/NC-rdf#title");
  if (!titleRes)  return(false);

  var newName = RDF.GetLiteral(result.value);
  if (!newName) return(false);

  var oldName = catDS.GetTarget(currentCatRes, titleRes, true);
  if (oldName)  oldName = oldName.QueryInterface(Components.interfaces.nsIRDFLiteral);
  if (oldName)
  {
    catDS.Change(currentCatRes, titleRes, oldName, newName);
  }
  else
  {
    catDS.Assert(currentCatRes, titleRes, newName, true);
  }

  // force popup to update
  chooseCategory(categoryList.selectedItem);

  return(true);
}



function RemoveCategory()
{
  // make sure we have at least one category
  var categoryPopup = document.getElementById( "categoryPopup" );
  if (!categoryPopup) return(false);
  if (categoryPopup.childNodes.length < 1)  return(false);

  var promptStr = bundle.GetStringFromName("RemoveCategoryPrompt");
  if (!confirm(promptStr))  return(false);

  var categoryRes = RDF.GetResource("NC:SearchCategoryRoot");
  if (!categoryRes) return(false);

  var categoryList = document.getElementById( "categoryList" );
  if (!categoryList)  return(false);

  var idNum = categoryList.selectedIndex;

  var id = categoryList.selectedItem.getAttribute("id");
  if ((!id) || (id == ""))  return(false);

  var idRes = RDF.GetResource(id);
  if (!idRes) return(false);

  RDFC.Init(catDS, categoryRes);

  var nodeIndex = RDFC.IndexOf(idRes);
  if (nodeIndex < 1)  return(false);    // how did that happen?

  RDFC.RemoveElementAt(nodeIndex, true, idRes);

  // try and select another category
  if (idNum > 0)  --idNum;
  else    idNum = 0;

  if (categoryPopup.childNodes.length > 0)
  {
    categoryList.selectedItem = categoryPopup.childNodes[idNum];
    chooseCategory(categoryList.selectedItem);
  }
  else
  {
    // last category was deleted, so empty out engine list
    var treeNode = document.getElementById("engineList");
    if (treeNode)
    {
      treeNode.setAttribute( "ref", "" );
    }
  }
  return(true);
}



function selectItems(treeRoot, containerID, targetID)
{
  var select_list = treeRoot.getElementsByAttribute("id", targetID);
  for (var x=0; x<select_list.length; x++)
  {
    var node = select_list[x];
    if (!node)  continue;

    var parent = node.parentNode.parentNode;
    if (!parent)  continue;

    var id = parent.getAttribute("ref");
    if (!id || id=="")
    {
      id = parent.getAttribute("id");
    }
    if (!id || id=="")  continue;

    if (id == containerID)
    {
      treeRoot.selectItem(node);
      break;
    }
  }
}
