# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
# The Original Code is the Firefox Preferences System.
#
# The Initial Developer of the Original Code is
# Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Gervase Markham <gerv@gerv.net> (Logic)
#   Ben Goodger <ben@mozilla.org>
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

var gFontScalingDialog = {
  init: function ()
  {
    sizeToContent();
  },
  
  onAccept: function ()
  {
    // Get value from the dialog to work out dpi
    var horizSize = parseFloat(document.getElementById("horizSize").value);
    var units = document.getElementById("units").value;
  
    if (!horizSize || horizSize < 0) {
      // We can't calculate anything without a proper value
      window.arguments[0].newdpi = -1;
      return true;
    }
      
    // Convert centimetres to inches.
    // The magic number is allowed because it's a fundamental constant :-)
    if (units == "centimetres")
      horizSize /= 2.54;
  
    // These shouldn't change, but you can't be too careful.
    var horizBarLengthPx = document.getElementById("horizRuler").boxObject.width;
    var horizDPI = parseInt(horizBarLengthPx) / horizSize;
  
    // Average the two <shrug>.
    window.arguments[0].newdpi = Math.round(horizDPI);
    dump("*** intergoat = " + window.arguments[0].toSource() + "\n");
  
    return true;
  }
};

