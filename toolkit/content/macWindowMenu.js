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
# The Original Code is Mozilla.org Code.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-2000 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s):
#   Asaf Romano <mozilla.mano@sent.com>
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the LGPL or the GPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****

function macWindowMenuDidShow()
{
  var windowManagerDS =
    Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator']
              .getService(Components.interfaces.nsIWindowDataSource);
  var sep = document.getElementById("sep-window-list");
  // Using double parens to avoid warning
  while ((sep = sep.nextSibling)) {
    var url = sep.getAttribute('id');
    var win = windowManagerDS.getWindowForResource(url);
    if (win.document.documentElement.getAttribute("inwindowmenu") == "false")
      sep.hidden = true;
    else if (win == window)
      sep.setAttribute("checked", "true");
  }
}

function toOpenWindow( aWindow )
{
  // deminiaturize the window, if it's in the Dock
  if (aWindow.windowState == STATE_MINIMIZED)
    aWindow.restore();
  aWindow.document.commandDispatcher.focusedWindow.focus();
}

function ShowWindowFromResource( node )
{
  var windowManagerDS =
    Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator']
              .getService(Components.interfaces.nsIWindowDataSource);

  var desiredWindow = null;
  var url = node.getAttribute('id');
  desiredWindow = windowManagerDS.getWindowForResource( url );
  if (desiredWindow)
    toOpenWindow(desiredWindow);
}

function zoomWindow()
{
  if (window.windowState == STATE_NORMAL)
    window.maximize();
  else
    window.restore();
}
