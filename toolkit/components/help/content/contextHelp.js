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
# The Original Code is Mozilla Application Suite.
#
# The Initial Developer of the Original Code is
# Ian Oeschger.
# Portions created by the Initial Developer are Copyright (C) 2003
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#      brantgurganus2001@cherokeescouting.org
#      Jswalden86@netzero.net
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

#ifndef MOZ_THUNDERBIRD
const MOZILLA_CONTENT_PACK = "chrome://help/locale/firebirdhelp.rdf";
#else
const MOZILLA_CONTENT_PACK = "chrome://help/locale/thunderbirdhelp.rdf";
#endif

# Set the default content pack to the Mozilla content pack. Use the
# setHelpFileURI function to set this value.
var helpFileURI = MOZILLA_CONTENT_PACK;

# openHelp - Opens up the Mozilla Help Viewer with the specified
#    topic and content pack.
# see http://www.mozilla.org/projects/help-viewer/content_packs.html
function openHelp(topic, contentPack)
{
# helpFileURI is the content pack to use in this function. If contentPack is defined,
# use that and set the helpFileURI to that value so that it will be the default.
  helpFileURI = contentPack || helpFileURI;

# Try to find previously opened help.
  var topWindow = locateHelpWindow(helpFileURI);

  if ( topWindow ) {
# Open topic in existing window.
    topWindow.focus();
    topWindow.displayTopic(topic);
  } else {
# Open topic in new window.
    const params = Components.classes["@mozilla.org/embedcomp/dialogparam;1"]
                             .createInstance(Components.interfaces.nsIDialogParamBlock);
    params.SetNumberStrings(2);
    params.SetString(0, helpFileURI);
    params.SetString(1, topic);
    const ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                         .getService(Components.interfaces.nsIWindowWatcher);
    ww.openWindow(null, "chrome://help/content/help.xul", "_blank", "chrome,all,alwaysRaised,dialog=no", params);
  }
}

# setHelpFileURI - Sets the default content pack to use in the Help Viewer
function setHelpFileURI(rdfURI)
{
  helpFileURI = rdfURI;
}

# Locate existing help window for this content pack.
function locateHelpWindow(contentPack) {
    const windowManagerInterface = Components
        .classes['@mozilla.org/appshell/window-mediator;1'].getService()
        .QueryInterface(Components.interfaces.nsIWindowMediator);
    const iterator = windowManagerInterface.getEnumerator("mozilla:help");
    var topWindow = null;
    var aWindow;

# Loop through help windows looking for one with selected content
# pack.
    while (iterator.hasMoreElements()) {
        aWindow = iterator.getNext();
        if (aWindow.getHelpFileURI() == contentPack) {
            topWindow = aWindow;
        }
    }
    return topWindow;
}
