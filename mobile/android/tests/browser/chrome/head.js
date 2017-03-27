/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function fuzzyEquals(a, b) {
  return (Math.abs(a - b) < 1e-6);
}

function promiseBrowserEvent(browser, eventType) {
  return new Promise((resolve) => {
    function handle(event) {
      // Since we'll be redirecting, don't make assumptions about the given URL and the loaded URL
      if (event.target != browser.contentDocument || event.target.location.href == "about:blank") {
        info("Skipping spurious '" + eventType + "' event" + " for " + event.target.location.href);
        return;
      }
      info("Received event " + eventType + " from browser");
      browser.removeEventListener(eventType, handle, true);
      resolve(event);
    }

    browser.addEventListener(eventType, handle, true);
    info("Now waiting for " + eventType + " event from browser");
  });
}

function promiseTabEvent(container, eventType) {
  return new Promise((resolve) => {
    function handle(event) {
      info("Received event " + eventType + " from container");
      container.removeEventListener(eventType, handle, true);
      resolve(event);
    }

    container.addEventListener(eventType, handle, true);
    info("Now waiting for " + eventType + " event from container");
  });
}

function promiseNotification(topic) {
  Cu.import("resource://gre/modules/Services.jsm");

  return new Promise((resolve, reject) => {
    function observe(subject, topic, data) {
      info("Received " + topic + " notification from Gecko");
      Services.obs.removeObserver(observe, topic);
      resolve();
    }
    Services.obs.addObserver(observe, topic, false);
    info("Now waiting for " + topic + " notification from Gecko");
  });
}

function promiseLinkVisit(url) {
  Cu.import("resource://gre/modules/Services.jsm");

  var topic = "link-visited";
  return new Promise((resolve, reject) => {
    function observe(subject, topic, data) {
      info("Received " + topic + " notification from Gecko");
      var uri = subject.QueryInterface(Ci.nsIURI);
      if (uri.spec != url) {
        info("Visited URL " + uri.spec + " is not desired URL " + url + "; ignoring.");
        return;
      }
      info("Visited URL " + uri.spec + " is desired URL " + url);
      Services.obs.removeObserver(observe, topic);
      resolve();
    };
    Services.obs.addObserver(observe, topic, false);
    info("Now waiting for " + topic + " notification from Gecko with URL " + url);
  });
}

function makeObserver(observerId) {
  let deferred = Promise.defer();

  let ret = {
    id: observerId,
    count: 0,
    promise: deferred.promise,
    observe: function (subject, topic, data) {
      ret.count += 1;
      let msg = { subject: subject,
                  topic: topic,
                  data: data };
      deferred.resolve(msg);
    },
  };

  return ret;
};

