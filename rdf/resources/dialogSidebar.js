// -*- Mode: Java -*-

// the rdf service
var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

// the current profile directory
// XXX obviously, this shouldn't be hard-coded
var profiledir = 'resource:/res/rdf/';


// the location of the flash registry.
var sidebardb = profiledir + 'sidebar-browser.rdf';
//var sidebardb = 'file:///C:/matt/rdf/sidebar-browser.rdf';
var sidebar_resource = 'NC:BrowserSidebarRoot';

function dumpTree(node, depth) {
  var inde
  nt = "| | | | | | | | | | | | | | | | | | | | | | | | | | | | | + ";
  var kids = node.childNodes;
  dump(indent.substr(indent.length - depth*2));

  // Print your favorite attributes here
  dump(node.nodeName)
  dump(" "+node.getAttribute('id'));
  dump("\n");

  for (var ii=0; ii < kids.length; ii++) {
    dumpTree(kids[ii], depth + 1);
  }
}

function Init(sidebardb, sidebar_resource)
{
 dump("here we go \n");
  // Initialize the Sidebar

  // Install all the datasources named in the Flash Registry into
  // the tree control. Datasources are listed as members of the
  // NC:FlashDataSources sequence, and are loaded in the order that
  // they appear in that sequence.
  var registry = RDF.GetDataSource(sidebardb);

  // Create a 'container' wrapper around the sidebar_resources
  // resource so we can use some utility routines that make access a
  // bit easier.
  var sb_datasource = Components.classes['component://netscape/rdf/container'].createInstance();
  sb_datasource = sb_datasource.QueryInterface(Components.interfaces.nsIRDFContainer);

  sb_datasource.Init(registry, RDF.GetResource(sidebar_resource));
  
  var sideoption = document.getElementById('selectList');

  // Now enumerate all of the flash datasources.
  dump(sb_datasource.GetElements() + '\n');
  var enumerator = sb_datasource.GetElements();
  var count = 0;
  var countTotal = sb_datasource.GetCount();
  dump("Start Get:" + countTotal + "\n");
  while (enumerator.HasMoreElements()) {
    count = ++count;
	dump(count + "\n");
    var service = enumerator.GetNext();
    service = service.QueryInterface(Components.interfaces.nsIRDFResource);

    var new_option = createOption(registry, service);
    if (new_option) {
      sideoption.appendChild(new_option);
    }
  }
}

function createOption(registry, service) {
  
  var option_title = getAttr(registry, service, 'title');
  var option_customize = getAttr(registry, service, 'customize');
  var option_content   = getAttr(registry, service, 'content');

  dump(option_title + "\n");
  
  var optionSelect = createOptionTitle(option_title);
  var option = document.createElement('html:option');
  dump(option);
  
  option.setAttribute('title', option_title);
  option.setAttribute('customize', option_customize);
  option.setAttribute('content', option_content);
  
 
  option.appendChild(optionSelect);

  return option;
}


function createOptionTitle(titletext)
{
 dump('create optionText');
  var title = document.createElement('html:option');
  var textOption = document.createTextNode(titletext);
  //should be able to use title.add actaully
  title.appendChild(textOption);

  return textOption;
}

