/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Eric Hodel's <drbrain@segment7.net> code.
 *
 * The Initial Developer of the Original Code is
 * Eric Hodel.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Christopher Hoess <choess@force.stwing.upenn.edu>
 *      Tim Taylor <tim@tool-man.org>
 *      Henri Sivonen <henris@clinet.fi>
 *      Stuart Ballard <sballard@netreach.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/** 
 * called on every page load 
 *
 * FIXME: refresh isn't called until the entire document has finished loading
 * XXX: optimization: don't refresh when page reloaded if the page hasn't
 *    been modified
 * XXX: optimization: don't refresh when toolbar is compacted
 */
 
LinkToolbarUI = function()
{
}

LinkToolbarUI.prototype.refresh =
function()
{
  if (!linkToolbarUI.isLinkToolbarEnabled())
    return;

  linkToolbarUI.doRefresh();
}

LinkToolbarUI.prototype.isLinkToolbarEnabled =
function()
{
  if (document.getElementById("linktoolbar").getAttribute("hidden") == "true")
    return false;
  else
    return true;
}

LinkToolbarUI.prototype.doRefresh =
function()
{
 // Not doing <meta http-equiv> elements yet.
 // If you enable checking for A links, the browser becomes 
 // unresponsive during this call...for as long as 2 or more 
 // seconds on heavily linked documents.

 var currentNode = window._content.document.documentElement;
  if(!(currentNode instanceof Components.interfaces.nsIDOMHTMLHtmlElement)) return;
  currentNode = currentNode.firstChild;
  
  while(currentNode) {
    if(currentNode instanceof Components.interfaces.nsIDOMHTMLHeadElement) {
      currentNode = currentNode.firstChild;
      
      while(currentNode) {
        if ((currentNode instanceof Components.interfaces.nsIDOMHTMLLinkElement) && 
            (currentNode.rel || currentNode.rev) && 
             currentNode.href) {
          linkToolbarHandler.handle(currentNode);
        }
        
        currentNode = currentNode.nextSibling;
      }
    } else if (currentNode instanceof Components.interfaces.nsIDOMElement) {
      // head is supposed to be the first element inside html.
      // Got something else instead. returning
       return;
    } else {
      // Got a comment node or something like that. Moving on.
      currentNode = currentNode.nextSibling;
    }
  }  

  document.getElementById("linktoolbar").
           setAttribute("hasitems", linkToolbarHandler.hasItems);
}

LinkToolbarUI.prototype.getLinkElements =
function()
{
  return getHeadElement().getElementsByTagName("link");
}

LinkToolbarUI.prototype.getHeadElement =
function()
{
  return window._content.document.getElementsByTagName("head").item(0);
}

LinkToolbarUI.prototype.getAnchorElements =
function()
{
  // XXX: document.links includes AREA links, which technically 
  //    shouldn't be checked for REL attributes
  // FIXME: doesn't work on XHTML served as application/xhtml+xml
  return window._content.document.links;
}

/** called on every page unload */
LinkToolbarUI.prototype.clear =
function()
{
  if (!linkToolbarUI.isLinkToolbarEnabled())
    return;

  linkToolbarUI.doClear();
}

LinkToolbarUI.prototype.doClear =
function()
{
  this.hideMiscellaneousSeperator();
  linkToolbarHandler.clearAllItems();
}

/* called whenever something on the toolbar is clicked */
LinkToolbarUI.prototype.clicked =
function(event)
{
  // Only handle primary click.  Change this if we get a context menu
  if (0 != event.button) return;  

  // Return if this is one of the menubuttons.
  if (event.target.getAttribute("type") == "menu") return;
  
  if (!event.target.getAttribute("href")) return;

  var destURL = event.target.getAttribute("href");
  
  // We have to do a security check here, because we are loading URIs given
  // to us by a web page from chrome, which is privileged.
  try {
    var ssm = Components.classes["@mozilla.org/scriptsecuritymanager;1"].getService().
	  	          QueryInterface(Components.interfaces.nsIScriptSecurityManager);
  	ssm.checkLoadURIStr(window.content.location.href, destURL, 0);
 	  loadURI(destURL);
  } catch (e) {
    dump("Error: it is not permitted to load this URI from a <link> element: " + e);
  }
}

// functions for twiddling XUL elements in the toolbar

LinkToolbarUI.prototype.toggleLinkToolbar =
function(checkedItem)
{
  this.goToggleTristateToolbar("linktoolbar", checkedItem);
  this.initHandlers();
  if (this.isLinkToolbarEnabled())
    this.doRefresh();
  else
    this.doClear();
}

LinkToolbarUI.prototype.showMiscellaneousSeperator =
function()
{
  document.getElementById("misc-seperator").removeAttribute("collapsed");
}

LinkToolbarUI.prototype.hideMiscellaneousSeperator =
function()
{
  document.getElementById("misc-seperator").setAttribute("collapsed", "true");
}
LinkToolbarUI.prototype.initLinkbarVisibilityMenu = 
function()
{
  var state = document.getElementById("linktoolbar").getAttribute("hidden");
  if (!state)
    state = "maybe";
  var checkedItem = document.getElementById("cmd_viewlinktoolbar_" + state);
  checkedItem.setAttribute("checked", true);
  checkedItem.checked = true;
}
LinkToolbarUI.prototype.goToggleTristateToolbar =
function(id, checkedItem)
{
  var toolbar = document.getElementById(id);
  if (toolbar)
  {
    toolbar.setAttribute("hidden", checkedItem.value);
    document.persist(id, "hidden");
  }
}
LinkToolbarUI.prototype.initHandlers =
function()
{
  var contentArea = document.getElementById("appcontent");
  if (this.isLinkToolbarEnabled())
  {
    if (!this.handlersActive) {
      contentArea.addEventListener("load", linkToolbarUI.refresh, true);
      contentArea.addEventListener("unload", linkToolbarUI.clear, true);
      this.handlersActive = true;
    }
  } else
  {
    if (this.handlersActive) {
      contentArea.removeEventListener("load", linkToolbarUI.refresh, true);
      contentArea.removeEventListener("unload", linkToolbarUI.clear, true);
      this.handlersActive = false;
    }
  }
}

const linkToolbarUI = new LinkToolbarUI;

