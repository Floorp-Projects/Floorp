// -*- Mode: Java -*-

// the rdf service
var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

// the current profile directory
// XXX obviously, this shouldn't be hard-coded
var profiledir = 'resource:/res/rdf/';

// the location of the flash registry.
var sidebardb = profiledir + 'panels-browser.rdf';
var sidebar_resource = 'NC:BrowserSidebarRoot';

function Init(sidebardb, sidebar_resource)
{
  // Initialize the Sidebar
  dump('Sidebar Init('+sidebardb+','+sidebar_resource+')\n');

  //var tree = document.getElementById('tree');

  // Install all the datasources named in the Flash Registry into
  // the tree control. Datasources are listed as members of the
  // NC:FlashDataSources sequence, and are loaded in the order that
  // they appear in that sequence.
  var registry = RDF.GetDataSource(sidebardb);

  // Add it to the tree control's composite datasource.
  //tree.database.AddDataSource(registry);

  // Create a 'container' wrapper around the sidebar_resources
  // resource so we can use some utility routines that make access a
  // bit easier.
  var sb_datasource = Components.classes['component://netscape/rdf/container'].createInstance();
  sb_datasource = sb_datasource.QueryInterface(Components.interfaces.nsIRDFContainer);

  sb_datasource.Init(registry, RDF.GetResource(sidebar_resource));
  
  // Now enumerate all of the flash datasources.
  var enumerator = sb_datasource.GetElements();

  while (enumerator.HasMoreElements()) {
    var service = enumerator.GetNext();
    service = service.QueryInterface(Components.interfaces.nsIRDFResource);

    var new_panel = createPanel(registry, service);
    if (new_panel) {
      document.documentElement.appendChild(new_panel);
    }
  }
}

function createPanel(registry, service) {
  var panel_title = getAttr(registry, service, 'title');
  var panel_content = getAttr(registry, service, 'content');
  var panel_customize = getAttr(registry, service, 'customize');

  dump('Adding...' + panel_title + '\n');
  var div = document.createElement('div');
  var title = document.createElement('titlebutton');
  var customize = document.createElement('titlebutton');
  var toolbar = document.createElement('toolbar');
  var iframe = document.createElement('html:iframe');
  var newline = document.createElement('html:br');
  title.setAttribute('value', panel_title);
  customize.setAttribute('value', 'Customize');
  iframe.setAttribute('src', panel_content);

  toolbar.appendChild(title);
  toolbar.appendChild(customize);
  div.appendChild(toolbar);
  div.appendChild(iframe);
  div.appendChild(newline);
  return div;
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


// To get around "window.onload" not working in viewer.
function Boot()
{
    var root = document.documentElement;
    if (root == null) {
        setTimeout(Boot, 0);
    }
    else {
        Init(sidebardb, sidebar_resource);
    }
}

setTimeout('Boot()', 0);
