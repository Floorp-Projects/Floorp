/* -*- Mode: Java; tab-width: 2; c-basic-offset: 2 -*-
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
var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService()
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService)

var sidebar = new Object
sidebar.db       = 'chrome://sidebar/content/sidebar.rdf'
sidebar.resource = 'NC:SidebarRoot'

function sidebarOverlayInit()
{
  var sidebar_element = document.getElementById('sidebarbox');
  if (sidebar_element.getAttribute('hidden')) {
    return
 	}

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
  sb_datasource     = sb_datasource.createInstance()
  var containder    = Components.interfaces.nsIRDFContainer
  sb_datasource     = sb_datasource.QueryInterface(containder)
  sb_datasource.Init(registry, RDF.GetResource(sidebar.resource))
  
  var mypanelsbox = document.getElementById('sidebarbox')

  // Now enumerate all of the flash datasources.
  var enumerator = sb_datasource.GetElements()

  while (enumerator.HasMoreElements()) {
    var service = enumerator.GetNext()
    service = service.QueryInterface(Components.interfaces.nsIRDFResource)

    var is_last = !enumerator.HasMoreElements()
    var new_panel = addSidebarPanel(mypanelsbox, registry, service, is_last)
  }
}

function addSidebarPanel(parent, registry, service, is_last) {
  var panel_title     = getSidebarAttr(registry, service, 'title')
  var panel_content   = getSidebarAttr(registry, service, 'content')
  var panel_height    = getSidebarAttr(registry, service, 'height')

  var iframe   = document.createElement('html:iframe')

	iframe.setAttribute('src', panel_content)
	dump("panel_content="+panel_content+"\n")
  if (is_last) {
    //iframe.setAttribute('flex', '100%')
    iframe.setAttribute('class','panelframe')
	} else {
    iframe.setAttribute('class','panelframe notlast')
  }

  addSidebarPanelTitle(parent, panel_title, is_last)
  parent.appendChild(iframe)
}

function addSidebarPanelTitle(parent, titletext, is_last)
{
  var splitter  = document.createElement('splitter')
  splitter.setAttribute('class', 'panelbar')
	splitter.setAttribute('resizeafter', 'grow')
	splitter.setAttribute('collapse', 'after')

  var label = document.createElement('html:div')
  var text = document.createTextNode(titletext)
  label.appendChild(text)
  label.setAttribute('class','panelbar')

  var spring = document.createElement('spring')
  spring.setAttribute('flex','100%')

  var titledbutton = document.createElement('titledbutton')
  titledbutton.setAttribute('class', 'borderless showhide')
  if (!is_last) {
		titledbutton.setAttribute('onclick', 'openCloseSidebarPanel(this.parentNode)')
	}

  //var corner    = document.createElement('html:img')
  //corner.setAttribute('src', 'chrome://sidebar/skin/corner.gif')
  //splitter.appendChild(corner)

  splitter.appendChild(label)
  splitter.appendChild(spring)
  splitter.appendChild(titledbutton)
  parent.appendChild(splitter)
}

function getSidebarAttr(registry,service,attr_name) {
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

function openCloseSidebarPanel(splitter) {
  var state = splitter.getAttribute("state")

  if (state == "" || state == "open") {
    splitter.setAttribute("state", "collapsed")
  } else {
    splitter.setAttribute("state", "")
	}
}

function reloadSidebar() {
	var titlebox = document.getElementById('titlebox')
	var panel = titlebox.nextSibling
  while (panel) {
    var next = panel.nextSibling
    panel.parentNode.removeChild(panel)
    panel = next
	}
  sidebarOverlayInit(sidebar.db, sidebar.resource)
}

function customizeSidebar() {
	var newWin = window.openDialog('chrome://sidebar/content/customize.xul','New','chrome', sidebar.db, sidebar.resource)
	return newWin
}

function sidebarShowHide() {
  var sidebar = document.getElementById('sidebarbox')
  var sidebar_splitter = document.getElementById('sidebarsplitter')
  var is_hidden = sidebar.getAttribute('hidden')
  if (is_hidden && is_hidden == "true") {
    //dump("Showing the sidebar\n")
    sidebar.setAttribute('hidden','')
    sidebar_splitter.setAttribute('hidden','')
    sidebarOverlayInit(sidebar.db, sidebar.resource)
	} else {
    //dump("Hiding the sidebar\n")
    sidebar.setAttribute('hidden','true')
    sidebar_splitter.setAttribute('hidden','true')
	}
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

