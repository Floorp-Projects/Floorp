# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
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
# Portions created by the Initial Developer are Copyright (C) 2003
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Frank Yung-Fong Tang <ftang@netscape.com>
#   Simon Montagu <smontagu@netscape.com>
#   Seth Spitzer <sspitzer@netscape.com>
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
# ***** END LICENSE BLOCK ***** */

var gLangCode;

function onLoad()
{
  var size = document.getElementById("size");
  var downloadButton = document.getElementById("downloadButton");
  var install = document.getElementById("install");
  var fontPackageBundle = document.getElementById("fontPackageBundle");

  // test if win2k (win nt 5.0) or winxp (win nt 5.1)
  if (navigator.userAgent.toLowerCase().indexOf("windows nt 5") != -1) 
  {
    downloadButton.setAttribute("hidden", "true");
    size.setAttribute("hidden", "true");

    // if no download button
    // set title to "Install Font"
    // and set cancel button to "OK"
    document.title = fontPackageBundle.getString("windowTitleNoDownload");
    var cancelButton = document.getElementById("cancelButton");
    cancelButton.setAttribute("label", fontPackageBundle.getString("cancelButtonNoDownload"));
  } 
  else 
  {
    install.setAttribute("hidden", "true");
  }

  // argument is a lang code of the form xx or xx-yy
  gLangCode = window.arguments[0];

  var titleString = fontPackageBundle.getString("name_" + gLangCode);
  var languageTitle = document.getElementById("languageTitle");
  languageTitle.setAttribute("value", titleString);
  
  var sizeString = fontPackageBundle.getString("size_" + gLangCode);
  var sizeSpecification = document.getElementById("sizeSpecification");
  sizeSpecification.setAttribute("value", sizeString);
}

function download()
{ 
  window.open("http://www.mozilla.org/projects/intl/fonts/win/redirect/package_" + gLangCode + ".html");
}

