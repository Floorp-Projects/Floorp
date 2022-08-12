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

class EventLogger {
  constructor(expectedNumberOfEvents = Number.MAX_VALUE) {
    this._log = [];
    this._eventsPromise = new Promise(r => (this._countReached = r));
    this._expectedNumberOfEvents = expectedNumberOfEvents;
  }
  handleEvent(event) {
    this._log.push(event);
    if (this._log.length >= this._expectedNumberOfEvents) {
      this._countReached(this._log);
    }
  }
  get log() {
    return this._log;
  }
  waitForExpectedEvents() {
    return this._eventsPromise;
  }
}
