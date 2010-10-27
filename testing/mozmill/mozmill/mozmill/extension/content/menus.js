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

var frame = {}; Components.utils.import('resource://mozmill/modules/frame.js', frame);


function getBasename(path){
  var splt = "/"
  if (navigator.platform.indexOf("Win") != -1){
    splt = "\\";
  }
  var pathArr = path.split(splt);
  return pathArr[pathArr.length-1]
}

function openFile(){
  var openObj = utils.openFile(window);
  if (openObj) {
    $("#tabs").tabs("select", 0);
    var index = editor.getTabForFile(openObj.path);
    if(index != -1)
      editor.switchTab(index);
    else
      editor.openNew(openObj.data, openObj.path);
  }
}

function saveAsFile() {
  var content = editor.getContent();
  var filename = utils.saveAsFile(window, content);
  if (filename) {
    $("#tabs").tabs("select", 0);
    editor.changeFilename(filename);
    saveToFile();
    return true;
  }
  return false;
}

function saveToFile() {
  var filename = editor.getFilename();
  var content = editor.getContent();
  utils.saveFile(window, content, filename);
  editor.onFileSaved();
}

function saveFile() {
  var filename = editor.getFilename();
  if(/mozmill\.utils\.temp/.test(filename))
    saveAsFile();
  else {
    saveToFile();
  }
}

function closeFile() {
  $("#tabs").tabs("select", 0);
  var really = confirm("Are you sure you want to close this file?");
  if (really == true)
    editor.closeCurrentTab();
}

function runFile(){
  var nsIFilePicker = Components.interfaces.nsIFilePicker;
  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  fp.init(window, "Select a File", nsIFilePicker.modeOpen);
  fp.appendFilter("JavaScript Files","*.js");
  var res = fp.show();
  if (res == nsIFilePicker.returnOK){
    $("#tabs").tabs("select", 1);
    frame.runTestFile(fp.file.path, true);
  }
  testFinished();
}

function runDirectory(){
  var nsIFilePicker = Components.interfaces.nsIFilePicker;
  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  fp.init(window, "Select a Directory", nsIFilePicker.modeGetFolder);
  var res = fp.show();
  if (res == nsIFilePicker.returnOK){
    $("#tabs").tabs("select", 1);
    frame.runTestDirectory(fp.file.path, true);
  }
  testFinished();
}

function runEditor(){
  saveToFile();
  var filename = editor.getFilename();
  frame.runTestFile(filename);
  testFinished();
}

function newFile(){
  editor.openNew();
}

function newTemplate(){
  var template = "var setupModule = function(module) {\n" +
   "  module.controller = mozmill.getBrowserController();\n" +
   "}\n" +
   "\n" +
   "var testFoo = function(){\n" +
   "  controller.open('http://www.google.com');\n" +
   "}\n";
  editor.openNew(template);
}

function tabSelected(selector) {
  editor.switchTab(selector.selectedIndex);
}

function openHelp() {
  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]  
                         .getService(Components.interfaces.nsIWindowMediator);  
  var browser = wm.getMostRecentWindow("navigator:browser").gBrowser;
  browser.selectedTab =
    browser.addTab("http://quality.mozilla.org/docs/mozmill/getting-started/");
}

