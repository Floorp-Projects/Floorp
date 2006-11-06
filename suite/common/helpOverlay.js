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
 * The Original Code is SeaMonkey internet suite code.
 *
 * The Initial Developer of the Original Code is
 * the SeaMonkey project at mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Neil Rashbrook <neil@parkwaycc.co.uk>
 *  Robert Kaiser <kairo@kairo.at>
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

var helpExternal;

var helpContentListener = {
  onStartURIOpen: function(aURI) {
    return false;
  },
  doContent: function(aContentType, aIsContentPreferred, aRequest, aContentHandler) {
    throw Components.results.NS_ERROR_UNEXPECTED;
  },
  isPreferred: function(aContentType, aDesiredContentType) {
    return false;
  },
  canHandleContent: function(aContentType, aIsContentPreferred, aDesiredContentType) {
    return false;
  },
  loadCookie: null,
  parentContentListener: null,
  QueryInterface: function (aIID) {
    if (aIID.equals(Components.interfaces.nsIURIContentListener) ||
        aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
        aIID.equals(Components.interfaces.nsISupports))
      return this;
    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
  }
};

function initOverlay() {
  helpExternal = document.getElementById("help-external");
  helpExternal.docShell.useErrorPages = false;
  helpExternal
    .docShell
    .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
    .getInterface(Components.interfaces.nsIURIContentListener)
    .parentContentListener = helpContentListener;
  helpExternal.addProgressListener(window.XULBrowserWindow, Components.interfaces.nsIWebProgress.NOTIFY_ALL);
}

function contentClick(event) {
  // is this a left click on a link?
  if (event.shiftKey || event.ctrlKey || event.altKey || event.metaKey || event.button != 0)
    return true;

  // is this a link?
  var target = event.target;
  while (!(target instanceof HTMLAnchorElement))
    if (!(target = target.parentNode))
      return true;

  // is this an internal link?
  if (target.href.lastIndexOf("chrome:", 0) == 0)
    return true;

  var formatter = Components.classes["@mozilla.org/toolkit/URLFormatterService;1"]
                  .getService(Components.interfaces.nsIURLFormatter);
  var uri = target.href;
  if (/^x-moz-url-link:/.test(uri))
    uri = formatter.formatURLPref(RegExp.rightContext);

  const loadFlags = Components.interfaces.nsIWebNavigation.LOAD_FLAGS_IS_LINK;
  try {
    helpExternal.webNavigation.loadURI(uri, loadFlags, null, null, null);
  } catch (e) {}
  return false;
}
