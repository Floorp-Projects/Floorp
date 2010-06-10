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

var inspection = {}; Components.utils.import('resource://mozmill/modules/inspection.js', inspection);
var utils = {}; Components.utils.import('resource://mozmill/modules/utils.js', utils);

var DomInspectorConnector = function() {
  this.lastEvent = null;
  this.lastTime = null;
  this.on = false;
}
DomInspectorConnector.prototype.grab = function(){
  var disp = $('dxDisplay').textContent;
  var dispArr = disp.split(': ');
  $('editorInput').value += 'new elementslib.'+dispArr[0].toUpperCase()+"('"+dispArr[1]+"')\n";
}  

DomInspectorConnector.prototype.changeClick = function(e) {
  if (this.on){
    this.dxOff()
    this.dxOn();
  }
  else {
    this.dxOff();
  }

}

DomInspectorConnector.prototype.evtDispatch = function(e) {
  
  //if this function was called less than a second ago, exit
  //this should solve the flickering problem
  var currentTime = new Date();
  var newTime = currentTime.getTime();
  
  if (this.lastTime != null){
    var timeDiff = newTime - this.lastTime;
    this.lastTime = newTime;
        
    if (timeDiff < 2){
      this.lastEvent = e;
      return;
    }
  } else { this.lastTime = newTime; }
  
  //Fix the scroll bar exception Bug 472124
  try { var i = inspection.inspectElement(e); }
  catch(err){ return; }
  
  var dxC = i.controllerText;
  var dxE = i.elementText;
  var dxV = String(i.validation);

  document.getElementById('dxController').innerHTML = dxC;
  document.getElementById('dxValidation').innerHTML = dxV;
  document.getElementById('dxElement').innerHTML = dxE;

  return dxE;
}
DomInspectorConnector.prototype.dxToggle = function(){
  if (this.on){
    this.dxOff();
    $("#inspectDialog").dialog().parents(".ui-dialog:first").find(".ui-dialog-buttonpane button")[2].innerHTML = "Start";
  }
  else{
    this.dxOn();
    $("#inspectDialog").dialog().parents(".ui-dialog:first").find(".ui-dialog-buttonpane button")[2].innerHTML = "Stop";
  }
}
//Turn on the recorder
//Since the click event does things like firing twice when a double click goes also
//and can be obnoxious im enabling it to be turned off and on with a toggle check box
DomInspectorConnector.prototype.dxOn = function() {
  this.on = true;
  $("#inspectDialog").dialog().parents(".ui-dialog:first").find(".ui-dialog-buttonpane button")[2].innerHTML = "Stop";
  document.getElementById('eventsOut').value = "";
  //document.getElementById('inspectClickSelection').disabled = true;
  //$('domExplorer').setAttribute('label', 'Disable Inspector');
  //defined the click method, default to dblclick
  var clickMethod = "dblclick";
  if (document.getElementById('inspectSingle').checked){
    clickMethod = 'click';
  }
  //document.getElementById('dxContainer').style.display = "block";
  //var w = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService(Components.interfaces.nsIWindowMediator).getMostRecentWindow('');
  var enumerator = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator)
                     .getEnumerator("");
  while(enumerator.hasMoreElements()) {
    var win = enumerator.getNext();
    //if (win.title != 'Error Console' && win.title != 'MozMill IDE'){
    if (win.document.title != 'MozMill IDE'){
      this.dxRecursiveBind(win, clickMethod);
      //win.focus();
    }
  }

  var observerService =
    Components.classes["@mozilla.org/observer-service;1"]
      .getService(Components.interfaces.nsIObserverService);

  observerService.addObserver(this.observer, "toplevel-window-ready", false);
};

//when a new dom window gets opened
DomInspectorConnector.prototype.observer = {
  observe: function(subject,topic,data){
    var clickMethod = "dblclick";
    if ($('inspectSingle').selected){
      clickMethod = 'click';
    }
    //Attach listener to new window here
    MozMilldx.dxRecursiveBind(subject, clickMethod);
  }
};

DomInspectorConnector.prototype.dxOff = function() {
  this.on = false;
  $("#inspectDialog").dialog().parents(".ui-dialog:first").find(".ui-dialog-buttonpane button")[2].innerHTML = "Start";
  //document.getElementById('inspectClickSelection').disabled = false;

  //try to cleanup left over outlines
  if (this.lastEvent){
    this.lastEvent.target.style.border = "";
  }
  
  //defined the click method, default to dblclick
  var clickMethod = "dblclick";
  if (document.getElementById('inspectSingle').checked){
    clickMethod = 'click';
  }
  
  //because they share this box
  var copyOutputBox = $('copyout');
  //$('domExplorer').setAttribute('label', 'Enable Inspector');
  //$('dxContainer').style.display = "none";
  //var w = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService(Components.interfaces.nsIWindowMediator).getMostRecentWindow('');
  for each(win in utils.getWindows()) {
    this.dxRecursiveUnBind(win, 'click');
  }
  
  for each(win in utils.getWindows()) {
    this.dxRecursiveUnBind(win, 'dblclick');
  }
  

  var observerService =
    Components.classes["@mozilla.org/observer-service;1"]
      .getService(Components.interfaces.nsIObserverService);

  try { 
    observerService.removeObserver(this.observer, "toplevel-window-ready");
  } catch(err){}

};

