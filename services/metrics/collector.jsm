/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

#ifndef MERGED_COMPARTMENT
this.EXPORTED_SYMBOLS = ["Collector"];

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/services/metrics/dataprovider.jsm");
#endif

Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-common/utils.js");


/**
 * Handles and coordinates the collection of metrics data from providers.
 *
 * This provides an interface for managing `Metrics.Provider` instances. It
 * provides APIs for bulk collection of data.
 */
this.Collector = function (storage) {
  this._log = Log4Moz.repository.getLogger("Services.Metrics.Collector");

  this._providers = new Map();
  this._storage = storage;

  this._providerInitQueue = [];
  this._providerInitializing = false;
  this.providerErrors = new Map();
}

Collector.prototype = Object.freeze({
  get providers() {
    let providers = [];
    for (let [name, entry] of this._providers) {
      providers.push(entry.provider);
    }

    return providers;
  },

  /**
   * Registers a `MetricsProvider` with this collector.
   *
   * Once a `MetricsProvider` is registered, data will be collected from it
   * whenever we collect data.
   *
   * The returned value is a promise that will be resolved once registration
   * is complete.
   *
   * Providers are initialized as part of registration by calling
   * provider.init().
   *
   * @param provider
   *        (Metrics.Provider) The provider instance to register.
   *
   * @return Promise<null>
   */
  registerProvider: function (provider) {
    if (!(provider instanceof Provider)) {
      throw new Error("Argument must be a Provider instance.");
    }

    if (this._providers.has(provider.name)) {
      return Promise.resolve();
    }

    let deferred = Promise.defer();
    this._providerInitQueue.push([provider, deferred]);

    if (this._providerInitQueue.length == 1) {
      this._popAndInitProvider();
    }

    return deferred.promise;
  },

  /**
   * Remove a named provider from the collector.
   *
   * It is the caller's responsibility to shut down the provider
   * instance.
   */
  unregisterProvider: function (name) {
    this._providers.delete(name);
  },

  _popAndInitProvider: function () {
    if (!this._providerInitQueue.length || this._providerInitializing) {
      return;
    }

    let [provider, deferred] = this._providerInitQueue.shift();
    this._providerInitializing = true;

    this._log.info("Initializing provider with storage: " + provider.name);
    let initPromise;
    try {
      initPromise = provider.init(this._storage);
    } catch (ex) {
      this._log.warn("Provider failed to initialize: " +
                     CommonUtils.exceptionStr(ex));
      this._providerInitializing = false;
      deferred.reject(ex);
      this._popAndInitProvider();
      return;
    }

    initPromise.then(
      function onSuccess(result) {
        this._log.info("Provider finished initialization: " + provider.name);
        this._providerInitializing = false;

        this._providers.set(provider.name, {
          provider: provider,
          constantsCollected: false,
        });

        this.providerErrors.set(provider.name, []);

        deferred.resolve(result);
        this._popAndInitProvider();
      }.bind(this),
      function onError(error) {
        this._log.warn("Provider initialization failed: " +
                       CommonUtils.exceptionStr(error));
        this._providerInitializing = false;
        deferred.reject(error);
        this._popAndInitProvider();
      }.bind(this)
    );

  },

  /**
   * Collects all constant measurements from all providers.
   *
   * Returns a Promise that will be fulfilled once all data providers have
   * provided their constant data. A side-effect of this promise fulfillment
   * is that the collector is populated with the obtained collection results.
   * The resolved value to the promise is this `Collector` instance.
   */
  collectConstantData: function () {
    let entries = [];

    for (let [name, entry] of this._providers) {
      if (entry.constantsCollected) {
        this._log.trace("Provider has already provided constant data: " +
                        name);
        continue;
      }

      entries.push(entry);
    }

    let onCollect = function (entry, result) {
      entry.constantsCollected = true;
    };

    return this._callCollectOnProviders(entries, "collectConstantData",
                                        onCollect);
  },

  /**
   * Calls collectDailyData on all providers.
   */
  collectDailyData: function () {
    return this._callCollectOnProviders(this._providers.values(),
                                        "collectDailyData");
  },

  _callCollectOnProviders: function (entries, fnProperty, onCollect=null) {
    let promises = [];

    for (let entry of entries) {
      let provider = entry.provider;
      let collectPromise;
      try {
        collectPromise = provider[fnProperty].call(provider);
      } catch (ex) {
        this._log.warn("Exception when calling " + provider.name + "." +
                       fnProperty + ": " + CommonUtils.exceptionStr(ex));
        this.providerErrors.get(provider.name).push(ex);
        continue;
      }

      if (!collectPromise) {
        this._log.warn("Provider does not return a promise from " +
                       fnProperty + "(): " + provider.name);
        continue;
      }

      let promise = collectPromise.then(function onCollected(result) {
        if (onCollect) {
          try {
            onCollect(entry, result);
          } catch (ex) {
            this._log.warn("onCollect callback threw: " +
                           CommonUtils.exceptionStr(ex));
          }
        }

        return Promise.resolve(result);
      });

      promises.push([provider.name, promise]);
    }

    return this._handleCollectionPromises(promises);
  },

  /**
   * Handles promises returned by the collect* functions.
   *
   * This consumes the data resolved by the promises and returns a new promise
   * that will be resolved once all promises have been resolved.
   *
   * The promise is resolved even if one of the underlying collection
   * promises is rejected.
   */
  _handleCollectionPromises: function (promises) {
    if (!promises.length) {
      return Promise.resolve(this);
    }

    let deferred = Promise.defer();
    let finishedCount = 0;

    let onComplete = function () {
      finishedCount++;
      if (finishedCount >= promises.length) {
        deferred.resolve(this);
      }
    }.bind(this);

    for (let [name, promise] of promises) {
      let onError = function (error) {
        this._log.warn("Collection promise was rejected: " +
                       CommonUtils.exceptionStr(error));
        this.providerErrors.get(name).push(error);
        onComplete();
      }.bind(this);
      promise.then(onComplete, onError);
    }

    return deferred.promise;
  },
});

