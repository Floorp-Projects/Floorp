// -*- Mode: Java -*-

// the rdf service
var RDF;
var NC = "http://home.netscape.com/NC-rdf#";

var sidebar;

function Init()
{
  RDF=Components.classes['component://netscape/rdf/rdf-service'].getService();
  RDF=RDF.QueryInterface(Components.interfaces.nsIRDFService);
 
  sidebar          = new Object;
  sidebar.db       = window.arguments[0];
  sidebar.resource = window.arguments[1];

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
  
  // Now enumerate all of the flash datasources.
  var enumerator = sb_datasource.GetElements();
  var count = 0;
  while (enumerator.HasMoreElements()) {
    count = ++count;
    var service = enumerator.GetNext();
    service = service.QueryInterface(Components.interfaces.nsIRDFResource);

    addOption(registry, service);
  }
  enableButtons();
}

function addOption(registry, service) {
  
  var option_title = getAttr(registry, service, 'title');
  var option_customize = getAttr(registry, service, 'customize');
  var option_content   = getAttr(registry, service, 'content');

  var optionSelect = createOptionTitle(option_title);
  var option = document.createElement('html:option');
  
  option.setAttribute('title', option_title);
  option.setAttribute('customize', option_customize);
  option.setAttribute('content', option_content);
  
 
  option.appendChild(optionSelect);

  var sideoption = document.getElementById('selectList');
  sideoption.appendChild(option);
}

function createOptionTitle(titletext)
{
  var title = document.createElement('html:option');
  var textOption = document.createTextNode(titletext);
  title.appendChild(textOption);

  return textOption;
}

function getAttr(registry,service,attr_name) {
  var attr = registry.GetTarget(service,
           RDF.GetResource(NC + attr_name),
           true);
  if (attr)
    attr = attr.QueryInterface(Components.interfaces.nsIRDFLiteral);
  if (attr)
      attr = attr.Value;
  return attr;
}

function selectChange() {
  enableButtons();
}

function moveUp() {
  var list = document.getElementById('selectList'); 
  var index = list.selectedIndex;
  if (index > 0) {
    var optionBefore   = list.childNodes.item(index-1);	
    var selectedOption = list.childNodes.item(index);
    list.remove(index);
    list.insertBefore(selectedOption, optionBefore);
    list.selectedIndex = index - 1;
    enableButtons();
    enableSave();
  }
}
   
function moveDown() {
  var list = document.getElementById('selectList');	
  var index = list.selectedIndex;
  if (index != -1 &&
      index != list.options.length - 1) {
    var selectedOption = list.childNodes.item(index);
    var optionAfter    = list.childNodes.item(index+1);
    list.remove(index+1);
    list.insertBefore(optionAfter, selectedOption);
    list.selectedIndex = index + 1;
    enableButtons();
    enableSave();
  }
}

function enableButtons() {
  var up        = document.getElementById('up');
  var down      = document.getElementById('down');
  var list      = document.getElementById('selectList');
  var customize = document.getElementById('customize-button');
  var index = list.selectedIndex;
  var noneSelected = (index == -1);
  var isFirst      = (index == 0);
  var isLast       = (index == list.options.length - 1);

  // up /\ button
  if (noneSelected || isFirst) {
    up.setAttribute('disabled', 'true');
  } else {
    up.setAttribute('disabled', '');
  }
  // down \/ button
  if (noneSelected || isLast) {
    down.setAttribute('disabled', 'true');
  } else {
    down.setAttribute('disabled', '');
  }
  // "Customize..." button
  var customizeURL = null;
  if (!noneSelected) {
    var option = list.childNodes.item(index);
    customizeURL = option.getAttribute('customize');
  }
  if (customizeURL == 'null') {
    customize.setAttribute('disabled','true');
  } else {
    customize.setAttribute('disabled','');
  }
}

function CustomizePanel() 
{
  var list  = document.getElementById('selectList');	
  var index = list.selectedIndex;

  if (index != -1) {
    var title         = list.childNodes.item(index).getAttribute('title');
    var customize_URL = list.childNodes.item(index).getAttribute('customize');

    if (!title || !customize_URL) return;

    var customize = window.open("chrome://sidebar/content/customize-panel.xul",
			      "PanelPreview", "chrome");

    customize.panel_name          = title;
    customize.panel_customize_URL = customize_URL;
  }
  enableSave();
}

