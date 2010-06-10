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

var EXPORTED_SYMBOLS = ["mozmill"];

var mozmill = Components.utils.import('resource://mozmill/modules/mozmill.js');
var utils = {}; Components.utils.import('resource://mozmill/modules/utils.js', utils);
var enumerator = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                   .getService(Components.interfaces.nsIWindowMediator)
                   .getEnumerator("");



while(enumerator.hasMoreElements()) {
  var win = enumerator.getNext();
  win.documentLoaded = true;
  
  try {
    win.content.documentLoaded = true;
  } catch(e){}

  win.addEventListener("DOMContentLoaded", function(event) {
    win.documentLoaded = true;
    
    //try attaching a listener to the dom content for load and beforeunload
    //so that we can properly set the documentLoaded flag
    try {
      win.content.addEventListener("load", function(event) {
        win.content.documentLoaded = true;
      }, false);
      win.content.addEventListener("beforeunload", function(event) {
        win.content.documentLoaded = false;
      }, false);
    } catch(err){}

  }, false);
  
};

//when a new dom window gets opened
var observer = {
  observe: function(subject,topic,data){
      subject.addEventListener("DOMContentLoaded", function(event) {
        subject.documentLoaded = true;
        
        //try attaching a listener to the dom content for load and beforeunload
        //so that we can properly set the documentLoaded flag
        try {
          subject.content.addEventListener("load", function(event) {
            subject.content.documentLoaded = true;
          }, false);
          subject.content.addEventListener("beforeunload", function(event) {
            subject.content.documentLoaded = false;
          }, false);
        } catch(err){}

      }, false);  
  }
};

var observerService =
  Components.classes["@mozilla.org/observer-service;1"]
    .getService(Components.interfaces.nsIObserverService);

observerService.addObserver(observer, "toplevel-window-ready", false);

