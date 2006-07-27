/* -*- Mode: Java; tab-width: 4; insert-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// xul/id summary:
//
// <box> sidebar-box
//    <splitter> sidebar-panels-splitter
//       <box> sidebar-panels-splitter-box*
//    <sidebarheader> sidebar-title-box
//       <menubutton> sidebar-panel-picker*
//          <menupopup> sidebar-panel-picker-popup
//    <box> sidebar-panels
//       <template> sidebar-template*
//          <box> sidebar-iframe-no-panels
// <splitter> sidebar-splitter
// <menupopup> menu_View_Popup*
//    <menuitem> sidebar-menu

//////////////////////////////////////////////////////////////
// Global variables
//////////////////////////////////////////////////////////////


var gCurFrame;
var gTimeoutID = null;
var gMustInit = true;
var gAboutToUncollapse = false;
var gCheckMissingPanels = true;

function setBlank()
{
    gTimeoutID = null;
    gCurFrame.setAttribute('src', 'chrome://communicator/content/sidebar/PageNotFound.xul');
}


// Uncomment for debug output
const SB_DEBUG = false;

// pref for limiting number of tabs in view
// initialized in sidebar_overlay_init()
var gNumTabsInViewPref;

// The rdf service
const RDF_URI = '@mozilla.org/rdf/rdf-service;1';
var RDF = Components.classes[RDF_URI].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

const NC = "http://home.netscape.com/NC-rdf#";

// The directory services property to find panels.rdf
const PANELS_RDF_FILE = "UPnls";
const SIDEBAR_VERSION = "0.1";

// The default sidebar:
var sidebarObj = new Object;
sidebarObj.never_built = true;

//////////////////////////////////////////////////////////////////////
// sbPanelList Class
//
// Wrapper around DOM representation of the sidebar. This UI event
// handlers and the sidebar datasource observers call into this.  This
// class is responsible for keeping the DOM state of the sidebar up to
// date.
// This class does not make any changes to the sidebar rdf datasource.
//////////////////////////////////////////////////////////////////////

function sbPanelList(container_id)
{
  debug("sbPanelList("+container_id+")");
  this.node = document.getElementById(container_id);
  this.childNodes = this.node.childNodes;
  this.initialized = false; // set after first display of tabs
}

sbPanelList.prototype.get_panel_from_id =
function (id)
{
  debug("get_panel_from_id(" + id + ")");
  var index = 0;
  var header = null;
  if (id && id != '') {
    for (var ii=2; ii < this.node.childNodes.length; ii += 2) {
      header = this.node.childNodes.item(ii);
      if (header.getAttribute('id') == id) {
        debug("get_panel_from_id: Found at index, " + ii);
        index = ii;
        break;
      }
    }
  }
  if (index > 0) {
    return new sbPanel(id, header, index);
  } else {
    return null;
  }
}

sbPanelList.prototype.get_panel_from_header_node =
function (node)
{
  return this.get_panel_from_id(node.getAttribute('id'));
}

sbPanelList.prototype.get_panel_from_header_index =
function (index)
{
  return this.get_panel_from_header_node(this.node.childNodes.item(index));
}

sbPanelList.prototype.find_first =
function (panels)
{
  debug("pick_default_panel: length=" + this.node.childNodes.length);
  for (var ii = 2; ii < this.node.childNodes.length; ii += 2) {
    var panel = this.get_panel_from_header_index(ii);
    if (!panel.is_excluded() && panel.is_in_view()) {
      return panel;
    }
  }
  return null;
}

sbPanelList.prototype.find_last =
function (panels)
{
  debug("pick_default_panel: length=" + this.node.childNodes.length);
  for (var ii=(this.node.childNodes.length - 1); ii >= 2; ii -= 2) {
    var panel = this.get_panel_from_header_index(ii);
    if (!panel.is_excluded() && panel.is_in_view()) {
      return panel;
    }
  }
  return null;
}

sbPanelList.prototype.visible_panels_exist =
function ()
{
  var i;
  var panels = this.node.childNodes;
  for (i = 2; i < panels.length; i += 2)
  {
    if (!panels.item(i).hidden)
      return true;
  }
  return false;
}

sbPanelList.prototype.num_panels_included =
function ()
{
  var count = 0;
  var panels = this.node.childNodes;
  for (var i = 2; i < panels.length; i += 2)
  {
    var curr = this.get_panel_from_header_index(i);
    if (!curr.is_excluded())
      count++;
  }
  return count;
}

sbPanelList.prototype.num_panels_in_view =
function ()
{
  var count = 0;
  var panels = this.node.childNodes;
  for (var i = 2; i < panels.length; i += 2)
  {
    var curr = this.get_panel_from_header_index(i);
    if (curr.is_in_view())
      count++;
  }
  return count;
}

sbPanelList.prototype.select =
function (panel, force_reload)
{
  if (!force_reload && panel.is_selected()) {
    return;
  }
  // select(): Open this panel and possibly reload it.
  if (this.node.getAttribute('last-selected-panel') != panel.id) {
    // "last-selected-panel" is used as a global variable.
    // this.update() will reference "last-selected-panel".
    // This way the value can be persisted in localstore.rdf.
    this.node.setAttribute('last-selected-panel', panel.id);
  }
  this.update(force_reload);
}

sbPanelList.prototype.exclude =
function (panel)
{
  if (this.node.getAttribute('last-selected-panel') == panel.id) {
    this.select_default_panel();
  } else {
    this.update(false);
  }
}


sbPanelList.prototype.select_default_panel =
function ()
{
  var default_panel = null

  // First, check the XUL for the "defaultpanel" attribute of "sidebar-box".
  var sidebar_container = document.getElementById('sidebar-box');
  var content_default_id = sidebar_container.getAttribute('defaultpanel');
  if (content_default_id != '') {
    var content = sidebarObj.panels.get_panel_from_id(content_default_id);
    if (content && !content.is_excluded() && content.is_in_view()) {
      default_panel = content;
    }
  }

  // Second, try to use the panel persisted in 'last-selected-panel'.
  if (!default_panel) {
    var last_selected_id = this.node.getAttribute('last-selected-panel');
    if (last_selected_id != '') {
      var last = sidebarObj.panels.get_panel_from_id(last_selected_id);
      if (last && !last.is_excluded() && last.is_in_view()) {
        default_panel = last;
      }
    }
  }

  // Finally, just use the last one in the list.
  if (!default_panel) {
    default_panel = this.find_last();
  }

  if (default_panel) {
    this.node.setAttribute('last-selected-panel', default_panel.id);
  }
  this.update(false);
}

sbPanelList.prototype.refresh =
function ()
{
  var last_selected_id = this.node.getAttribute('last-selected-panel');
  var last_selected = sidebarObj.panels.get_panel_from_id(last_selected_id);
  if (last_selected && last_selected.is_selected()) {
    // The desired panel is already selected
    this.update(false);
  } else {
    this.select_default_panel();
  }
}

// panel_loader(): called from a timer that is set in sbPanelList.update()
// Removes the "Loading..." screen when the panel has finished loading.
function panel_loader() {
  debug("panel_loader()");

  if (gTimeoutID != null) {
      clearTimeout(gTimeoutID);
      gTimeoutID = null; 
  }

  this.removeEventListener("load", panel_loader, true);
  this.removeAttribute('collapsed');
  this.setAttribute('loadstate','loaded');
  this.parentNode.firstChild.setAttribute('hidden','true');

  if (this.hasAttribute('focusOnLoad')) {
    var elementToFocus = this.contentDocument.documentElement.getAttribute('elementtofocus');
    if (elementToFocus) {
      var element = this.contentDocument.getElementById(elementToFocus);
      if (element)
        element.focus();
      else
        dump(elementToFocus + ' element was not found to focus!\n');
    } else {
      this.contentWindow.focus();
    }
    this.removeAttribute('focusOnLoad');
  }
}
sbPanelList.prototype.update =
function (force_reload)
{
  // This function requires that the attribute 'last-selected-panel'
  // holds the id of a non-excluded panel. If it doesn't, no panel will
  // be selected. The attribute is used instead of a funciton
  // parameter to allow the value to be persisted in localstore.rdf.
  var selected_id = this.node.getAttribute('last-selected-panel');

  if (sidebar_is_collapsed()) {
    sidebarObj.collapsed = true;
  } else {
    sidebarObj.collapsed = false;
  }

  var num_included = sidebarObj.panels.num_panels_included();
  if (num_included > gNumTabsInViewPref)
    document.getElementById("nav-buttons-box").hidden = false;
  else
    document.getElementById("nav-buttons-box").hidden = true;

  var have_set_top = 0;
  var have_set_after_selected = 0;
  var is_after_selected = 0;
  var last_header = 0;
  var num_in_view = 0;
  debug("this.initialized: " + this.initialized);
  for (var ii=2; ii < this.node.childNodes.length; ii += 2) {
    var header = this.node.childNodes.item(ii);
    var content = this.node.childNodes.item(ii+1);
    var id = header.getAttribute('id');
    var panel = new sbPanel(id, header, ii);
    var excluded = panel.is_excluded();
    var in_view = false;
    if (!this.initialized) 
    {
      if (num_in_view < gNumTabsInViewPref)
        in_view = true;
    }
    else
    {
      if (header.getAttribute("in-view") == "true")
        in_view = true;
    }
    if (excluded || !in_view)
    {
      debug("item("+ii/2+") excluded: " + excluded + 
                          " in view: " + in_view);
      header.setAttribute('hidden','true');
      content.setAttribute('hidden','true');
      if (!in_view)
      {
        header.setAttribute("in-view", false);
        header.removeAttribute("top-panel");
        header.removeAttribute("last-panel");
      }
    } else {
      // only set if in view
      if (!this.initialized || (num_in_view < gNumTabsInViewPref))
        last_header = header;
      header.removeAttribute('last-panel');
      // only set if in view
      if (!have_set_top && 
          (!this.initialized || (header.getAttribute("in-view") == "true")))
      {
        header.setAttribute('top-panel','true');
        have_set_top = 1;
      } else {
        header.removeAttribute('top-panel');
      }
      if (!have_set_after_selected && is_after_selected) {
        header.setAttribute('first-panel-after-selected','true');
        have_set_after_selected = 1
      } else {
        header.removeAttribute('first-panel-after-selected');
      }
      header.removeAttribute('hidden');
      header.setAttribute("in-view", true);
      num_in_view++;
      
      // (a) when we have hit the maximum number of tabs that can be in view and no tab 
      //     has been selected yet
      //     -or-
      // (b) when we have reached the last tab we are about to display
      if ( ((num_in_view == num_included) ||
            (num_in_view == gNumTabsInViewPref)) &&
          !is_after_selected )
      {
        selected_id = id;
        this.node.setAttribute('last-selected-panel', id);
      }

      // Pick sandboxed, or unsandboxed iframe
      var iframe = panel.get_iframe();
      var load_state;

      if (selected_id == id) {
        is_after_selected = 1
        debug("item("+ii/2+") selected");
        header.setAttribute('selected', 'true');
        content.removeAttribute('hidden');
        content.removeAttribute('collapsed');

        if (sidebarObj.collapsed && panel.is_sandboxed()) {
          if (!panel.is_persistent()) {
            debug("    set src=about:blank");
            iframe.setAttribute('src', 'about:blank');
          }
        } else {
          var saved_src = iframe.getAttribute('content');
          var src = iframe.getAttribute('src');
          // either we have been requested to force_reload or the
          // panel src has changed so we must restore the original src
          if (force_reload || (saved_src != src)) {
            debug("    set src="+saved_src);
            iframe.setAttribute('src', saved_src);

            if (gTimeoutID != null)
              clearTimeout(gTimeoutID);

            gCurFrame = iframe;
            gTimeoutID = setTimeout (setBlank, 20000);
          }
        }

        load_state = content.getAttribute('loadstate');
        if (load_state == 'stopped') {
          load_state = 'never loaded';
          toggleLoadarea(content);
        }
        if (load_state == 'never loaded') {
          iframe.removeAttribute('hidden');
          iframe.setAttribute('loadstate', 'loading');
          iframe.addEventListener('load', panel_loader, true);
        }
      } else {
        debug("item("+ii/2+")");
        header.removeAttribute('selected');
        content.setAttribute('collapsed','true');

        if (!panel.is_persistent()) {
          iframe.setAttribute('src', 'about:blank');
          load_state = content.getAttribute('loadstate');
          if (load_state == 'loading') {
            iframe.removeEventListener("load", panel_loader, true);
            content.setAttribute('hidden','true');
            iframe.setAttribute('loadstate', 'never loaded');
          }
        }
        if (panel.is_sandboxed()) {
          if (!panel.is_persistent())
            iframe.setAttribute('src', 'about:blank');
        }
      }
    }
  }
  if (last_header) {
    last_header.setAttribute('last-panel','true');
  }

  var no_panels_iframe = document.getElementById('sidebar-iframe-no-panels');
  if (have_set_top) {
      no_panels_iframe.setAttribute('hidden','true');
      // The hide and show of 'sidebar-panels' should not be needed,
      // but some old profiles may have this persisted as hidden (50973).
      this.node.removeAttribute('hidden');
  } else {
      no_panels_iframe.removeAttribute('hidden');
  }

  this.initialized = true;
}


//////////////////////////////////////////////////////////////////////
// sbPanel Class
//
// Like sbPanelList, this class is a wrapper around DOM representation
// of individual panels in the sidebar. This UI event handlers and the
// sidebar datasource observers call into this.  This class is
// responsible for keeping the DOM state of the sidebar up to date.
// This class does not make any changes to the sidebar rdf datasource.
//////////////////////////////////////////////////////////////////////

function sbPanel(id, header, index)
{
  // This constructor should only be called by sbPanelList class.
  // To create a panel instance, use the helper functions in sbPanelList:
  //   sb_panel.get_panel_from_id(id)
  //   sb_panel.get_panel_from_header_node(dom_node)
  //   sb_panel.get_panel_from_header_index(index)
  this.id = id;
  this.header = header;
  this.index = index;
  this.parent = sidebarObj.panels;
}

sbPanel.prototype.get_header =
function ()
{
  return this.header;
}

sbPanel.prototype.get_content =
function ()
{
  return this.get_header().nextSibling;
}

sbPanel.prototype.is_sandboxed =
function ()
{
  if (typeof this.sandboxed == "undefined") {
    var content = this.get_content();
    var unsandboxed_iframe = content.childNodes.item(1);
    this.sandboxed = !unsandboxed_iframe.getAttribute('content').match(/^chrome:/);
  }
  return this.sandboxed;
}

sbPanel.prototype.get_iframe =
function ()
{
  if (typeof this.iframe == "undefined") {
    var content = this.get_content();
    if (this.is_sandboxed()) {
      var unsandboxed_iframe = content.childNodes.item(2);
      this.iframe = unsandboxed_iframe;
    } else {
      var sandboxed_iframe = content.childNodes.item(1);
      this.iframe = sandboxed_iframe;
    }
  }
  return this.iframe;
}

// This exclude function is used on panels and on the panel picker menu.
// That is why it is hanging out in the global name space instead of
// minding its own business in the class.
function sb_panel_is_excluded(node)
{
  var exclude = node.getAttribute('exclude');
  return ( exclude && exclude != '' &&
           exclude.indexOf(sidebarObj.component) != -1 );
}
sbPanel.prototype.is_excluded =
function ()
{
  return sb_panel_is_excluded(this.get_header());
}

sbPanel.prototype.is_in_view =
function()
{
  return (this.header.getAttribute("in-view") == "true");
}

sbPanel.prototype.is_selected =
function (panel_id)
{
  return 'true' == this.get_header().getAttribute('selected');
}

sbPanel.prototype.is_persistent =
function ()
{
    var rv = false;
    var datasource = RDF.GetDataSource(sidebarObj.datasource_uri);
    var persistNode = datasource.GetTarget(RDF.GetResource(this.id),
                                           RDF.GetResource(NC + "persist"),
                                           true);
    if (persistNode)
    {
        persistNode = 
          persistNode.QueryInterface(Components.interfaces.nsIRDFLiteral);
        rv = persistNode.Value == 'true';
    }

    return rv;
}

sbPanel.prototype.select =
function (force_reload)
{
  this.parent.select(this, force_reload);
}

sbPanel.prototype.stop_load =
function ()
{
  var iframe = this.get_iframe();
  var content = this.get_content();
  var load_state = iframe.getAttribute('loadstate');
  if (load_state == "loading") {
    debug("Stop the presses");
    iframe.removeEventListener("load", panel_loader, true);
    content.setAttribute("loadstate", "stopped");
    iframe.setAttribute('src', 'about:blank');
    toggleLoadarea(content);
  }
}

function toggleLoadarea(content)
{
  // toggle between "loading" and "load stopped" in the UI
  var widgetBox = content.firstChild.firstChild;
  var widgetBoxKids = widgetBox.childNodes;
  var stopButton = widgetBoxKids.item(3);
  var reloadButton = widgetBoxKids.item(4);
  var loadingImage = widgetBox.firstChild;
  var loadingText = loadingImage.nextSibling;
  var loadStoppedText = loadingText.nextSibling;

  // sanity check
  if (stopButton.getAttribute("type") != "stop")
  {
    debug("Error: Expected button of type=\"stop\" but didn't get one!");
    return;
  }

  if (!stopButton.hidden)
  {
    // change button from "stop" to "reload"
    stopButton.hidden = "true";
    reloadButton.removeAttribute("hidden");

    // hide the loading image and set text to "load stopped"
    loadingImage.hidden = "true";
    loadingText.hidden = "true";
    loadStoppedText.removeAttribute("hidden");
  }
  else
  {
    // change button from "reload" to "stop"
    stopButton.removeAttribute("hidden");
    reloadButton.hidden = "true";

    // show the loading image and set text to "loading"
    loadingImage.removeAttribute("hidden");
    loadingText.removeAttribute("hidden");
    loadStoppedText.hidden = "true";
  }
}

sbPanel.prototype.exclude =
function ()
{
  // Exclusion is handled by the datasource,
  // but we need to make sure this panel is no longer selected.
  this.get_header().removeAttribute('selected');
  this.parent.exclude(this);
}

sbPanel.prototype.reload =
function ()
{
  if (!this.is_excluded()) {
    this.select(true);
  }
}

//////////////////////////////////////////////////////////////////
// Panels' RDF Datasource Observer
//
// This observer will ensure that the Sidebar UI stays current
// when the datasource changes.
// - When "refresh" is asserted, the sidebar refreshed.
//   Currently this happens when a panel is included/excluded or
//   added/removed (the later comes from the customize dialog).
// - When "refresh_panel" is asserted, the targeted panel is reloaded.
//   Currently this happens when the customize panel dialog is closed.
//////////////////////////////////////////////////////////////////
var panel_observer = {
  onAssert : function(ds,src,prop,target) {
    //debug ("observer: assert");
    // "refresh" is asserted by select menu and by customize.js.
    if (prop == RDF.GetResource(NC + "refresh")) {
      sidebarObj.panels.initialized = false; // reset so panels are put in view
      sidebarObj.panels.refresh();
    } else if (prop == RDF.GetResource(NC + "refresh_panel")) {
      var panel_id = target.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
      var panel = sidebarObj.panels.get_panel_from_id(panel_id);
      panel.reload();
    }
  },
  onUnassert : function(ds,src,prop,target) {
    //debug ("observer: unassert");
  },
  onChange : function(ds,src,prop,old_target,new_target) {
    //debug ("observer: change");
  },
  onMove : function(ds,old_src,new_src,prop,target) {
    //debug ("observer: move");
  },
  onBeginUpdateBatch : function(ds) {
    //debug ("observer: onBeginUpdateBatch");
  },
  onEndUpdateBatch : function(ds) {
    //debug ("observer: onEndUpdateBatch");
  }
};

// Use an assertion to pass a "refresh" event to all the sidebars.
// They use observers to watch for this assertion (see above).
function refresh_all_sidebars() {
  sidebarObj.datasource.Assert(RDF.GetResource(sidebarObj.resource),
                               RDF.GetResource(NC + "refresh"),
                               RDF.GetLiteral("true"),
                               true);
  sidebarObj.datasource.Unassert(RDF.GetResource(sidebarObj.resource),
                                 RDF.GetResource(NC + "refresh"),
                                 RDF.GetLiteral("true"));
}

//////////////////////////////////////////////////////////////
// Sidebar Init
//////////////////////////////////////////////////////////////
function sidebar_overlay_init() {
  if (sidebar_is_collapsed() && !gAboutToUncollapse)
    return;
  gMustInit = false;
  sidebarObj.panels = new sbPanelList('sidebar-panels');
  sidebarObj.datasource_uri = get_sidebar_datasource_uri();
  sidebarObj.resource = 'urn:sidebar:current-panel-list';

  sidebarObj.master_datasources = "";
  sidebarObj.master_datasources = get_remote_datasource_url();
  sidebarObj.master_datasources += " chrome://communicator/content/sidebar/local-panels.rdf";
  sidebarObj.master_resource = 'urn:sidebar:master-panel-list';
  sidebarObj.component = document.firstChild.getAttribute('windowtype');
  debug("sidebarObj.component is " + sidebarObj.component);

  // Initialize the display
  var sidebar_element = document.getElementById('sidebar-box');
  var sidebar_menuitem = document.getElementById('sidebar-menu');
  if (sidebar_is_hidden()) {
    if (sidebar_menuitem) {
      sidebar_menuitem.setAttribute('checked', 'false');
    }
  } else {
    if (sidebar_menuitem) {
      sidebar_menuitem.setAttribute('checked', 'true');
    }

    // for old profiles that don't persist the hidden attribute when splitter is not hidden.
    var sidebar_splitter = document.getElementById('sidebar-splitter')
    if (sidebar_splitter)
      sidebar_splitter.setAttribute('hidden', 'false');
    
    if (sidebarObj.never_built) {
      sidebarObj.never_built = false;
      
      debug("sidebar = " + sidebarObj);
      debug("sidebarObj.resource = " + sidebarObj.resource);
      debug("sidebarObj.datasource_uri = " + sidebarObj.datasource_uri);

      // Obtain the pref for limiting the number of tabs in view
      try
      {
        var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                      getService(Components.interfaces.nsIPrefBranch);
        gNumTabsInViewPref = prefs.getIntPref("sidebar.num_tabs_in_view");
      }
      catch (ex)
      {
        gNumTabsInViewPref = 8; // failover default
      }

      // Show the header for the panels area. Use a splitter if there
      // is stuff over the panels area.
      var sidebar_panels_splitter = document.getElementById('sidebar-panels-splitter');
      if (sidebar_element.firstChild != sidebar_panels_splitter) {
        debug("Showing the panels splitter");
        sidebar_panels_splitter.removeAttribute('hidden');
      }
    }
    if (sidebar_is_collapsed()) {
      sidebarObj.collapsed = true;
    } else {
      sidebarObj.collapsed = false;
    }

    sidebar_open_default_panel(100, 0);
  }
}

function sidebar_overlay_destruct() {
    var panels = document.getElementById('sidebar-panels');
    debug("Removing observer from database.");
    panels.database.RemoveObserver(panel_observer);
}

var gBusyOpeningDefault = false;

function sidebar_open_default_panel(wait, tries) {
  // check for making function reentrant
  if (gBusyOpeningDefault)
    return;
  gBusyOpeningDefault = true;

  var ds = RDF.GetDataSource(sidebarObj.datasource_uri);
  var currentListRes = RDF.GetResource("urn:sidebar:current-panel-list");
  var panelListRes = RDF.GetResource("http://home.netscape.com/NC-rdf#panel-list");
  var container = ds.GetTarget(currentListRes, panelListRes, true);
  if (container) {
    // Add the user's current panel choices to the template builder,
    // which will aggregate it with the other datasources that describe
    // the individual panel's title, customize URL, and content URL.
    var panels = document.getElementById('sidebar-panels');
    panels.database.AddDataSource(ds);
    
    debug("Adding observer to database.");
    panels.database.AddObserver(panel_observer);

    // XXX This is a hack to force re-display
    panels.builder.rebuild();
  } else {
    if (tries < 3) {
      // No children yet, try again later
      setTimeout('sidebar_open_default_panel(' + (wait*2) + ',' + (tries+1) + ')',wait);
      gBusyOpeningDefault = false;
      return;
    } else {
      sidebar_fixup_datasource();
    }
  }

  sidebarObj.panels.refresh();
  gBusyOpeningDefault = false;
  if (gCheckMissingPanels)
    check_for_missing_panels();
}

function SidebarRebuild() {
  sidebarObj.panels.initialized = false; // reset so panels are brought in view
  var panels = document.getElementById("sidebar-panels");
  panels.builder.rebuild();
  sidebar_open_default_panel(100, 0);
}

const NS_ERROR_FILE_NOT_FOUND = 0x80520012;

function check_for_missing_panels() {
  var tabs = sidebarObj.panels.node.childNodes;
  var currHeader;
  var currTab;
  for (var i = 2; i < tabs.length; i += 2) {
    currHeader = tabs[i];
    currTab = new sbPanel(currHeader.getAttribute("id"), currHeader, i);
    if (!currTab.is_excluded()) {
      if (currHeader.hasAttribute("prereq") && currHeader.getAttribute("prereq") != "") {
        var prereq_file = currHeader.getAttribute("prereq");
        var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                                  .getService(Components.interfaces.nsIIOService);      
        var uri = ioService.newURI(prereq_file, null, null);
        var channel = ioService.newChannelFromURI(uri);
        try {
          channel.open();
        }
        catch(ex if (ex.result == NS_ERROR_FILE_NOT_FOUND)) {
          sidebarObj.datasource = RDF.GetDataSource(sidebarObj.datasource_uri);
          sidebarObj.datasource.Assert(RDF.GetResource(currHeader.getAttribute("id")),
                                       RDF.GetResource(NC + "exclude"),
                                       RDF.GetLiteral(sidebarObj.component),
                                       true);
          currTab.exclude();
        }        
      }
    }
  }
  gCheckMissingPanels = false;
}

//////////////////////////////////////////////////////////////
// Sidebar File and Datasource functions
//////////////////////////////////////////////////////////////

function sidebar_get_panels_file() {
  try {
    var locator_service = Components.classes["@mozilla.org/file/directory_service;1"].getService();
    if (locator_service)
      locator_service = locator_service.QueryInterface(Components.interfaces.nsIProperties);
    // Use the fileLocator to look in the profile directory to find
    // 'panels.rdf', which is the database of the user's currently
    // selected panels.
    // If <profile>/panels.rdf doesn't exist, GetFileLocation() will copy
    // bin/defaults/profile/panels.rdf to <profile>/panels.rdf
    var sidebar_file = locator_service.get(PANELS_RDF_FILE, Components.interfaces.nsIFile);
    if (!sidebar_file.exists()) {
      // This should not happen, as GetFileLocation() should copy
      // defaults/panels.rdf to the users profile directory
      debug("Sidebar panels file does not exist");
      throw("Panels file does not exist");
    }
    return sidebar_file;
  } catch (ex) {
    // This should not happen
    debug("Error: Unable to grab panels file.\n");
    throw(ex);
  }
  return null;
}

function sidebar_revert_to_default_panels() {
  try {
    var sidebar_file = sidebar_get_panels_file();

    sidebar_file.remove(false);

    // Since we just removed the panels file,
    // this should copy the defaults over.
    sidebar_file = sidebar_get_panels_file();

    debug("sidebar defaults reloaded");
    var datasource = RDF.GetDataSource(sidebarObj.datasource_uri);
    datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Refresh(true);
  } catch (ex) {
    debug("Error: Unable to reload panel defaults file.\n");
  }
  return null;
}

function get_sidebar_datasource_uri() {
  try {
    var sidebar_file = sidebar_get_panels_file();
    
    var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
    var fileHandler = ioService.getProtocolHandler("file").QueryInterface(Components.interfaces.nsIFileProtocolHandler);
    
    return fileHandler.getURLSpecFromFile(sidebar_file);
  } catch (ex) {
    // This should not happen
    debug("Error: Unable to load panels file.\n");
  }
  return null;
}

// Get the template for the available panels url from preferences.
// Replace variables in the url:
//     %LOCALE%  -->  Application locale (e.g. en-us).
//     %VERSION% --> Sidebar file format version (e.g. 0.0).
function get_remote_datasource_url() {
  var url = '';
  var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                        .getService(Components.interfaces.nsIPrefBranch);
  var locale;
  try {
    url = prefs.getCharPref("sidebar.customize.all_panels.url");
    url = url.replace(/%SIDEBAR_VERSION%/g, SIDEBAR_VERSION);
  } catch(ex) {
    debug("Unable to get remote url pref. What now? "+ex);
  }
  try {
    locale = prefs.getComplexValue("intl.content.langcode",
                                   Components.interfaces.nsIPrefLocalizedString);
  } catch(ex) {
    try {
      debug("No lang code pref, intl.content.langcode.");
      debug("Use locale from user agent string instead");

      locale = prefs.getComplexValue("general.useragent.locale",
                                     Components.interfaces.nsIPrefLocalizedString);
  } catch(ex) {
      debug("Unable to get system locale. What now? "+ex);
    }
  }
  locale = locale.data.toLowerCase();
  url = url.replace(/%LOCALE%/g, locale);

  debug("Remote url is " + url);
  return url;
}

function sidebar_fixup_datasource() {
  var datasource = RDF.GetDataSource(sidebarObj.datasource_uri);
  var resource = RDF.GetResource(sidebarObj.resource);

  var panel_list = datasource.GetTarget(resource,
                                        RDF.GetResource(NC+"panel-list"),
                                        true);
  if (!panel_list) {
    debug("Sidebar datasource is an old format or busted\n");
    sidebar_revert_to_default_panels();
  } else {
    // The datasource is ok, but it just has no panels.
    // sidebar_refresh() will display some helper content.
    // Do nothing here.
  }
}

//////////////////////////////////////////////////////////////
// Sidebar Interface for XUL
//////////////////////////////////////////////////////////////

// Change the sidebar content to the selected panel.
// Called when a panel title is clicked.
function SidebarSelectPanel(header, should_popopen, should_unhide) {
  debug("SidebarSelectPanel("+header+","+should_popopen+","+should_unhide+")");
  var panel = sidebarObj.panels.get_panel_from_header_node(header);

  if (!panel) {
    return false;
  }

  var popopen = false;
  var unhide = false;

  if (panel.is_excluded()) {
    return false;
  }
  if (sidebar_is_hidden()) {
    if (should_unhide) {
      unhide = true;
    } else {
      return false;
    }
  }
  if (sidebar_is_collapsed()) {
    if (should_popopen) {
      popopen = true;
    } else {
      return false;
    }
  }
  if (unhide)  SidebarShowHide();
  if (popopen) SidebarExpandCollapse();

  try { 
    panel.get_iframe().setAttribute('focusOnLoad', true); 
  } catch (ex) { 
    // ignore exception for cases where content isn't built yet
    // e.g., auto opening search tab: we don't want to focus search field
  }
  if (!panel.is_selected()) panel.select(false);

  return true;
}

function SidebarGetLastSelectedPanel()
{
  return (sidebarObj.panels && 
          sidebarObj.panels.node.getAttribute('last-selected-panel'));
}

function SidebarGetRelativePanel(direction)
{
  // direction == 1 to view next panel, -1 to view prev panel

  if (sidebar_is_hidden())
    SidebarShowHide();
  if (sidebar_is_collapsed())
    SidebarExpandCollapse();

  var currentPanel = sidebarObj.panels.get_panel_from_id(SidebarGetLastSelectedPanel());
  if (!currentPanel) {
    sidebarObj.panels.select_default_panel();
    return;
  }

  var newPanel = currentPanel;

  do {
    var newPanelIndex = newPanel.index + (direction * 2);
    if (newPanelIndex < 2 || newPanelIndex >= sidebarObj.panels.node.childNodes.length)
      newPanel = (direction == 1)? sidebarObj.panels.find_first(): sidebarObj.panels.find_last();
    else 
      newPanel = sidebarObj.panels.get_panel_from_header_index(newPanelIndex);

    if (!newPanel)
      break;

    if (!newPanel.is_excluded()) {
      SidebarSelectPanel(newPanel.header, true, true);  // found a panel that's not excluded to select -- do it
      break;
    }
  } while (newPanel != currentPanel);  // keep looking for a panel, but don't loop infinitely
}

function SidebarStopPanelLoad(header) {
  var panel = sidebarObj.panels.get_panel_from_header_node(header);
  panel.stop_load();
}

function SidebarReloadPanel(header) {
  var panel = sidebarObj.panels.get_panel_from_header_node(header);
  panel.reload();
}

// No one is calling this right now.
function SidebarReload() {
  sidebarObj.panels.refresh();
}
 
// Set up a lame hack to avoid opening two customize
// windows on a double click.
var gDisableCustomize = false;
function enable_customize() {
  gDisableCustomize = false;
}

// Bring up the Sidebar customize dialog.
function SidebarCustomize() {
  // Use a single sidebar customize dialog
  var cwindowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
  var iwindowManager = Components.interfaces.nsIWindowMediator;
  var windowManager  = cwindowManager.QueryInterface(iwindowManager);

  var customizeWindow = windowManager.getMostRecentWindow('sidebar:customize');

  if (customizeWindow) {
    debug("Reuse existing customize dialog");
    customizeWindow.focus();
  } else {
    debug("Open a new customize dialog");

    if (false == gDisableCustomize) {
      debug("First time creating customize dialog");
      gDisableCustomize = true;

      var panels = document.getElementById('sidebar-panels');

      customizeWindow = window.openDialog(
                         'chrome://communicator/content/sidebar/customize.xul',
                         '_blank','centerscreen,chrome,resizable,dialog=no,dependent',
                         sidebarObj.master_datasources,
                         sidebarObj.master_resource,
                         sidebarObj.datasource_uri,
                         sidebarObj.resource);
      setTimeout(enable_customize, 2000);
    }
  }
}

function BrowseMorePanels()
{
  var url = '';
  var browser_url = "chrome://navigator/content/navigator.xul";
  var locale;
  try {
    var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefBranch);
    url = prefs.getCharPref("sidebar.customize.directory.url");
    var temp = prefs.getCharPref("browser.chromeURL");
    if (temp) browser_url = temp;
  } catch(ex) {
    debug("Unable to get prefs: "+ex);
  }
  window.openDialog(browser_url, "_blank", "chrome,all,dialog=no", url);
}



function sidebar_is_collapsed() {
  var sidebar_splitter = document.getElementById('sidebar-splitter');
  return (sidebar_splitter &&
          sidebar_splitter.getAttribute('state') == 'collapsed');
}

function SidebarExpandCollapse() {
  var sidebar_splitter = document.getElementById('sidebar-splitter');
  var sidebar_box = document.getElementById('sidebar-box');
  if (sidebar_splitter.getAttribute('state') == 'collapsed') {
    if (gMustInit)
      sidebar_overlay_init();
    debug("Expanding the sidebar");
    sidebar_splitter.removeAttribute('state');
    sidebar_box.removeAttribute('collapsed');
    SidebarSetButtonOpen(true);
  } else {
    debug("Collapsing the sidebar");
    sidebar_splitter.setAttribute('state', 'collapsed');
    sidebar_box.setAttribute('collapsed', 'true');
    SidebarSetButtonOpen(false);
  }
}

// sidebar_is_hidden() - Helper function for SidebarShowHide().
function sidebar_is_hidden() {
  var sidebar_title = document.getElementById('sidebar-title-box');
  var sidebar_box = document.getElementById('sidebar-box');
  return sidebar_box.getAttribute('hidden') == 'true'
         || sidebar_title.getAttribute('hidden') == 'true';
}

// Show/Hide the entire sidebar.
// Invoked by the "View / Sidebar" menu option.
function SidebarShowHide() {
  var sidebar_box = document.getElementById('sidebar-box');
  var title_box = document.getElementById('sidebar-title-box');
  var sidebar_panels_splitter = document.getElementById('sidebar-panels-splitter');
  var sidebar_panels_splitter_box = document.getElementById('sidebar-panels-splitter-box');
  var sidebar_splitter = document.getElementById('sidebar-splitter');
  var sidebar_menu_item = document.getElementById('sidebar-menu');
  var tabs_menu = document.getElementById('sidebar-panel-picker');

  if (sidebar_is_hidden()) {
    debug("Showing the sidebar");

    // for older profiles: 
    sidebar_box.setAttribute('hidden', 'false'); 
    sidebar_panels_splitter_box.setAttribute('hidden', 'false'); 

    sidebar_box.removeAttribute('collapsed');
    if (sidebar_splitter.getAttribute('state') == 'collapsed')
      sidebar_splitter.removeAttribute('state');
    title_box.removeAttribute('hidden');
    sidebar_panels_splitter_box.removeAttribute('collapsed');
    sidebar_splitter.setAttribute('hidden', 'false');
    if (sidebar_box.firstChild != sidebar_panels_splitter) {
      debug("Showing the panels splitter");
      sidebar_panels_splitter.removeAttribute('hidden');
      if (sidebar_panels_splitter.getAttribute('state') == 'collapsed')
        sidebar_panels_splitter.removeAttribute('state');
    }
    sidebar_overlay_init();
    sidebar_menu_item.setAttribute('checked', 'true');
    tabs_menu.removeAttribute('hidden');
    SidebarSetButtonOpen(true);
  } else {
    debug("Hiding the sidebar");
    var hide_everything = sidebar_panels_splitter.getAttribute('hidden') == 'true';
    if (hide_everything) {
      debug("Hide everything");
      sidebar_box.setAttribute('collapsed', 'true');
      sidebar_splitter.setAttribute('hidden', 'true');
    } else {
      sidebar_panels_splitter.setAttribute('hidden', 'true');
    }
    title_box.setAttribute('hidden', 'true');
    sidebar_panels_splitter_box.setAttribute('collapsed', 'true');
    sidebar_menu_item.setAttribute('checked', 'false');
    tabs_menu.setAttribute('hidden', 'true');
    SidebarSetButtonOpen(false);
  }
  // Immediately save persistent values
  document.persist('sidebar-title-box', 'hidden');
  persist_width();
  window.content.focus();
}

function SidebarBuildPickerPopup() {
  var menu = document.getElementById('sidebar-panel-picker-popup');
  menu.database.AddDataSource(RDF.GetDataSource(sidebarObj.datasource_uri));
  menu.builder.rebuild();

  for (var ii=3; ii < menu.childNodes.length; ii++) {
    var panel_menuitem = menu.childNodes.item(ii);
    if (sb_panel_is_excluded(panel_menuitem)) {
      debug(ii+": "+panel_menuitem.getAttribute('label')+ ": excluded; uncheck.");
      panel_menuitem.removeAttribute('checked');
    } else {
      debug(ii+": "+panel_menuitem.getAttribute('label')+ ": included; check.");
      panel_menuitem.setAttribute('checked', 'true');
    }
  }
}

function SidebarTogglePanel(panel_menuitem) {
  // Create a "container" wrapper around the current panels to
  // manipulate the RDF:Seq more easily.
  sidebarObj.datasource = RDF.GetDataSource(sidebarObj.datasource_uri);

  var did_exclude = false;
  var panel_id = panel_menuitem.getAttribute('id');
  var panel = sidebarObj.panels.get_panel_from_id(panel_id);
  var panel_exclude = panel_menuitem.getAttribute('exclude')
  if (panel_exclude == '') {
    // Nothing excluded for this panel yet, so add this component to the list.
    debug("Excluding " + panel_id + " from " + sidebarObj.component);
    sidebarObj.datasource.Assert(RDF.GetResource(panel_id),
                                RDF.GetResource(NC + "exclude"),
                                RDF.GetLiteral(sidebarObj.component),
                                true);
    panel.exclude();
    did_exclude = true;
  } else {
    // Panel has an exclude string, but it may or may not have the
    // current component listed in the string.
    debug("Current exclude string: " + panel_exclude);
    var new_exclude = panel_exclude;
    if (sb_panel_is_excluded(panel_menuitem)) {
      debug("Plucking this component out of the exclude list");
      replace_pat = new RegExp(sidebarObj.component + "\s*");
      new_exclude = new_exclude.replace(replace_pat,'');
      new_exclude = new_exclude.replace(/^\s+/,'');
      // did_exclude remains false
    } else {
      debug("Adding this component to the exclude list");
      new_exclude = new_exclude + " " + sidebarObj.component;
      panel.exclude();
      did_exclude = true;
    }
    if (new_exclude == '') {
      debug("Removing exclude list");
      sidebarObj.datasource.Unassert(RDF.GetResource(panel_id),
                                     RDF.GetResource(NC + "exclude"),
                                     RDF.GetLiteral(sidebarObj.component));
    } else {
      debug("New exclude string: " + new_exclude);
      exclude_target =
        sidebarObj.datasource.GetTarget(RDF.GetResource(panel_id),
                                        RDF.GetResource(NC + "exclude"),
                                        true);
      sidebarObj.datasource.Change(RDF.GetResource(panel_id),
                                   RDF.GetResource(NC + "exclude"),
                                   exclude_target,
                                   RDF.GetLiteral(new_exclude));
    }
  }

  if (did_exclude)
  {
    // if we excluded a tab in view then add another one
    var tabs = sidebarObj.panels.node.childNodes;
    if (panel.is_in_view())
    {
      // we excluded one so let's try to bring a non-excluded one into view
      var newFirst = null;
      var added = false;
      for (var i = 2; i < tabs.length ; i += 2)
      {
        var currTab = sidebarObj.panels.get_panel_from_header_index(i);
        var hasPotential = !currTab.is_excluded() && !currTab.is_in_view();

        // set potential new first tab in case we can't find one after the
        // tab that was just excluded
        if (!newFirst && hasPotential)
          newFirst = currTab;

        if (i > panel.index && hasPotential)
        {
          currTab.header.setAttribute("in-view", true);
          added = true;
          break;
        }
      }
      if (!added && newFirst)
        newFirst.header.setAttribute("in-view", true);

      // lose it from current view
      panel.header.setAttribute("in-view", false);
    }
  }
  else
  {
    panel.header.setAttribute("in-view", true);

    // if we have one too many tabs we better get rid of an old one
    if (sidebarObj.panels.num_panels_in_view() > gNumTabsInViewPref)
    {
      // we included a new tab so let's take the last one out of view
      for (i = 2; i < tabs.length; i += 2)
      {
        var currHeader = tabs[i];
        if (currHeader.hasAttribute("last-panel"))
          currHeader.setAttribute("in-view", false);
      }
    }

    panel.select(false);
  }

  if (did_exclude && !sidebarObj.panels.visible_panels_exist())
    // surrender focus to main content area
    window.content.focus();
  else
    // force all the sidebars to update
    refresh_all_sidebars();

  // Write the modified panels out.
  sidebarObj.datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Flush();
}

function SidebarNavigate(aDirection)
{
  debug("SidebarNavigate " + aDirection);

  var tabs = sidebarObj.panels.node.childNodes;
  var i;
  var currHeader;
  var currTab;
  // move forward a tab (down in the template)
  if (aDirection > 0)
  {
    // ensure we have a tab below the last one
    var foundLast = false; 
    var oldFirst = null;
    for (i = 2; i < tabs.length; i += 2) 
    {
      currHeader = tabs[i];
      currTab = new sbPanel(currHeader.getAttribute("id"), currHeader, i);
  
      if (!currTab.is_excluded())
      {     
        if (foundLast)
        {
          debug("toggling old first and new last");
          debug("new last:  " + currHeader.getAttribute("id"));
          debug("old first: " + oldFirst.getAttribute("id"));
          currHeader.setAttribute("in-view", true);
          oldFirst.setAttribute("in-view", false);
          
          // if old first was selected select new first instead
          if (oldFirst.getAttribute("id") == 
              sidebarObj.panels.node.getAttribute("last-selected-panel"))
          {
            sidebarObj.panels.node.setAttribute('last-selected-panel', 
              currTab.id);
          }
          
          break;
        }

        if (!foundLast && currHeader.hasAttribute("last-panel"))
        {
          debug("found last");
          foundLast = true;
        }

        // set the old first in case we find a new last below
        // the old last and need to toggle the new first's ``in-view''
        if (!oldFirst && currTab.is_in_view())
          oldFirst = currHeader;
      }
    }
  }
  
  // move back a tab (up in the template)
  else if (aDirection < 0)
  {
    var newFirst = null, newLast = null;
    var foundFirst = false;
    for (i = 2; i < tabs.length; i += 2)
    {
      currHeader = tabs[i];
      currTab = new sbPanel(currHeader.getAttribute("id"), currHeader, i);

      if (!currTab.is_excluded())
      {
        if (!foundFirst && currHeader.hasAttribute("top-panel"))
        {
          debug("found first");
          foundFirst = true;
        }
        if (!foundFirst)
        {
          debug("setting newFirst");
          newFirst = currHeader;
        }

        if (currHeader.hasAttribute("last-panel"))
        {
          debug("found last");

          // ensure we have a tab above the first one
          if (newFirst)
          {
            debug("toggling new first and old last");
            debug("new first: " + newFirst.getAttribute("id"));
            debug("old last:  " + currHeader.getAttribute("id"));

            newFirst.setAttribute("in-view", true);
            currHeader.setAttribute("in-view", false); // hide old last
          
            // if old last was selected, now select one above it
            if (sidebarObj.panels.node.getAttribute("last-selected-panel") ==
                currTab.id)
            {
              sidebarObj.panels.node.setAttribute("last-selected-panel", 
                newLast.getAttribute("id"));
            }

            break;
          }
        }
        if (currTab.is_in_view())
          newLast = currHeader;
      }
    }
  }

  if (aDirection)
    sidebarObj.panels.update(false);
}

//////////////////////////////////////////////////////////////
// Sidebar Hacks and Work-arounds
//////////////////////////////////////////////////////////////

// SidebarCleanUpExpandCollapse() - Respond to grippy click.
function SidebarCleanUpExpandCollapse() {
  // XXX Mini hack. Persist isn't working too well. Force the persist,
  // but wait until the change has commited.
  if (gMustInit) {
    gAboutToUncollapse = true;
    sidebar_overlay_init();
  }

  setTimeout("document.persist('sidebar-box', 'collapsed');",100);
  setTimeout("sidebarObj.panels.refresh();",100);
}

function PersistHeight() {
  // XXX Mini hack. Persist isn't working too well. Force the persist,
  // but wait until the last drag has been committed.
  // May want to do something smarter here like only force it if the
  // height has really changed.
  setTimeout("document.persist('sidebar-panels-splitter-box','height');",100);
}

function persist_width() {
  // XXX Mini hack. Persist isn't working too well. Force the persist,
  // but wait until the width change has commited.
  setTimeout("document.persist('sidebar-box', 'width');",100);

  var is_collapsed = document.getElementById('sidebar-box').
                       getAttribute('collapsed') == 'true';
  SidebarSetButtonOpen(!is_collapsed);
}

function SidebarFinishClick() {

  // XXX Semi-hack for bug #16516.
  // If we had the proper drag event listener, we would not need this
  // timeout. The timeout makes sure the width is written to disk after
  // the sidebar-box gets the newly dragged width.
  setTimeout("persist_width()",100);

  var is_collapsed = document.getElementById('sidebar-box').getAttribute('collapsed') == 'true';
  debug("collapsed: " + is_collapsed);
  if (is_collapsed != sidebarObj.collapsed) {
    if (gMustInit)
      sidebar_overlay_init();
  }
}

function SidebarSetButtonOpen(aSidebarNowOpen)
{
  // change state so toolbar icon can be updated
  var pt = document.getElementById("PersonalToolbar");
  if (pt) {
    pt.setAttribute("prefixopen", aSidebarNowOpen);

    // set tooltip for toolbar icon
    var header = document.getElementById("sidebar-title-box");
    var tooltip = header.getAttribute(aSidebarNowOpen ? 
                  "tooltipclose" : "tooltipopen");
    pt.setAttribute("prefixtooltip", tooltip);
  }
}

function SidebarInitContextMenu(aMenu, aPopupNode)
{
  var panel = sidebarObj.panels.get_panel_from_header_node(aPopupNode);
  var switchItem = document.getElementById("switch-ctx-item");
  var reloadItem = document.getElementById("reload-ctx-item");
  var stopItem = document.getElementById("stop-ctx-item");

  // the current panel can be reloaded, but other panels are not showing 
  // any content, so we only allow you to switch to other panels
  if (panel.is_selected()) 
  {
    switchItem.setAttribute("collapsed", "true"); 
    reloadItem.removeAttribute("disabled");
  }
  else 
  {
    switchItem.removeAttribute("collapsed");
    reloadItem.setAttribute("disabled", "true");
  }

  // only if a panel is currently loading enable the ``Stop'' item
  if (panel.get_iframe().getAttribute("loadstate") == "loading")
    stopItem.removeAttribute("disabled");
  else
    stopItem.setAttribute("disabled", "true");
}

///////////////////////////////////////////////////////////////
// Handy Debug Tools
//////////////////////////////////////////////////////////////
var debug = null;
var dump_attributes = null;
var dump_tree = null;
if (!SB_DEBUG) {
  debug = function (s) {};
  dump_attributes = function (node, depth) {};
  dump_tree = function (node) {};
  var _dump_tree_recur = function (node, depth, index) {};
} else {
  debug = function (s) { dump("-*- sbOverlay: " + s + "\n"); };

  dump_attributes = function (node, depth) {
    var attributes = node.attributes;
    var indent = "| | | | | | | | | | | | | | | | | | | | | | | | | | | | . ";

    if (!attributes || attributes.length == 0) {
      debug(indent.substr(indent.length - depth*2) + "no attributes");
    }
    for (var ii=0; ii < attributes.length; ii++) {
      var attr = attributes.item(ii);
      debug(indent.substr(indent.length - depth*2) + attr.name +
            "=" + attr.value);
    }
  }
  dump_tree = function (node) {
    _dump_tree_recur(node, 0, 0);
  }
  _dump_tree_recur = function (node, depth, index) {
    if (!node) {
      debug("dump_tree: node is null");
    }
    var indent = "| | | | | | | | | | | | | | | | | | | | | | | | | | | | + ";
    debug(indent.substr(indent.length - depth*2) + index +
          " " + node.nodeName);
    if (node.nodeName != "#text") {
      dump_attributes(node, depth);
    }
    var kids = node.childNodes;
    for (var ii=0; ii < kids.length; ii++) {
      _dump_tree_recur(kids[ii], depth + 1, ii);
    }
  }
}

//////////////////////////////////////////////////////////////
// Install the load/unload handlers
//////////////////////////////////////////////////////////////
addEventListener("load", sidebar_overlay_init, false);
addEventListener("unload", sidebar_overlay_destruct, false);
