// -*- Mode: Java -*-

// the rdf service
var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

// the current profile directory
// XXX obviously, this shouldn't be hard-coded
var profiledir = 'resource:/res/rdf/';

// the location of the flash registry.
var sidebardb = profiledir + 'sidebar-browser.rdf';
var sidebar_resource = 'NC:BrowserSidebarRoot';

function dumpTree(node, depth) {
  var indent = "| | | | | | | | | | | | | | | | | | | | | | | | | | | | | + ";
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
  
  var sidebox = document.getElementById('sidebox');

  // Now enumerate all of the flash datasources.
  var enumerator = sb_datasource.GetElements();

  while (enumerator.HasMoreElements()) {
    var service = enumerator.GetNext();
    service = service.QueryInterface(Components.interfaces.nsIRDFResource);

    var new_panel = createPanel(registry, service);
    if (new_panel) {
      sidebox.appendChild(new_panel);
    }
  }
}

function createPanel(registry, service) {
  var panel_title     = getAttr(registry, service, 'title');
  var panel_customize = getAttr(registry, service, 'customize');
  var panel_content   = getAttr(registry, service, 'content');
  var panel_height    = getAttr(registry, service, 'height');

  var box      = document.createElement('box');
  var iframe   = document.createElement('html:iframe');

  var iframeId = iframe.getAttribute('id');
  var panelbar = createPanelTitle(panel_title, panel_customize, iframeId);

  box.setAttribute('align', 'vertical');
  iframe.setAttribute('src', panel_content);
  if (panel_height)
    iframe.setAttribute('style', 'height:' + panel_height + 'px');
  box.appendChild(panelbar);
  box.appendChild(iframe);

  return box;
}

function resize(id) {
  var box = document.getElementById('sidebox');
  var iframe = document.getElementById(id);
  var height = iframe.getAttribute('height');
  dump ('height='+height+'\n');

  if (iframe.getAttribute('display') == 'none') {
    iframe.setAttribute('style', 'height:' + height + 'px; visibility:visible');
    iframe.setAttribute('display','block');
  } else {
    iframe.setAttribute('style', 'height:0px; visibility:hidden');
    iframe.setAttribute('display','none');
    //var parent = iframe.parentNode;
    //parent.removeChild(iframe);
  }
}

function createPanelTitle(titletext,customize_url, id)
{
  var panelbar  = document.createElement('box');
  var title     = document.createElement('titledbutton');
  var customize = document.createElement('titledbutton');
  var spring     = document.createElement('spring');

  title.setAttribute('value', titletext);
  title.setAttribute('class', 'paneltitle');
  title.setAttribute('onclick', 'resize("'+id+'")');
  spring.setAttribute('flex', '100%');
  customize.setAttribute('value', 'Customize');
  if (customize_url) {
    customize.setAttribute('onclick',
	                   'window.open("'+customize_url+'");');
  } else {
    customize.setAttribute('disabled','true');
  }
  panelbar.setAttribute('class', 'panelbar');


  panelbar.appendChild(title);
  panelbar.appendChild(spring);
  panelbar.appendChild(customize);

  return panelbar;
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
        setTimeout(Boot, 1);
    }
    else {
        Init(sidebardb, sidebar_resource);
    }
}

setTimeout('Boot()', 1);