DomInspectorConnector.prototype.getFoc = function(e){
  MozMilldx.dxToggle();
  e.target.style.border = "";
  e.stopPropagation();
  e.preventDefault();
  window.focus();
}

DomInspectorConnector.prototype.inspectorToClipboard = function(){
  copyToClipboard($('#dxController')[0].innerHTML +'\n'+$('#dxElement')[0].innerHTML);
};

//Copy inspector output to clipboard if alt,shift,c is pressed
DomInspectorConnector.prototype.clipCopy = function(e){
   if (e == true){
     copyToClipboard($('#dxElement')[0].innerHTML + ' '+$('#dxValidation')[0].innerHTML + ' ' + $('#dxController')[0].innerHTML);
   }
   else if (e.altKey && e.shiftKey && (e.charCode == 199)){
     copyToClipboard($('#dxElement')[0].innerHTML + ' '+$('#dxValidation')[0].innerHTML + ' ' + $('#dxController')[0].innerHTML);
   }
   else {
     window.document.getElementById('eventsOut').value += "-----\n";
     window.document.getElementById('eventsOut').value += "Shift Key: "+ e.shiftKey + "\n";
     window.document.getElementById('eventsOut').value += "Control Key: "+ e.ctrlKey + "\n";
     window.document.getElementById('eventsOut').value += "Alt Key: "+ e.altKey + "\n";
     window.document.getElementById('eventsOut').value += "Meta Key: "+ e.metaKey + "\n\n";
     
     //we need some code here to map keyCodes to the desired mozilla VK_WHATEVER
     //and substituting that in for e.charCode in the following concat.
     
     var ctrlString = "";
     ctrlString += MozMilldx.evtDispatch(e);
     ctrlString += "\nController: controller.keypress(element,"+e.charCode+",";
     ctrlString += "{";
     if (e.ctrlKey){
       ctrlString += "ctrlKey:"+e.ctrlKey.toString()+",";
     }
     if (e.altKey){
       ctrlString += "altKey:"+e.altKey.toString()+",";
     }
     if (e.shiftKey){
       ctrlString += "shiftKey:"+e.shiftKey.toString()+",";
     }
     if (e.metaKey){
       ctrlString += "metaKey:"+e.metaKey.toString();
     }
     ctrlString += "});\n";
     ctrlString = ctrlString.replace(/undefined/g, "false");         
     document.getElementById('eventsOut').value += ctrlString;
   }
}
//Recursively bind to all the iframes and frames within
DomInspectorConnector.prototype.dxRecursiveBind = function(frame, clickMethod) {
  //Make sure we haven't already bound anything to this frame yet
  this.dxRecursiveUnBind(frame, clickMethod);
  
  frame.addEventListener('mouseover', this.evtDispatch, true);
  frame.addEventListener('mouseout', this.evtDispatch, true);
  frame.addEventListener(clickMethod, this.getFoc, true);
  frame.addEventListener('keypress', this.clipCopy, true);
  
  
  var iframeCount = frame.window.frames.length;
  var iframeArray = frame.window.frames;

  for (var i = 0; i < iframeCount; i++)
  {
      try {
        iframeArray[i].addEventListener('mouseover', this.evtDispatch, true);
        iframeArray[i].addEventListener('mouseout', this.evtDispatch, true);
        iframeArray[i].addEventListener(clickMethod, this.getFoc, true);
        iframeArray[i].addEventListener('keypress', this.clipCopy, true);
        

        this.dxRecursiveBind(iframeArray[i], clickMethod);
      }
      catch(error) {
          //mozmill.results.writeResult('There was a problem binding to one of your iframes, is it cross domain?' + 
          //'Binding to all others.' + error);

      }
  }
}
//Recursively bind to all the iframes and frames within
DomInspectorConnector.prototype.dxRecursiveUnBind = function(frame, clickMethod) {
  frame.removeEventListener('mouseover', this.evtDispatch, true);
  frame.removeEventListener('mouseout', this.evtDispatch, true);
  frame.removeEventListener(clickMethod, this.getFoc, true);
  frame.removeEventListener('keypress', this.clipCopy, true);
  
  
  var iframeCount = frame.window.frames.length;
  var iframeArray = frame.window.frames;

  for (var i = 0; i < iframeCount; i++)
  {
      try {
        iframeArray[i].removeEventListener('mouseover', this.evtDispatch, true);
        iframeArray[i].removeEventListener('mouseout', this.evtDispatch, true);
        iframeArray[i].removeEventListener(clickMethod, this.getFoc, true);
        iframeArray[i].removeEventListener('keypress', this.clipCopy, true);

        this.dxRecursiveUnBind(iframeArray[i], clickMethod);
      }
      catch(error) {
          //mozmill.results.writeResult('There was a problem binding to one of your iframes, is it cross domain?' + 
          //'Binding to all others.' + error);
      }
  }
}

var MozMilldx = new DomInspectorConnector();

// Scoping bug workarounds
var enableDX = function () {
  MozMilldx.dxOn();
}
var disableDX = function () {
  MozMilldx.dxOff();
}