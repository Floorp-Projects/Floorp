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
# The Original Code is Firebird Help code.
#
# The Initial Developer of the Original Code is
# R.J. Keller
# Portions created by the Initial Developer are Copyright (C) 2003
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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
#
#
# CustomizeToolbar() - Enables the Customize Toolbar dialog and sets toolbar
#	and toolbar button properties approprately.
#
#    id - The id of the toolbar to enable customization for.
function CustomizeToolbar(id)
{
  var customizePopup = document.getElementById("cmd_CustomizeToolbars");
  customizePopup.setAttribute("disabled", "true");

  window.openDialog("chrome://help/content/customizeToolbar.xul", "CustomizeToolbar",
                    "chrome,all,dependent", document.getElementById(id));
}

# ToolboxCustomizeDone() - Resets the toolbar back to its default state. Reenables
#	toolbar buttons and the "Customize Toolbar" command.
#
#    aToolboxChanged - boolean value of whether or not the toolbar was changed.
function ToolboxCustomizeDone(aToolboxChanged)
{
# Update global UI elements that may have been added or removed
  var customizePopup = document.getElementById("cmd_CustomizeToolbars");
  customizePopup.removeAttribute("disabled");

# make sure our toolbar buttons have the correct enabled state restored to them...
  if (this.UpdateToolbar != undefined)
    UpdateToolbar(focus);

# Set the Sidebar Toolbar Button's label appropriately.
  var sidebarButton = document.getElementById("sidebar-button");
  var strBundle = document.getElementById("bundle_help");
  if (document.getElementById("helpsidebar-box").hidden) {
    sidebarButton.label = strBundle.getString("showSidebarLabel");
  } else {
    sidebarButton.label = strBundle.getString("hideSidebarLabel");
  }
}

function UpdateToolbar(caller)
{
# dump("XXX update help-toolbar " + caller + "\n");
  document.commandDispatcher.updateCommands('help-toolbar');

# re-enable toolbar customization command
  var customizePopup = document.getElementById("cmd_CustomizeToolbars");
  customizePopup.removeAttribute("disabled");

# hook for extra toolbar items
  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  observerService.notifyObservers(window, "help:updateToolbarItems", null);
}
