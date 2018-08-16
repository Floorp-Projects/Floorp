"use strict";

const InspectorUtils = SpecialPowers.InspectorUtils;

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
  // <videocontrols> is the second anonymous child node of <video>, but
  // the first child node of <audio>.
  const videoControlIndex = video.nodeName == "VIDEO" ? 1 : 0;
  const videoControl = InspectorUtils.getChildrenForNode(video, true)[videoControlIndex];

  return videoControl.ownerDocument
    .getAnonymousElementByAttribute(videoControl, aName, aValue);
}

function executeTests() {
  return tests
    .map(fn => () => new Promise(fn))
    .reduce((promise, task) => promise.then(task), Promise.resolve());
}