function getAttr(registry,service,attr_name) {
  var attr = registry.GetTarget(service,
           RDF.GetResource('http://home.netscape.com/NC-rdf#' + attr_name),
           true);
  if (attr)
    attr = attr.QueryInterface(
          Components.interfaces.nsIRDFLiteral);
  if (attr)
      attr = attr.Value;
  return attr;
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

function moveUp() {
    var list = document.getElementById('selectList'); 
	var listSelect = list.selectedIndex;
	
	if (list.selectedIndex != -1) {
		var listOption = list.childNodes.item(listSelect).cloneNode(true);
		var listOptionBefore = list.childNodes.item(listSelect-1);	
		list.remove(listSelect);
		list.insertBefore(listOption, listOptionBefore);
		dump("\n" + listOption + "\n");
	}
	 
}
   
function moveDown() {
    var list = document.getElementById('selectList');	
	var listSelect = list.selectedIndex;
	dump("list\n" + listSelect);	
	if (list.selectedIndex != -1) {
		var listOption = list.childNodes.item(listSelect);
		var listOptionBefore = list.childNodes.item(listSelect+1).cloneNode(true);
		list.remove(listSelect+1);
		list.insertBefore(listOptionBefore, listOption);
		dump("\n" + listOption + "\n");
	 }
}

function deleteOption()
{
	var list = document.getElementById('selectList');	
	var listSelect = list.selectedIndex;
	if (list.selectedIndex != -1) {
    var list = document.getElementById('selectList');
    var listSelect = list.selectedIndex;
	list.remove(listSelect);
	}
}

function DumpIt() {
	var list = document.getElementById('selectList'); 
	var listLen = list.childNodes.length;
	
    for (var i=0;i<listLen; ++i) {
	   dump('length:' + listLen + '\n');
	   dump(list.childNodes.item(i).getAttribute('title') + '\n');

	   writeRDF(list.childNodes.item(i).getAttribute('title'),list.childNodes.item(i).getAttribute('content'),list.childNodes.item(i).getAttribute('customize'),0);
	   }
}

function save() {
	self.close();
	}

// Note that there is a bug with resource: URLs right now.
var FileURL = "file:///C:/matt/rdf/sidebar-browser.rdf";

// var the "NC" namespace. Used to construct resources
var NC = "http://home.netscape.com/NC-rdf#";

function writeRDF(title,content,customize,append)
{
    // Get the RDF service
    var RDF = Components.classes["component://netscape/rdf/rdf-service"].getService();
    RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
    dump("RDF = " + RDF + "\n");

    // Open the RDF file synchronously. This is tricky, because
    // GetDataSource() will do it asynchronously. So, what we do is
    // this. First try to manually construct the RDF/XML datasource
    // and read it in. This might throw an exception if the datasource
    // has already been read in once. In which case, we'll just get
    // the existing datasource from the RDF service.
    var datasource;

    try {
        datasource = Components.classes["component://netscape/rdf/datasource?name=xml-datasource"].createInstance();
        datasource = datasource.QueryInterface(Components.interfaces.nsIRDFXMLDataSource);
        datasource.Init(FileURL);
        datasource.Open(true);

        dump("datasource = " + datasource + ", opened for the first time.\n");
    }
    catch (ex) {
        datasource = RDF.GetDataSource(FileURL);
        dump("datasource = " + datasource + ", using registered datasource.\n");
    }


    // Create a "container" wrapper around the "NC:BrowserSidebarRoot"
    // object. This makes it easier to manipulate the RDF:Seq correctly.
    var container = Components.classes["component://netscape/rdf/container"].createInstance();
    container = container.QueryInterface(Components.interfaces.nsIRDFContainer);

    container.Init(datasource, RDF.GetResource("NC:BrowserSidebarRoot"));
    dump("initialized container " + container + " on NC:BrowserSidebarRoot\n");

    // Now append a new resource to it. The resource will have the URI
    // "file:///D:/tmp/container.rdf#3".
    var count = container.GetCount();
    dump("container has " + count + " elements\n");

    var element = RDF.GetResource(FileURL + "#" + count);
	dump(FileURL + "#" + count + "\n");
	
  if (append == 0 ) {
	container.AppendElement(element);
	//container.RemoveElement(element,true);
    dump("appended " + element + " to the container\n");

    // Now make some sidebar-ish assertions about it...
    datasource.Assert(element,
                      RDF.GetResource(NC + "title"),
                      RDF.GetLiteral(title + ' ' + count),
                      true);

    datasource.Assert(element,
                      RDF.GetResource(NC + "content"),
                      RDF.GetLiteral(content),
                      true);

    datasource.Assert(element,
                      RDF.GetResource(NC + "customize"),
                      RDF.GetLiteral(customize),
                      true);

    dump("added assertions about " + element + "\n");
   }  else {
    var element = 'file://C:/matt/rdf/sidebar-browser.rdf#bookmarks';
	dump(element);
     container.RemoveElement(element,true);
	 }

    // Now serialize it back to disk
    datasource.Flush();
    dump("wrote " + FileURL + " back to disk.\n");
}


// To get around "window.onload" not working in viewer.


function Boot()
{
    var root = document.documentElement;
	dump("booting \n");
    if (root == null) {
        setTimeout(Boot, 0);
    }
    else {
        Init(sidebardb, sidebar_resource);
    }
}

setTimeout('Boot()', 0);
dump("finished\n");
