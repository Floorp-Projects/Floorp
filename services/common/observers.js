/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["Observers"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

/**
 * A service for adding, removing and notifying observers of notifications.
 * Wraps the nsIObserverService interface.
 *
 * @version 0.2
 */
var Observers = {
  /**
   * Register the given callback as an observer of the given topic.
   *
   * @param   topic       {String}
   *          the topic to observe
   *
   * @param   callback    {Object}
   *          the callback; an Object that implements nsIObserver or a Function
   *          that gets called when the notification occurs
   *
   * @param   thisObject  {Object}  [optional]
   *          the object to use as |this| when calling a Function callback
   *
   * @returns the observer
   */
  add(topic, callback, thisObject) {
    let observer = new Observer(topic, callback, thisObject);
    this._cache.push(observer);
    Services.obs.addObserver(observer, topic, true);

    return observer;
  },

  /**
   * Unregister the given callback as an observer of the given topic.
   *
   * @param topic       {String}
   *        the topic being observed
   *
   * @param callback    {Object}
   *        the callback doing the observing
   *
   * @param thisObject  {Object}  [optional]
   *        the object being used as |this| when calling a Function callback
   */
  remove(topic, callback, thisObject) {
    // This seems fairly inefficient, but I'm not sure how much better
    // we can make it.  We could index by topic, but we can't index by callback
    // or thisObject, as far as I know, since the keys to JavaScript hashes
    // (a.k.a. objects) can apparently only be primitive values.
    let [observer] = this._cache.filter(v => v.topic == topic &&
                                             v.callback == callback &&
                                             v.thisObject == thisObject);
    if (observer) {
      Services.obs.removeObserver(observer, topic);
      this._cache.splice(this._cache.indexOf(observer), 1);
    } else {
      throw new Error("Attempt to remove non-existing observer");
    }
  },

  /**
   * Notify observers about something.
   *
   * @param topic   {String}
   *        the topic to notify observers about
   *
   * @param subject {Object}  [optional]
   *        some information about the topic; can be any JS object or primitive
   *
   * @param data    {String}  [optional] [deprecated]
   *        some more information about the topic; deprecated as the subject
   *        is sufficient to pass all needed information to the JS observers
   *        that this module targets; if you have multiple values to pass to
   *        the observer, wrap them in an object and pass them via the subject
   *        parameter (i.e.: { foo: 1, bar: "some string", baz: myObject })
   */
  notify(topic, subject, data) {
    subject = (typeof subject == "undefined") ? null : new Subject(subject);
       data = (typeof data == "undefined") ? null : data;
    Services.obs.notifyObservers(subject, topic, data);
  },

  /**
   * A cache of observers that have been added.
   *
   * We use this to remove observers when a caller calls |remove|.
   *
   * XXX This might result in reference cycles, causing memory leaks,
   * if we hold a reference to an observer that holds a reference to us.
   * Could we fix that by making this an independent top-level object
   * rather than a property of this object?
   */
  _cache: []
};


function Observer(topic, callback, thisObject) {
  this.topic = topic;
  this.callback = callback;
  this.thisObject = thisObject;
}

Observer.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]),
  observe(subject, topic, data) {
    // Extract the wrapped object for subjects that are one of our wrappers
    // around a JS object.  This way we support both wrapped subjects created
    // using this module and those that are real XPCOM components.
    if (subject && typeof subject == "object" &&
        ("wrappedJSObject" in subject) &&
        ("observersModuleSubjectWrapper" in subject.wrappedJSObject))
      subject = subject.wrappedJSObject.object;

    if (typeof this.callback == "function") {
      if (this.thisObject)
        this.callback.call(this.thisObject, subject, data);
      else
        this.callback(subject, data);
    } else // typeof this.callback == "object" (nsIObserver)
      this.callback.observe(subject, topic, data);
  }
};


function Subject(object) {
  // Double-wrap the object and set a property identifying the wrappedJSObject
  // as one of our wrappers to distinguish between subjects that are one of our
  // wrappers (which we should unwrap when notifying our observers) and those
  // that are real JS XPCOM components (which we should pass through unaltered).
  this.wrappedJSObject = { observersModuleSubjectWrapper: true, object };
}

Subject.prototype = {
  QueryInterface: ChromeUtils.generateQI([]),
  getScriptableHelper() {},
  getInterfaces() {}
};
