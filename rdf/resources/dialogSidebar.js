// -*- Mode: Java -*-

// the rdf service
var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

var sidebar = new Object;

function Init()
{
  sidebar.db       = window.arguments[0];
  sidebar.resource = window.arguments[1];

 dump("Sidebar Customize Init("+sidebar.db+",\n                       "+sidebar.resource+")\n");
  var registry;
  try {
    // First try to construct a new one and load it
    // synchronously. nsIRDFService::GetDataSource() loads RDF/XML
    // asynchronously by default.
    registry = Components.classes['component://netscape/rdf/datasource?name=xml-datasource'].createInstance();
    registry = registry.QueryInterface(Components.interfaces.nsIRDFDataSource);

    var remote = registry.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
    remote.Init(sidebar.db); // this will throw if it's already been opened and registered.

    // read it in synchronously.
    remote.Refresh(true);
  }
  catch (ex) {
    // if we get here, then the RDF/XML has been opened and read
    // once. We just need to grab the datasource.
    registry = RDF.GetDataSource(sidebar.db);
  }

  // Create a 'container' wrapper around the sidebar.resources
  // resource so we can use some utility routines that make access a
  // bit easier.
  var sb_datasource = Components.classes['component://netscape/rdf/container'].createInstance();
  sb_datasource = sb_datasource.QueryInterface(Components.interfaces.nsIRDFContainer);
  sb_datasource.Init(registry, RDF.GetResource(sidebar.resource));
  
  var sideoption = document.getElementById('selectList');

  // Now enumerate all of the flash datasources.
  var enumerator = sb_datasource.GetElements();
  var count = 0;
  while (enumerator.HasMoreElements()) {
    count = ++count;
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
