// Note that there is a bug with resource: URLs right now.

//var FileURL = "file:///C:/matt/rdf/sidebar-browser.rdf";
var FileURL = "resource:/res/rdf/sidebar-browser.rdf";
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
        dump("File:" + FileURL + " datasource = " + datasource + ", using registered datasource.\n");
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
                      RDF.GetLiteral(title),
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
    //datasource.Flush();
    dump("wrote " + FileURL + " back to disk.\n");
}




function selected(event, node)
{ 
   dump('node:\n' + node);
   selectedNode_Title = node.getAttribute('title');
   selectedNode_Content = node.getAttribute('content');
   selectedNode_Customize = node.getAttribute('customize');
}

function Addit() 
{
	if (window.frames[0].selectedNode_Title != null) {
	dump(window.frames[0].selectedNode_Title + '\n');
	createOption();
      } else {
	  dump('Nothing selected');
	  }
}

function createOption() {
	
	var selectedNode_Title = window.frames[0].selectedNode_Title;
	var selectedNode_Content = window.frames[0].selectedNode_Content;
    var selectedNode_Customize = window.frames[0].selectedNode_Customize;
    var list = parent.frames[0].document.getElementById('selectList'); 
	dump('Here' + selectedNode_Title);
	writeRDF(selectedNode_Title,selectedNode_Content,selectedNode_Customize,0)
  //dump(option_title + "\n");
   parent.frames[0].location.reload();
  
}
