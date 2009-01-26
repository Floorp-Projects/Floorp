#!/usr/bin/js

// mock objects
function alert(str) {
  print(str);
}
function dump(str) {
  print(str);
}
window = new Object();
document = new Object();
document.createEvent = function(str) {
  obj = new Object();
  obj.initMouseEvent = function() {}
  return obj;
}
document.getElementById = function(str) {
  obj = new Object(); 
  if (str == 'contentPageloader') {
    obj.content = new Object();
    obj.content.addEventListener = function() {}
    obj.content.removeEventListener = function() {}
    obj.content.loadURI = function() {}
    return obj.content;
  } else if (str == 'plStartButton') {
    obj.startButton = new Object();
    obj.startButton.setAttribute = function(key, value) {}
    return obj.startButton;
  }
}
evt = new Object();
evt.type = 'load';
window.setTimeout = function() {}
this.content = document.getElementById('content');

// dummy data
pages = ['http://google.com'];

load(['chrome/content/pageloader.js']);
load(['chrome/content/report.js']);

plInit(true);
plInit(false);
for (cycle = 0; cycle < NUM_CYCLES*2; cycle++) {
  plLoadHandler(evt);
}
