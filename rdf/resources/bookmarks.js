// -*- Mode: Java -*-

function BookmarksNewWindow()
{
  var toolkitCore = XPAppCoresManager.Find("toolkitCore");
  if (!toolkitCore) {
    toolkitCore = new ToolkitCore();
    if (toolkitCore) {
      toolkitCore.Init("toolkitCore");
    }
  }
  if (toolkitCore) {
    toolkitCore.ShowWindow("resource://res/rdf/bookmarks.xul",window);
  }
}

function BookmarkProperties()
{
  var tree = document.getElementById('bookmarksTree');
  var select_list = tree.getElementsByAttribute("selected", "true");

  if (select_list.length >= 1) {
    dump("Bookmark Properties: Have selection\n");
    var propsCore = XPAppCoresManager.Find(select_list[0].id);
    if (!propsCore) {
      dump("Bookmark Properties: no existing appcore\n");
      var propsCore = new DOMPropsCore();
      if (propsCore) {
        dump("Bookmark Properties: initing new appcore\n");
	propsCore.Init(select_list[0].id);
      } else {
        dump("Bookmark Properties: failed to create new appcore\n");
      }
    }
    if (propsCore) {
      dump("Bookmark Properties: opening new window\n");
      propsCore.showProperties("resource://res/rdf/bm-props.xul",
			       window, select_list[0]);
      return true;
    }
  } else {
    dump("nothing selected!\n"); 
  }
  return false;
}

function OpenURL(event,node)
{
  if (node.getAttribute('container') == "true") {
    return false;
  }
  url = node.getAttribute('id');

  // Ignore "NC:" urls.
  if (url.substring(0, 3) == "NC:") {
    return false;
  }

  /*window.open(url,'bookmarks');*/
  var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
  if (!toolkitCore) {
    toolkitCore = new ToolkitCore();
    if (toolkitCore) {
      toolkitCore.Init("ToolkitCore");
    }
  }
  if (toolkitCore) {
    toolkitCore.ShowWindow(url,window);
  }

  dump("OpenURL(" + url + ")\n");

  return true;
}

function doSort(sortColName)
{
  var node = document.getElementById(sortColName);
  // determine column resource to sort on
  var sortResource = node.getAttribute('resource');
  if (!node) return(false);

  var sortDirection="ascending";
  var isSortActive = node.getAttribute('sortActive');
  if (isSortActive == "true") {
    var currentDirection = node.getAttribute('sortDirection');
    if (currentDirection == "ascending")
      sortDirection = "descending";
    else if (currentDirection == "descending")
      sortDirection = "natural";
    else
      sortDirection = "ascending";
  }

  // get RDF Core service
  var rdfCore = XPAppCoresManager.Find("RDFCore");
  if (!rdfCore) {
    rdfCore = new RDFCore();
    if (!rdfCore) {
      return(false);
    }
    rdfCore.Init("RDFCore");
    XPAppCoresManager.Add(rdfCore);
  }
  // sort!!!
  rdfCore.doSort(node, sortResource, sortDirection);
  return(false);
}
