
var gDatasourceName = "";
var gMatchName = "";
var gMethodName = "";
var gTextName = "";

function doFind()
{
  gDatasourceName = "";
  gMatchName = "";
  gMethodName = "";
  gTextName = "";

  // get RDF datasource to query
  var datasourceNode = document.getElementById("datasource");
  var dataSource = datasourceNode.selectedItem.value;
  gDataSourceName = datasourceNode.selectedItem.label;
  dump("Datasource: " + gDatasourceName + "\n");

  // get match
  var matchNode = document.getElementById("match");
  var match = matchNode.selectedItem.value;
  gMatchName = matchNode.selectedItem.label;
  dump("Match: " + gMatchName + "\n");

  // get method
  var methodNode = document.getElementById("method");
  var method = methodNode.selectedItem.value;
  gMethodName = methodNode.selectedItem.label;
  dump("Method: " + method + "\n");

  // get user text to find
  var textNode = document.getElementById("findtext");
  if (!textNode.value) return false;
  gTextName = textNode.value;
  dump("Find text: " + text + "\n");

  // construct find URL
  var url = "find:datasource=" + datasource;
  url += "&match=" + match;
  url += "&method=" + method;
  url += "&text=" + text;
  dump("Find URL: " + url + "\n");

  // load find URL into results pane
  var resultsTree = parent.frames[1].document.getElementById("findresultstree");
  if (!resultsTree) return false;
  resultsTree.setAttribute("ref", url);

  // enable "Save Search" button
  var searchButton = document.getElementById("SaveSearch");
  if (searchButton)
    searchButton.removeAttribute("disabled");

  dump("doFind done.\n");
  return(true);
}



function saveSearch()
{
  var resultsTree = parent.frames[1].document.getElementById("findresultstree");
  if (!resultsTree) return(false);
  var searchURL = resultsTree.getAttribute("ref");
  if ((!searchURL) || (searchURL == ""))    return(false);

  dump("Bookmark search URL: " + searchURL + "\n");
  var searchTitle = "Search: " + gMatchName + " " + gMethodName + " '" + gTextName + "' in " + gDatasourceName;
  dump("Title: " + searchTitle + "\n\n");

  var bmks = Components.classes["@mozilla.org/browser/bookmarks-service;1"].getService();
  if (bmks) bmks = bmks.QueryInterface(Components.interfaces.nsIBookmarksService);
  if (bmks) bmks.addBookmarkImmediately(searchURL, searchTitle, bmks.BOOKMARK_FIND_TYPE, null);

  return(true);
}
