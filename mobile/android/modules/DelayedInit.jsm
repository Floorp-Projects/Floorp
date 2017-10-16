/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* globals MessageLoop */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = ["DelayedInit"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "MessageLoop",
                                   "@mozilla.org/message-loop;1",
                                   "nsIMessageLoop");

/**
 * Use DelayedInit to schedule initializers to run some time after startup.
 * Initializers are added to a list of pending inits. Whenever the main thread
 * message loop is idle, DelayedInit will start running initializers from the
 * pending list. To prevent monopolizing the message loop, every idling period
 * has a maximum duration. When that's reached, we give up the message loop and
 * wait for the next idle.
 *
 * DelayedInit is compatible with lazy getters like those from XPCOMUtils. When
 * the lazy getter is first accessed, its corresponding initializer is run
 * automatically if it hasn't been run already. Each initializer also has a
 * maximum wait parameter that specifies a mandatory timeout; when the timeout
 * is reached, the initializer is forced to run.
 *
 *   DelayedInit.schedule(() => Foo.init(), null, null, 5000);
 *
 * In the example above, Foo.init will run automatically when the message loop
 * becomes idle, or when 5000ms has elapsed, whichever comes first.
 *
 *   DelayedInit.schedule(() => Foo.init(), this, "Foo", 5000);
 *
 * In the example above, Foo.init will run automatically when the message loop
 * becomes idle, when |this.Foo| is accessed, or when 5000ms has elapsed,
 * whichever comes first.
 *
 * It may be simpler to have a wrapper for DelayedInit.schedule. For example,
 *
 *   function InitLater(fn, obj, name) {
 *     return DelayedInit.schedule(fn, obj, name, 5000); // constant max wait
 *   }
 *   InitLater(() => Foo.init());
 *   InitLater(() => Bar.init(), this, "Bar");
 */
var DelayedInit = {
  schedule: function(fn, object, name, maxWait) {
    return Impl.scheduleInit(fn, object, name, maxWait);
  },

  scheduleList: function(fns, maxWait) {
    for (let fn of fns) {
      Impl.scheduleInit(fn, null, null, maxWait);
    }
  },
};

// Maximum duration for each idling period. Pending inits are run until this
// duration is exceeded; then we wait for next idling period.
const MAX_IDLE_RUN_MS = 50;

var Impl = {
  pendingInits: [],

  onIdle: function() {
    let startTime = Cu.now();
    let time = startTime;
    let nextDue;

    // Go through all the pending inits. Even if we don't run them,
    // we still need to find out when the next timeout should be.
    for (let init of this.pendingInits) {
      if (init.complete) {
        continue;
      }

      if (time - startTime < MAX_IDLE_RUN_MS) {
        init.maybeInit();
        time = Cu.now();
      } else {
        // We ran out of time; find when the next closest due time is.
        nextDue = nextDue ? Math.min(nextDue, init.due) : init.due;
      }
    }

    // Get rid of completed ones.
    this.pendingInits = this.pendingInits.filter((init) => !init.complete);

    if (nextDue !== undefined) {
      // Schedule the next idle, if we still have pending inits.
      MessageLoop.postIdleTask(() => this.onIdle(),
                               Math.max(0, nextDue - time));
    }
  },

  addPendingInit: function(fn, wait) {
    let init = {
      fn: fn,
      due: Cu.now() + wait,
      complete: false,
      maybeInit: function() {
        if (this.complete) {
          return false;
        }
        this.complete = true;
        this.fn.call();
        this.fn = null;
        return true;
      },
    };

    if (!this.pendingInits.length) {
      // Schedule for the first idle.
      MessageLoop.postIdleTask(() => this.onIdle(), wait);
    }
    this.pendingInits.push(init);
    return init;
  },

  scheduleInit: function(fn, object, name, wait) {
    let init = this.addPendingInit(fn, wait);

    if (!object || !name) {
      // No lazy getter needed.
      return;
    }

    // Get any existing information about the property.
    let prop = Object.getOwnPropertyDescriptor(object, name) ||
               { configurable: true, enumerable: true, writable: true };

    if (!prop.configurable) {
      // Object.defineProperty won't work, so just perform init here.
      init.maybeInit();
      return;
    }

    // Define proxy getter/setter that will call first initializer first,
    // before delegating the get/set to the original target.
    Object.defineProperty(object, name, {
      get: function proxy_getter() {
        init.maybeInit();

        // If the initializer actually ran, it may have replaced our proxy
        // property with a real one, so we need to reload he property.
        let newProp = Object.getOwnPropertyDescriptor(object, name);
        if (newProp.get !== proxy_getter) {
          // Set prop if newProp doesn't refer to our proxy property.
          prop = newProp;
        } else {
          // Otherwise, reset to the original property.
          Object.defineProperty(object, name, prop);
        }

        if (prop.get) {
          return prop.get.call(object);
        }
        return prop.value;
      },
      set: function(newVal) {
        init.maybeInit();

        // Since our initializer already ran,
        // we can get rid of our proxy property.
        if (prop.get || prop.set) {
          Object.defineProperty(object, name, prop);
          return prop.set.call(object);
        }

        prop.value = newVal;
        Object.defineProperty(object, name, prop);
        return newVal;
      },
      configurable: true,
      enumerable: true,
    });
  }
};
