# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


// A couple of classes to simplify creating observers. 
//
// // Example1:
//
// function doSomething() { ... }
// var observer = new G_ObserverWrapper(topic, doSomething);
// someObj.addObserver(topic, observer);
//
// // Example2: 
//
// function doSomething() { ... }
// new G_ObserverServiceObserver("profile-after-change", 
//                               doSomething,
//                               true /* run only once */);


/**
 * This class abstracts the admittedly simple boilerplate required of
 * an nsIObserver. It saves you the trouble of implementing the
 * indirection of your own observe() function.
 *
 * @param topic String containing the topic the observer will filter for
 *
 * @param observeFunction Reference to the function to call when the 
 *                        observer fires
 *
 * @constructor
 */
function G_ObserverWrapper(topic, observeFunction) {
  this.debugZone = "observer";
  this.topic_ = topic;
  this.observeFunction_ = observeFunction;
}

/**
 * XPCOM
 */
G_ObserverWrapper.prototype.QueryInterface = function(iid) {
  if (iid.equals(Ci.nsISupports) || iid.equals(Ci.nsIObserver))
    return this;
  throw Components.results.NS_ERROR_NO_INTERFACE;
}

/**
 * Invoked by the thingy being observed
 */
G_ObserverWrapper.prototype.observe = function(subject, topic, data) {
  if (topic == this.topic_)
    this.observeFunction_(subject, topic, data);
}


/**
 * This class abstracts the admittedly simple boilerplate required of
 * observing an observerservice topic. It implements the indirection
 * required, and automatically registers to hear the topic.
 *
 * @param topic String containing the topic the observer will filter for
 *
 * @param observeFunction Reference to the function to call when the 
 *                        observer fires
 *
 * @param opt_onlyOnce Boolean indicating if the observer should unregister
 *                     after it has fired
 *
 * @constructor
 */
function G_ObserverServiceObserver(topic, observeFunction, opt_onlyOnce) {
  this.debugZone = "observerserviceobserver";
  this.topic_ = topic;
  this.observeFunction_ = observeFunction;
  this.onlyOnce_ = !!opt_onlyOnce;
  
  this.observer_ = new G_ObserverWrapper(this.topic_, 
                                         BindToObject(this.observe_, this));
  this.observerService_ = Cc["@mozilla.org/observer-service;1"]
                          .getService(Ci.nsIObserverService);
  this.observerService_.addObserver(this.observer_, this.topic_, false);
}

/**
 * Unregister the observer from the observerservice
 */
G_ObserverServiceObserver.prototype.unregister = function() {
  this.observerService_.removeObserver(this.observer_, this.topic_);
  this.observerService_ = null;
}

/**
 * Invoked by the observerservice
 */
G_ObserverServiceObserver.prototype.observe_ = function(subject, topic, data) {
  this.observeFunction_(subject, topic, data);
  if (this.onlyOnce_)
    this.unregister();
}

#ifdef DEBUG
function TEST_G_Observer() {
  if (G_GDEBUG) {

    var z = "observer UNITTEST";
    G_debugService.enableZone(z);

    G_Debug(z, "Starting");

    var regularObserverRan = 0;
    var observerServiceObserverRan = 0;

    function regularObserver() {
      regularObserverRan++;
    };

    function observerServiceObserver() {
      observerServiceObserverRan++;
    };

    var service = Cc["@mozilla.org/observer-service;1"]
                  .getService(Ci.nsIObserverService);
    var topic = "google-observer-test";

    var o1 = new G_ObserverWrapper(topic, regularObserver);
    service.addObserver(o1, topic, false);

    new G_ObserverServiceObserver(topic, 
                                  observerServiceObserver, true /* once */);

    // Notifications happen synchronously, so this is easy
    service.notifyObservers(null, topic, null);
    service.notifyObservers(null, topic, null);

    G_Assert(z, regularObserverRan == 2, "Regular observer broken");
    G_Assert(z, observerServiceObserverRan == 1, "ObsServObs broken");

    service.removeObserver(o1, topic);
    G_Debug(z, "PASSED");
  }
}
#endif
