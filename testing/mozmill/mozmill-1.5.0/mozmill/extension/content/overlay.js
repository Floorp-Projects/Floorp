// ***** BEGIN LICENSE BLOCK *****
// Version: MPL 1.1/GPL 2.0/LGPL 2.1
// 
// The contents of this file are subject to the Mozilla Public License Version
// 1.1 (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
// http://www.mozilla.org/MPL/
// 
// Software distributed under the License is distributed on an "AS IS" basis,
// WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
// for the specific language governing rights and limitations under the
// License.
// 
// The Original Code is Mozilla Corporation Code.
// 
// The Initial Developer of the Original Code is
// Adam Christian.
// Portions created by the Initial Developer are Copyright (C) 2008
// the Initial Developer. All Rights Reserved.
// 
// Contributor(s):
//  Adam Christian <adam.christian@gmail.com>
//  Mikeal Rogers <mikeal.rogers@gmail.com>
// 
// Alternatively, the contents of this file may be used under the terms of
// either the GNU General Public License Version 2 or later (the "GPL"), or
// the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
// in which case the provisions of the GPL or the LGPL are applicable instead
// of those above. If you wish to allow use of your version of this file only
// under the terms of either the GPL or the LGPL, and not to allow others to
// use your version of this file under the terms of the MPL, indicate your
// decision by deleting the provisions above and replace them with the notice
// and other provisions required by the GPL or the LGPL. If you do not delete
// the provisions above, a recipient may use your version of this file under
// the terms of any one of the MPL, the GPL or the LGPL.
// 
// ***** END LICENSE BLOCK *****

var utils = {}; Components.utils.import('resource://mozmill/modules/utils.js', utils);
var init = {}; Components.utils.import('resource://mozmill/modules/init.js', init);

var MozMill = {
  onLoad: function() {
    // initialization code
    this.initialized = true;
  },

  onMenuItemCommand: function() {
    var mmWindows = utils.getWindowByTitle('MozMill IDE');
    if (!mmWindows){
      var height = utils.getPreference("mozmill.height", 740);
      var width = utils.getPreference("mozmill.width", 635);
      //move to top left corner
      var left = utils.getPreference("mozmill.screenX", 0);
      var top = utils.getPreference("mozmill.screenY", 0);

      if (left == 0){
        //make only browser windows big
        var width = window.screen.availWidth/2.5;
        var height = window.screen.availHeight;
        window.resizeTo((window.screen.availWidth - width), window.screen.availHeight);

        var height = window.innerHeight;
        var left = window.innerWidth; 
      }
      
      var paramString = "chrome,resizable,height=" + height +
                               ",width=" + width + ",left="+left+",top="+top;
      var w = window.open("chrome://mozmill/content/mozmill.xul", "", paramString);
    } else { mmWindows[0].focus(); }
  }
};

window.addEventListener("load", function(e) { MozMill.onLoad(e); }, false);

 
function mozMillTestWindow() {
  window.openDialog("chrome://mozmill/content/testwindow.html", "_blank", "chrome,dialog=no, resizable");
}

//adding a mozmill keyboard shortcut
// window.addEventListener("keypress", function(e) { 
//   if ((e.charCode == 109) && (e.ctrlKey)) { 
//     MozMill.onMenuItemCommand(e); 
//   } 
// }, false);
