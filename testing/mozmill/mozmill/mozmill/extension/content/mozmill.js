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

var mozmill = {}; Components.utils.import('resource://mozmill/modules/mozmill.js', mozmill);
var utils = {}; Components.utils.import('resource://mozmill/modules/utils.js', utils);

var updateOutput = function(){
  //get the checkboxes
  var pass = document.getElementById('outPass');
  var fail = document.getElementById('outFail');
  var info = document.getElementById('outTest');

  //get the collections
  var passCollect = window.document.getElementsByClassName('pass');
  var failCollect = window.document.getElementsByClassName('fail');
  var infoCollect = window.document.getElementsByClassName('test');
  
  //set the htmlcollection display property in accordance item.checked
  var setDisplay = function(item, collection){
    for (var i = 0; i < collection.length; i++){
      if (item.checked == true){
        collection[i].style.display = "block";
      } else {
        collection[i].style.display = "none";
      }
    }
  };
  
  setDisplay(pass, passCollect);
  setDisplay(fail, failCollect);
  setDisplay(info, infoCollect);
};

function cleanUp(){
  //cleanup frame event listeners for output
  removeStateListeners();
  // Just store width and height
  utils.setPreference("mozmill.screenX", window.screenX);
  utils.setPreference("mozmill.screenY", window.screenY);
  utils.setPreference("mozmill.width", window.document.documentElement.clientWidth);
  utils.setPreference("mozmill.height", window.document.documentElement.clientHeight);
}
