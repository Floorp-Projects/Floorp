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
  var moveOn = function() {
    clearInterval(interval);
    nextTest();
  };
}

function getElementWithinVideo(video, aValue) {
  const shadowRoot = SpecialPowers.wrap(video).openOrClosedShadowRoot;
  return shadowRoot.getElementById(aValue);
}

/**
 * Runs querySelectorAll on an element's shadow root.
 * @param {Element} element
 * @param {string} selector
 */
function shadowRootQuerySelectorAll(element, selector) {
  const shadowRoot = SpecialPowers.wrap(element).openOrClosedShadowRoot;
  return shadowRoot?.querySelectorAll(selector);
}

function executeTests() {
  return tests
    .map(fn => () => new Promise(fn))
    .reduce((promise, task) => promise.then(task), Promise.resolve());
}

function once(target, name, cb) {
  let p = new Promise(function(resolve, reject) {
    target.addEventListener(
      name,
      function() {
        resolve();
      },
      { once: true }
    );
  });
  if (cb) {
    p.then(cb);
  }
  return p;
}
