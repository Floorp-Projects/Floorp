"use strict";

var tests = [];

function waitForCondition(condition, nextTest, errorMsg) {
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= 30) {
      ok(false, errorMsg);
      moveOn();
    }
    var conditionPassed;
    try {
      conditionPassed = condition();
    } catch (e) {
      ok(false, e + "\n" + e.stack);
      conditionPassed = false;
    }
    if (conditionPassed) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() { clearInterval(interval); nextTest(); };
}

function getAnonElementWithinVideoByAttribute(video, aName, aValue) {
  const domUtils = SpecialPowers.Cc["@mozilla.org/inspector/dom-utils;1"].
    getService(SpecialPowers.Ci.inIDOMUtils);
  const videoControl = domUtils.getChildrenForNode(video, true)[1];

  return SpecialPowers.wrap(videoControl.ownerDocument)
    .getAnonymousElementByAttribute(videoControl, aName, aValue);
}

function executeTests() {
  return tests
    .map(fn => () => new Promise(fn))
    .reduce((promise, task) => promise.then(task), Promise.resolve());
}
