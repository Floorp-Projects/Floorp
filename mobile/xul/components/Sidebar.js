# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Stephen Lamm            <slamm@netscape.com>
#   Robert John Churchill   <rjc@netscape.com>
#   David Hyatt             <hyatt@mozilla.org>
#   Christopher A. Aillon   <christopher@aillon.com>
#   Myk Melez               <myk@mozilla.org>
#   Pamela Greene           <pamg.bugs@gmail.com>
#   Gavin Sharp             <gavin@gavinsharp.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const SIDEBAR_CID = Components.ID("{22117140-9c6e-11d3-aaf1-00805f8a4905}");
const SIDEBAR_CONTRACTID = "@mozilla.org/sidebar;1";

function Sidebar() {
  // Depending on if we are in the parent or child, prepare to remote
  // certain calls
  var appInfo = Cc["@mozilla.org/xre/app-info;1"];
  if (!appInfo || appInfo.getService(Ci.nsIXULRuntime).processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
    // Parent process

    this.inContentProcess = false;

    // Used for wakeups service. FIXME: clean up with bug 593407
    this.wrappedJSObject = this;

    // Setup listener for child messages. We don't need to call
    // addMessageListener as the wakeup service will do that for us.
    this.receiveMessage = function(aMessage) {
      switch (aMessage.name) {
        case "Sidebar:AddSearchProvider":
          this.AddSearchProvider(aMessage.json.descriptionURL);
      }
    };
  } else {
    // Child process

    this.inContentProcess = true;
    this.messageManager = Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci.nsISyncMessageSender);
  }
}

Sidebar.prototype = {
  // =========================== utility code ===========================
  _validateSearchEngine: function validateSearchEngine(engineURL, iconURL) {
    try {
      // Make sure we're using HTTP, HTTPS, or FTP.
      if (! /^(https?|ftp):\/\//i.test(engineURL))
        throw "Unsupported search engine URL";
    
      // Make sure we're using HTTP, HTTPS, or FTP and refering to a
      // .gif/.jpg/.jpeg/.png/.ico file for the icon.
      if (iconURL &&
          ! /^(https?|ftp):\/\/.+\.(gif|jpg|jpeg|png|ico)$/i.test(iconURL))
        throw "Unsupported search icon URL.";
    } catch(ex) {
      Cu.reportError("Invalid argument passed to window.sidebar.addSearchEngine: " + ex);
      
      var searchBundle = Services.strings.createBundle("chrome://global/locale/search/search.properties");
      var brandBundle = Services.strings.createBundle("chrome://branding/locale/brand.properties");
      var brandName = brandBundle.GetStringFromName("brandShortName");
      var title = searchBundle.GetStringFromName("error_invalid_engine_title");
      var msg = searchBundle.formatStringFromName("error_invalid_engine_msg",
                                                  [brandName], 1);
      Services.prompt.alert(null, title, msg);
      return false;
    }
    
    return true;
  },

  // =========================== nsISidebar ===========================
  addPanel: function addPanel(aTitle, aContentURL, aCustomizeURL) {
    // not supported
  },

  addPersistentPanel: function addPersistentPanel(aTitle, aContentURL, aCustomizeURL) {
    // not supported
  },

  // The suggestedTitle and suggestedCategory parameters are ignored, but remain
  // for backward compatibility.
  addSearchEngine: function addSearchEngine(engineURL, iconURL, suggestedTitle,
                                            suggestedCategory) {
    if (!this._validateSearchEngine(engineURL, iconURL))
      return;

    // File extension for Sherlock search plugin description files
    const SHERLOCK_FILE_EXT_REGEXP = /\.src$/i;

    // OpenSearch files will likely be far more common than Sherlock files, and
    // have less consistent suffixes, so we assume that ".src" is a Sherlock
    // (text) file, and anything else is OpenSearch (XML).
    var dataType;
    if (SHERLOCK_FILE_EXT_REGEXP.test(engineURL))
      dataType = Ci.nsISearchEngine.DATA_TEXT;
    else
      dataType = Ci.nsISearchEngine.DATA_XML;

    Services.search.addEngine(engineURL, dataType, iconURL, true);
  },

  // =========================== nsISidebarExternal ===========================
  // This function exists to implement window.external.AddSearchProvider(),
  // to match other browsers' APIs.  The capitalization, although nonstandard here,
  // is therefore important.
  AddSearchProvider: function AddSearchProvider(aDescriptionURL) {
    if (!this._validateSearchEngine(aDescriptionURL, ""))
      return;

    if (this.inContentProcess) {
      this.messageManager.sendAsyncMessage("Sidebar:AddSearchProvider",
        { descriptionURL: aDescriptionURL });
      return;
    }

    const typeXML = Ci.nsISearchEngine.DATA_XML;
    Services.search.addEngine(aDescriptionURL, typeXML, "", true);
  },

  // This function exists to implement window.external.IsSearchProviderInstalled(),
  // for compatibility with other browsers.  It will return an integer value
  // indicating whether the given engine is installed for the current user.
  // However, it is currently stubbed out due to security/privacy concerns
  // stemming from difficulties in determining what domain issued the request.
  // See bug 340604 and
  // http://msdn.microsoft.com/en-us/library/aa342526%28VS.85%29.aspx .
  // XXX Implement this!
  IsSearchProviderInstalled: function IsSearchProviderInstalled(aSearchURL) {
    return 0;
  },

  // =========================== nsIClassInfo ===========================
  classInfo: XPCOMUtils.generateCI({classID: SIDEBAR_CID,
                                    contractID: SIDEBAR_CONTRACTID,
                                    interfaces: [Ci.nsISidebar,
                                                 Ci.nsISidebarExternal],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "Sidebar"}),

  // =========================== nsISupports ===========================
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISidebar,
                                         Ci.nsISidebarExternal]),

  // XPCOMUtils stuff
  classID: SIDEBAR_CID,
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([Sidebar]);
