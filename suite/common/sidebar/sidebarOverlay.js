/* -*- Mode: Java; tab-width: 4; insert-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and
 * limitations under the License.
 *
 * The Original Code is ______________________________________.
 * The Initial Developer of the Original Code is ________________________.
 * Portions created by ______________________ are Copyright (C) ______
 * _______________________. All Rights Reserved. 
 * Contributor(s): ______________________________________. 
 * 
 * Alternatively, the contents of this file may be used under the terms
 * of the _____ license (the ?[___] License?), in which case the
 * provisions of [______] License are applicable instead of those above.
 * If you wish to allow use of your version of this file only under the
 * terms of the [____] License and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the [___] License.  If you do not delete
 * the provisions above, a recipient may use your version of this file
 * under either the MPL or the [___] License.
 */

// the rdf service
var rdf_uri = 'component://netscape/rdf/rdf-service'
var RDF = Components.classes[rdf_uri].getService()
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService)

// the default sidebar:
var defaultsidebar = new Object
defaultsidebar.db = 'chrome://sidebar/content/sidebar.rdf'
defaultsidebar.resource = 'NC:SidebarRoot'

// the current sidebar:
var sidebar = null

function sidebarOverlayInit(usersidebar)
{
  // load up user-specified sidebar
  if (usersidebar) {
    //dump("usersidebar = " + usersidebar + "\n")
    //dump("usersidebar.resource = " + usersidebar.resource + "\n")
    //dump("usersidebar.db = " + usersidebar.db + "\n")
    sidebar = usersidebar
  } 
  else {                  
    try 
    {
      var profileInterface = Components.interfaces.nsIProfile
      var profileURI = 'component://netscape/profile/manager'
      var profileService  = Components.classes[profileURI].getService()
      profileService = profileService.QueryInterface(profileInterface)
      var sidebar_url = profileService.getCurrentProfileDirFromJS()
      sidebar_url.URLString += "sidebar.rdf"

      if (!(sidebar_url.exists())) {
        //dump("using " + defaultsidebar.db + " because " + 
        //sidebar_url.URLString + " does not exist\n")
      } else {
        //dump("sidebar url is " + sidebar_url.URLString + "\n")
        defaultsidebar.db = sidebar_url.URLString
      }
    }
    catch (ex) 
    {
      //dump("failed to get sidebar url, using default\n")
    }
    sidebar = defaultsidebar
  }

  var sidebar_element = document.getElementById('sidebar-box')
  if (sidebar_element.getAttribute('hidden') == 'true') {
    sidebar_element.setAttribute('style', 'display:none')
    return
  }

  //dump("sidebar = " + sidebar + "\n")
  //dump("sidebar.resource = " + sidebar.resource + "\n")
  //dump("sidebar.db = " + sidebar.db + "\n")
  //var panelsbox = document.getElementById('sidebar-panels')
  //panelsbox.setAttribute('datasources',sidebar.db);
  //panelsbox.setAttribute('datasource',sidebar.db);
  //return;

  var registry
  try {
    // First try to construct a new one and load it
    // synchronously. nsIRDFService::GetDataSource() loads RDF/XML
    // asynchronously by default.
    var xmlsrc = 'component://netscape/rdf/datasource?name=xml-datasource'
    registry = Components.classes[xmlsrc].createInstance()
    registry = registry.QueryInterface(Components.interfaces.nsIRDFDataSource)

    var remote = Components.interfaces.nsIRDFRemoteDataSource
    remote = registry.QueryInterface(remote)
    // this will throw if it's already been opened and registered.
    remote.Init(sidebar.db)

    // read it in synchronously.
    remote.Refresh(true)
  }
  catch (ex) {
    // if we get here, then the RDF/XML has been opened and read
    // once. We just need to grab the datasource.
    registry = RDF.GetDataSource(sidebar.db)
  }

  // Create a 'container' wrapper around the sidebar.resources
  // resource so we can use some utility routines that make access a
  // bit easier.
  var sb_datasource = Components.classes['component://netscape/rdf/container']
  var container    = Components.interfaces.nsIRDFContainer
  try {
    sb_datasource     = sb_datasource.createInstance()
    sb_datasource     = sb_datasource.QueryInterface(container)
    sb_datasource.Init(registry, RDF.GetResource(sidebar.resource))
  }
  catch (ex) {
    dump("failed to init sb_datasource\n")
  }
  
  var mypanelsbox = document.getElementById('sidebar-panels')
  if (!mypanelsbox) {
    dump("Unable to find sidebar panels\n")
    return;
  }

  // Now enumerate all of the datasources.
  var enumerator = null
  try {
  enumerator = sb_datasource.GetElements()
  }
  catch (ex) {
   dump("sb_datasource has no elements.\n")
  }

  if (!enumerator) return

  while (enumerator.HasMoreElements()) {
    var service = enumerator.GetNext()
    service = service.QueryInterface(Components.interfaces.nsIRDFResource)

    var is_last = !enumerator.HasMoreElements()
    var new_panel = sidebarAddPanel(mypanelsbox, registry, service, is_last)
  }
}