function RemovePanel()
{
  var list  = document.getElementById('selectList');	
  var index = list.selectedIndex;
  if (index != -1) {
    // XXX prompt user
    list.options[index] = null;
  }
  enableSave();
}

// Note that there is a bug with resource: URLs right now.
var FileURL = "file:////u/slamm/tt/sidebar-browser.rdf";

// var the "NC" namespace. Used to construct resources
function Save()
{
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
    //datasource.Init(FileURL);
    datasource.Init(sidebar.db);
    datasource.Open(true);
    dump("datasource = " + datasource + ", opened for the first time.\n");
  }
  catch (ex) {
      //datasource = RDF.GetDataSource(FileURL);
    datasource = RDF.GetDataSource(sidebar.db);
    dump("datasource = " + datasource + ", using registered datasource.\n");
  }

  // Create a "container" wrapper around the "NC:BrowserSidebarRoot"
  // object. This makes it easier to manipulate the RDF:Seq correctly.
  var container = Components.classes["component://netscape/rdf/container"].createInstance();
  container = container.QueryInterface(Components.interfaces.nsIRDFContainer);

  container.Init(datasource, RDF.GetResource(sidebar.resource));
  dump("initialized container " + container + " on " + sidebar.resource+"\n");

  // Remove all the current panels
  //
  var enumerator = container.GetElements();

  while (enumerator.HasMoreElements()) {
    var service = enumerator.GetNext();
    service = service.QueryInterface(Components.interfaces.nsIRDFResource);
    container.RemoveElement(service, true);
  }

  // Add the new panel list
  //
  var count = container.GetCount();
  dump("container has " + count + " elements\n");

  var list = document.getElementById('selectList'); 
  var list_length = list.childNodes.length;
	
  for (var ii=0; ii < list_length; ii++, count++) {
    dump(list.childNodes.item(ii).getAttribute('title') + '\n');

    var title     = list.childNodes.item(ii).getAttribute('title');
    var content   = list.childNodes.item(ii).getAttribute('content');
    var customize = list.childNodes.item(ii).getAttribute('customize');

    var element = RDF.GetResource(FileURL + "#" + count);
    dump(FileURL + "#" + count + "\n");
	
    container.AppendElement(element);
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
  }

  // Now serialize it back to disk
  datasource.Flush();
  dump("wrote " + FileURL + " back to disk.\n");

  //window.close();
}

function selected()
{ 
  var add_button = document.getElementById('add_button');
  var preview_button = document.getElementById('preview_button');
  var select_list = document.getElementsByAttribute("selected", "true");
  if (select_list.length >= 1) {
    add_button.setAttribute('disabled','');
    preview_button.setAttribute('disabled','');
  } else {
    add_button.setAttribute('disabled','true');
    preview_button.setAttribute('disabled','true');
  }
}

function AddPanel() 
{
  var tree = document.getElementById('other-panels');
  var database = tree.database;
  var select_list = document.getElementsByAttribute("selected", "true");
  for (var nodeIndex=0; nodeIndex<select_list.length; nodeIndex++) {
    var node = select_list[nodeIndex];
    if (!node)    break;
    var id = node.getAttribute("id");
    if (!id)      break;
    var rdfNode = RDF.GetResource(id);
    if (!rdfNode) break;
    addOption(database,rdfNode);
  }
  enableSave();
}

function PreviewPanel() 
{
  var tree = document.getElementById('other-panels');
  var database = tree.database;
  var select_list = document.getElementsByAttribute("selected", "true");
  for (var nodeIndex=0; nodeIndex<select_list.length; nodeIndex++) {
    var node = select_list[nodeIndex];
    if (!node)    break;
    var id = node.getAttribute("id");
    if (!id)      break;
    var rdfNode = RDF.GetResource(id);
    if (!rdfNode) break;

    var preview_name = getAttr(database, rdfNode, 'title');
    var preview_URL  = getAttr(database, rdfNode, 'content');
    if (!preview_URL || !preview_name) break;

    var preview = window.open("chrome://sidebar/content/preview.xul",
			      "PanelPreview", "chrome");
    preview.panel_name = preview_name;
    preview.panel_URL = preview_URL;
  }
  enableSave();
}

function enableSave() {
  var save_button = document.getElementById('save_button');
  save_button.setAttribute('disabled','');  
}
