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
var objects = {}; Components.utils.import('resource://mozmill/stdlib/objects.js', objects);
var arrays = {}; Components.utils.import('resource://mozmill/stdlib/arrays.js', arrays);
var events = {}; Components.utils.import('resource://mozmill/modules/events.js', events);
// var logging = {}; Components.utils.import('resource://mozmill/stdlib/logging.js', logging);
var controller = {};  Components.utils.import('resource://mozmill/modules/controller.js', controller);

// var recorderLogger = logging.getLogger('recorderLogger');

var currentRecorderArray = [];

var getEventSet = function (eArray) {
  var inSet = function (a, c) {
    for each(x in a) {
      if (x.evt.timeStamp == c.evt.timeStamp && c.evt.type == x.evt.type) {
        return true;
      }
    }
    return false;
  }
  
  var returnArray = [];
  for each(e in eArray) {
    // recorderLogger.info('ts '+e.evt.timeStamp+' '+inSet(returnArray, e))
    if (!inSet(returnArray, e)) {
      returnArray.push(e);
    }
  }
  return returnArray;
}

var recorderMethodCases = {
  'click': function (x) {return 'click('+x['inspected'].elementText+')';},
  //this code is causing issues when recording type
  //and also requires the same code as the DX for genating accurate keypress
  //based on the new VK_ keyCode API
  
  'keypress': function (x) {
    if (x['evt'].charCode == 0){
      return 'keypress(' + x['inspected'].elementText + ',' + x['evt'].keyCode + ', {ctrlKey:' +x['evt'].ctrlKey 
              + ', altKey:'+ x['evt'].altKey + ',shiftKey:' + x['evt'].shiftKey + ', metaKey:' + x['evt'].metaKey + '})';
    }
    else {
      return 'keypress(' + x['inspected'].elementText + ',"' + String.fromCharCode(x['evt'].charCode) + '", {ctrlKey:' +x['evt'].ctrlKey 
              + ', altKey:'+ x['evt'].altKey + ',shiftKey:' + x['evt'].shiftKey + ', metaKey:' + x['evt'].metaKey + '})'; 
    }
   },
  'change': function (x) {return 'type('+x['inspected'].elementText+',"'+x['evt'].target.value+'")';},
  'dblclick': function (x) { return 'doubleClick('+x['inspected'].elementText+')';},
}

var cleanupEventsArray = function (recorder_array) {
  var indexesForRemoval = [];
  var type_indexes = [x['evt'].type for each(x in recorder_array)];
  
  // Convert a set of keypress events to a single type event
  if (arrays.inArray(type_indexes, 'change')) {
    var offset = 0;
    while (arrays.indexOf(type_indexes, 'change', offset) != -1) {
      var eIndex = arrays.indexOf(type_indexes, 'change', offset);
      var e = recorder_array[eIndex];
      if (e['evt'].target.value != undefined && arrays.compare(e['evt'].target.value, 
        [String.fromCharCode(x['evt'].charCode) for 
        each(x in recorder_array.slice(eIndex - (e['evt'].target.value.length + 1) ,eIndex - 1))
        ])) {
          var i = eIndex - (e['evt'].target.value.length + 1)
          while (i < eIndex) {
            indexesForRemoval.push(i); i++;            
          }
        } else if (e['evt'].target.value != undefined && arrays.compare(e['evt'].target.value, 
        [String.fromCharCode(x['evt'].charCode) for 
        each(x in recorder_array.slice(eIndex - (e['evt'].target.value.length) ,eIndex - 1))
        ])) {
          var i = eIndex - (e['evt'].target.value.length )
          while (i < eIndex) {
            indexesForRemoval.push(i); i++;
          }
        }
      var offset = arrays.indexOf(type_indexes, 'change', offset) + 1;
    }
  }
  
  // Remove change events with '' as value
  for (i in recorder_array) {
    var x = recorder_array[i];
    if (x['evt'].type ==  "change") {
      if (x['evt'].target.value == '') {
        indexesForRemoval.push(i);
      }
    }
  }
  
  // Cleanup trailing cmd+~
  try {
    var i = 1;
    while (recorder_array[recorder_array.length - i]['inspected'].controllerText == 'new mozmill.controller.MozMillController(mozmill.utils.getWindowByTitle("MozMill IDE"))') {
      i++;
      if (recorder_array[recorder_array.length - i]['evt'].charCode == 96) {
        indexesForRemoval.push(recorder_array.length - i);
      }   
    }
  } catch(err){}
  
  // Remove any actions in the mozmill window
  for (i in recorder_array) {
    var inspected = recorder_array[i]['inspected'];
    if (inspected.controllerText == 'new mozmill.controller.MozMillController(mozmill.utils.getWindowByTitle("MozMill IDE"))') {
      indexesForRemoval.push(i);
    }
  }
  
  var narray = [];
  for (i in recorder_array) {
    if (!arrays.inArray(indexesForRemoval, i)) {
      narray.push(recorder_array[i]);
    }
  }
  
  return narray;
}