function sidebarAddPanel(parent, registry, service, is_last) {
  var panel_title     = sidebarGetAttr(registry, service, 'title')
  var panel_content   = sidebarGetAttr(registry, service, 'content')
  var panel_height    = sidebarGetAttr(registry, service, 'height')

  var iframe   = document.createElement('html:iframe')

  iframe.setAttribute('src', panel_content)
  if (panel_height) iframe.setAttribute('height', panel_height)
  iframe.setAttribute('class','panel-frame')

  sidebarAddPanelTitle(parent, panel_title, is_last)
  if (parent) parent.appendChild(iframe)
}

function sidebarAddPanelTitle(parent, titletext, is_last)
{
  var splitter  = document.createElement('splitter')
  splitter.setAttribute('class', 'panel-bar')
  splitter.setAttribute('resizeafter', 'grow')
  splitter.setAttribute('collapse', 'after')
  splitter.setAttribute('onclick', 'sidebarSavePanelState(this)')

  var label = document.createElement('html:div')
  var text = document.createTextNode(titletext)
  label.appendChild(text)
  label.setAttribute('class','panel-bar')

  var spring = document.createElement('spring')
  spring.setAttribute('flex','100%')

  var titledbutton = document.createElement('titledbutton')
  titledbutton.setAttribute('class', 'borderless show-hide')
  titledbutton.setAttribute('onclick','sidebarOpenClosePanel(this.parentNode)')

  splitter.appendChild(label)
  splitter.appendChild(spring)
  splitter.appendChild(titledbutton)
    if (parent) parent.appendChild(splitter)
}

function sidebarGetAttr(registry,service,attr_name) {
  var attr = registry.GetTarget(service,
           RDF.GetResource('http://home.netscape.com/NC-rdf#' + attr_name),
           true)
  if (attr)
    attr = attr.QueryInterface(
          Components.interfaces.nsIRDFLiteral)
  if (attr)
      attr = attr.Value
  return attr
}

function sidebarOpenClosePanel(splitter) {
  var state = splitter.getAttribute("state")
  var resizeafter = splitter.getAttribute("resizeafter")
  var hasOlderSibling = splitter.previousSibling

  if (!hasOlderSibling && resizeafter != 'grow') {
    return
  }
  if (state == "" || state == "open") {
    splitter.setAttribute("state", "collapsed")
  } else {
    splitter.setAttribute("state", "")
  }
}

function sidebarReload() {
  var panelsparent = document.getElementById('sidebar-panels')
  var panel = panelsparent.firstChild

  while (panel) {
    var next = panel.nextSibling
    panelsparent.removeChild(panel)
    panel = next
  } 
  sidebarOverlayInit(sidebar) 
}

function sidebarCustomize() {
  var newWin = window.openDialog('chrome://sidebar/content/customize.xul','New','chrome', sidebar.db, sidebar.resource)
  return newWin
}

function sidebarShowHide() {
  var sidebar = document.getElementById('sidebar-box')
  var sidebar_splitter = document.getElementById('sidebar-splitter')
  var is_hidden = sidebar.getAttribute('hidden')

  if (is_hidden && is_hidden == "true") {
    //dump("Showing the sidebar\n")
    sidebar.setAttribute('hidden','')
    sidebar_splitter.setAttribute('hidden','')
    sidebarOverlayInit()
  } else {
    //dump("Hiding the sidebar\n")
    sidebar.setAttribute('hidden','true')
    sidebar_splitter.setAttribute('hidden','true')
  }
}

function sidebarSavePanelState(splitter) {
}

function sidebarSaveState(splitter) {
  /* Do nothing for now */
  return;
  if (splitter.getAttribute('state') == "collapse") {
    dump("Expanding the sidebar\n")
  } else {
    dump("Collapsing the sidebar\n")
  }
  dumpStats()
}

function dumpAttributes(node) {
  var attributes = node.attributes

  if (!attributes || attributes.length == 0) {
    dump("no attributes")
  }
  for (var ii=0; ii < attributes.length; ii++) {
    var attr = attributes.item(ii)
    dump("attr "+ii+": "+ attr.name +"="+attr.value+"\n")
  }
}

function dumpStats() {
  var box = document.getElementById('sidebar-box');
  var splitter = document.getElementById('sidebar-splitter');
  var style = box.getAttribute('style')

  var visibility = style.match('visibility:([^;]*)')
  if (visibility) {
    visibility = visibility[1]
  }
  dump("sidebar-box.style="+style+"\n")
  dump("sidebar-box.visibility="+visibility+"\n")
  dump('sidebar-box.width='+box.getAttribute('width')+'\n')
  dump('sidebar-box attrs\n---------------------\n')
  dumpAttributes(box)
  dump('sidebar-splitter attrs\n--------------------------\n')
  dumpAttributes(splitter)
}

function dumpTree(node, depth) {
  var indent = "| | | | | | | | | | | | | | | | | | | | | | | | | | | | | + "
  var kids = node.childNodes
  dump(indent.substr(indent.length - depth*2))

  // Print your favorite attributes here
  dump(node.nodeName)
  dump(" "+node.getAttribute('id'))
  dump("\n")

  for (var ii=0; ii < kids.length; ii++) {
    dumpTree(kids[ii], depth + 1)
  }
}

// To get around "window.onload" not working in viewer.
function sidebarOverlayBoot()
{
    var panels = document.getElementById('sidebar-panels');
    if (panels == null) {
        setTimeout(sidebarOverlayBoot, 1);
    }
    else {
        sidebarOverlayInit();
    }
}

setTimeout('sidebarOverlayBoot()', 0);