var getRecordedScript = function (recorder_array) {
  var setup = {};
  var test = [];
  
  var recorder_array = cleanupEventsArray(getEventSet(recorder_array));
  
  for each(x in recorder_array) {
    var inspected = x['inspected'];
    if (!setup[inspected.controllerText]) {
      if (objects.getLength(setup) > 0) {
        var ext = String(objects.getLength(setup) + 1);
      } else {
        var ext = '';
      }
      setup[inspected.controllerText] = 'controller'+ext
    }
    if (recorderMethodCases[x['evt'].type] == undefined) {
      alert("Don't have a case for event type: "+x['evt'].type)
    }
    var methodString = recorderMethodCases[x['evt'].type](x).replace(inspected.documentString, inspected.documentString.replace('controller.', setup[inspected.controllerText]+'.'))

    if (x['evt'].button == 2){
      methodString = methodString.replace("click", "rightClick");
    }
    test.push(setup[inspected.controllerText]  + '.' + methodString + ';');
  }
  
  var rscript = ['var setupModule = function(module) {',];
  for (i in setup) {
    rscript.push("  "+setup[i]+' = '+i+';')
  }
  rscript.push('}')
  rscript.push('')
  rscript.push('var testRecorded = function () {')
  for each(t in test){
    rscript.push('  '+t);
  }
  rscript.push('}')
  return rscript.join('\n');
}

var RecorderConnector = function() {
  this.lastEvent = null;
  this.ison = false;
}

RecorderConnector.prototype.toggle = function(){
  if (this.ison)
    this.off();
  else
    this.on();
};

RecorderConnector.prototype.dispatch = function(evt){
  currentRecorderArray.push({'evt':evt, 'inspected':inspection.inspectElement(evt)});
  var value = editor.getContent();
  value += (evt.type + ':: ' + evt.timeStamp + '\n');
  editor.setContent(value);
}

//Recursively bind to all the iframes and frames within
RecorderConnector.prototype.bindListeners = function(frame) {
  frame.addEventListener('click', this.dispatch, false);
  frame.addEventListener('dblclick', this.dispatch, false);
  frame.addEventListener('change', this.dispatch, false);
  frame.addEventListener('keypress', this.dispatch, false);

  var iframeCount = frame.window.frames.length;
  var iframeArray = frame.window.frames;

  for (var i = 0; i < iframeCount; i++)
    this.bindListeners(iframeArray[i]);
}

//Recursively bind to all the iframes and frames within
RecorderConnector.prototype.unbindListeners = function(frame) {
  try {
    frame.removeEventListener('click', this.dispatch, false);
    frame.removeEventListener('dblclick', this.dispatch, false);
    frame.removeEventListener('change', this.dispatch, false);
    frame.removeEventListener('keypress', this.dispatch, false);
  }
  catch(error) {}
  
  var iframeCount = frame.window.frames.length;
  var iframeArray = frame.window.frames;

  for (var i = 0; i < iframeCount; i++)
    this.unbindListeners(iframeArray[i]);
}

//When a new win dom window gets opened
RecorderConnector.prototype.observer = {
  observe: function(subject,topic,data){
    var defer = function(){
      controller.waitForEval("subject.documentLoaded == true", 10000, 100, subject)
      MozMillrec.bindListeners(subject);
    }
    window.setTimeout(defer, 500);
  }
};

RecorderConnector.prototype.on = function() {
  this.ison = true;

  // update UI
  $("#recordToggle").text("Stop Recording");
  $("#recordToggle").addClass("ui-state-highlight");
  $("#recordToggle").addClass("ui-priority-primary");

  newFile();
  
  for each(win in utils.getWindows()) {
    if (win.document.title != "MozMill IDE"){
      this.bindListeners(win);
    }
  }

  var mmWindows = utils.getWindows('navigator:browser');
  if (mmWindows.length != 0){
    mmWindows[0].focus();
  }
  
  var observerService =
    Components.classes["@mozilla.org/observer-service;1"]
      .getService(Components.interfaces.nsIObserverService);
  
  //Attach the new window open listener
  observerService.addObserver(this.observer, "toplevel-window-ready", false);
  
  currentRecorderArray = [];
};

RecorderConnector.prototype.off = function() {
  this.ison = false;

  //update UI
  $("#recordToggle").text("Record");
  $("#recordToggle").removeClass("ui-state-highlight");
  $("#recordToggle").removeClass("ui-priority-primary");

  
  for each(win in utils.getWindows()) {
    this.unbindListeners(win);
  }
  var r = getRecordedScript(currentRecorderArray);

  editor.setContent(r);
  currentRecorderArray = [];
  //remove new window listener
  var observerService =
    Components.classes["@mozilla.org/observer-service;1"]
      .getService(Components.interfaces.nsIObserverService);
  try { observerService.removeObserver(this.observer, "toplevel-window-ready"); }
  catch(err){}
};

var MozMillrec = new RecorderConnector();

// Scoping bug workarounds
var enableRec = function () {
  MozMillrec.on();
}
var disableRec = function () {
  MozMillrec.off();
}
