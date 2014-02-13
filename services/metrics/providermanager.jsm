/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

#ifndef MERGED_COMPARTMENT
this.EXPORTED_SYMBOLS = ["ProviderManager"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/services/metrics/dataprovider.jsm");
#endif

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/utils.js");


/**
 * Handles and coordinates the collection of metrics data from providers.
 *
 * This provides an interface for managing `Metrics.Provider` instances. It
 * provides APIs for bulk collection of data.
 */
this.ProviderManager = function (storage) {
  this._log = Log.repository.getLogger("Services.Metrics.ProviderManager");

  this._providers = new Map();
  this._storage = storage;

  this._providerInitQueue = [];
  this._providerInitializing = false;

  this._pullOnlyProviders = {};
  this._pullOnlyProvidersRegisterCount = 0;
  this._pullOnlyProvidersState = this.PULL_ONLY_NOT_REGISTERED;
  this._pullOnlyProvidersCurrentPromise = null;

  // Callback to allow customization of providers after they are constructed
  // but before they call out into their initialization code.
  this.onProviderInit = null;
}

this.ProviderManager.prototype = Object.freeze({
  PULL_ONLY_NOT_REGISTERED: "none",
  PULL_ONLY_REGISTERING: "registering",
  PULL_ONLY_UNREGISTERING: "unregistering",
  PULL_ONLY_REGISTERED: "registered",

  get providers() {
    let providers = [];
    for (let [name, entry] of this._providers) {
      providers.push(entry.provider);
    }

    return providers;
  },

  /**
   * Obtain a provider from its name.
   */
  getProvider: function (name) {
    let provider = this._providers.get(name);

    if (!provider) {
      return null;
    }

    return provider.provider;
  },

  /**
   * Registers providers from a category manager category.
   *
   * This examines the specified category entries and registers found
   * providers.
   *
   * Category entries are essentially JS modules and the name of the symbol
   * within that module that is a `Metrics.Provider` instance.
   *
   * The category entry name is the name of the JS type for the provider. The
   * value is the resource:// URI to import which makes this type available.
   *
   * Example entry:
   *
   *   FooProvider resource://gre/modules/foo.jsm
   *
   * One can register entries in the application's .manifest file. e.g.
   *
   *   category healthreport-js-provider-default FooProvider resource://gre/modules/foo.jsm
   *   category healthreport-js-provider-nightly EyeballProvider resource://gre/modules/eyeball.jsm
   *
   * Then to load them:
   *
   *   let reporter = getHealthReporter("healthreport.");
   *   reporter.registerProvidersFromCategoryManager("healthreport-js-provider-default");
   *
   * If the category has no defined members, this call has no effect, and no error is raised.
   *
   * @param category
   *        (string) Name of category from which to query and load.
   * @return a newly spawned Task.
   */
  registerProvidersFromCategoryManager: function (category) {
    this._log.info("Registering providers from category: " + category);
    let cm = Cc["@mozilla.org/categorymanager;1"]
               .getService(Ci.nsICategoryManager);

    let promises = [];
    let enumerator = cm.enumerateCategory(category);
    while (enumerator.hasMoreElements()) {
      let entry = enumerator.getNext()
                            .QueryInterface(Ci.nsISupportsCString)
                            .toString();

      let uri = cm.getCategoryEntry(category, entry);
      this._log.info("Attempting to load provider from category manager: " +
                     entry + " from " + uri);

      try {
        let ns = {};
        Cu.import(uri, ns);

        let promise = this.registerProviderFromType(ns[entry]);
        if (promise) {
          promises.push(promise);
        }
      } catch (ex) {
        this._recordProviderError(entry,
                                  "Error registering provider from category manager",
                                  ex);
        continue;
      }
    }

    return Task.spawn(function wait() {
      for (let promise of promises) {
        yield promise;
      }
    });
  },

  /**
   * Registers a `MetricsProvider` with this manager.
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
    // We should perform an instanceof check here. However, due to merged
    // compartments, the Provider type may belong to one of two JSMs
    // isinstance gets confused depending on which module Provider comes
    // from. Some code references Provider from dataprovider.jsm; others from
    // Metrics.jsm.
    if (!provider.name) {
      throw new Error("Provider is not valid: does not have a name.");
    }
    if (this._providers.has(provider.name)) {
      return CommonUtils.laterTickResolvingPromise();
    }

    let deferred = Promise.defer();
    this._providerInitQueue.push([provider, deferred]);

    if (this._providerInitQueue.length == 1) {
      this._popAndInitProvider();
    }

    return deferred.promise;
  },

  /**
   * Registers a provider from its constructor function.
   *
   * If the provider is pull-only, it will be stashed away and
   * initialized later. Null will be returned.
   *
   * If it is not pull-only, it will be initialized immediately and a
   * promise will be returned. The promise will be resolved when the
   * provider has finished initializing.
   */
  registerProviderFromType: function (type) {
    let proto = type.prototype;
    if (proto.pullOnly) {
      this._log.info("Provider is pull-only. Deferring initialization: " +
                     proto.name);
      this._pullOnlyProviders[proto.name] = type;

      return null;
    }

    let provider = this._initProviderFromType(type);
    return this.registerProvider(provider);
  },

  /**
   * Initializes a provider from its type.
   *
   * This is how a constructor function should be turned into a provider
   * instance.
   *
   * A side-effect is the provider is registered with the manager.
   */
  _initProviderFromType: function (type) {
    let provider = new type();
    if (this.onProviderInit) {
      this.onProviderInit(provider);
    }

    return provider;
  },

  /**
   * Remove a named provider from the manager.
   *
   * It is the caller's responsibility to shut down the provider
   * instance.
   */
  unregisterProvider: function (name) {
    this._providers.delete(name);
  },

  /**
   * Ensure that pull-only providers are registered.
   */
  ensurePullOnlyProvidersRegistered: function () {
    let state = this._pullOnlyProvidersState;

    this._pullOnlyProvidersRegisterCount++;

    if (state == this.PULL_ONLY_REGISTERED) {
      this._log.debug("Requested pull-only provider registration and " +
                      "providers are already registered.");
      return CommonUtils.laterTickResolvingPromise();
    }

    // If we're in the process of registering, chain off that request.
    if (state == this.PULL_ONLY_REGISTERING) {
      this._log.debug("Requested pull-only provider registration and " +
                      "registration is already in progress.");
      return this._pullOnlyProvidersCurrentPromise;
    }

    this._log.debug("Pull-only provider registration requested.");

    // A side-effect of setting this is that an active unregistration will
    // effectively short circuit and finish as soon as the in-flight
    // unregistration (if any) finishes.
    this._pullOnlyProvidersState = this.PULL_ONLY_REGISTERING;

    let inFlightPromise = this._pullOnlyProvidersCurrentPromise;

    this._pullOnlyProvidersCurrentPromise =
      Task.spawn(function registerPullProviders() {

      if (inFlightPromise) {
        this._log.debug("Waiting for in-flight pull-only provider activity " +
                        "to finish before registering.");
        try {
          yield inFlightPromise;
        } catch (ex) {
          this._log.warn("Error when waiting for existing pull-only promise: " +
                         CommonUtils.exceptionStr(ex));
        }
      }

      for each (let providerType in this._pullOnlyProviders) {
        // Short-circuit if we're no longer registering.
        if (this._pullOnlyProvidersState != this.PULL_ONLY_REGISTERING) {
          this._log.debug("Aborting pull-only provider registration.");
          break;
        }

        try {
          let provider = this._initProviderFromType(providerType);

          // This is a no-op if the provider is already registered. So, the
          // only overhead is constructing an instance. This should be cheap
          // and isn't worth optimizing.
          yield this.registerProvider(provider);
        } catch (ex) {
          this._recordProviderError(providerType.prototype.name,
                                    "Error registering pull-only provider",
                                    ex);
        }
      }

      // It's possible we changed state while registering. Only mark as
      // registered if we didn't change state.
      if (this._pullOnlyProvidersState == this.PULL_ONLY_REGISTERING) {
        this._pullOnlyProvidersState = this.PULL_ONLY_REGISTERED;
        this._pullOnlyProvidersCurrentPromise = null;
      }
    }.bind(this));
    return this._pullOnlyProvidersCurrentPromise;
  },

  ensurePullOnlyProvidersUnregistered: function () {
    let state = this._pullOnlyProvidersState;

    // If we're not registered, this is a no-op.
    if (state == this.PULL_ONLY_NOT_REGISTERED) {
      this._log.debug("Requested pull-only provider unregistration but none " +
                      "are registered.");
      return CommonUtils.laterTickResolvingPromise();
    }

    // If we're currently unregistering, recycle the promise from last time.
    if (state == this.PULL_ONLY_UNREGISTERING) {
      this._log.debug("Requested pull-only provider unregistration and " +
                 "unregistration is in progress.");
      this._pullOnlyProvidersRegisterCount =
        Math.max(0, this._pullOnlyProvidersRegisterCount - 1);

      return this._pullOnlyProvidersCurrentPromise;
    }

    // We ignore this request while multiple entities have requested
    // registration because we don't want a request from an "inner,"
    // short-lived request to overwrite the desire of the "parent,"
    // longer-lived request.
    if (this._pullOnlyProvidersRegisterCount > 1) {
      this._log.debug("Requested pull-only provider unregistration while " +
                      "other callers still want them registered. Ignoring.");
      this._pullOnlyProvidersRegisterCount--;
      return CommonUtils.laterTickResolvingPromise();
    }

    // We are either fully registered or registering with a single consumer.
    // In both cases we are authoritative and can commence unregistration.

    this._log.debug("Pull-only providers being unregistered.");
    this._pullOnlyProvidersRegisterCount =
      Math.max(0, this._pullOnlyProvidersRegisterCount - 1);
    this._pullOnlyProvidersState = this.PULL_ONLY_UNREGISTERING;
    let inFlightPromise = this._pullOnlyProvidersCurrentPromise;

    this._pullOnlyProvidersCurrentPromise =
      Task.spawn(function unregisterPullProviders() {

      if (inFlightPromise) {
        this._log.debug("Waiting for in-flight pull-only provider activity " +
                        "to complete before unregistering.");
        try {
          yield inFlightPromise;
        } catch (ex) {
          this._log.warn("Error when waiting for existing pull-only promise: " +
                         CommonUtils.exceptionStr(ex));
        }
      }

      for (let provider of this.providers) {
        if (this._pullOnlyProvidersState != this.PULL_ONLY_UNREGISTERING) {
          return;
        }

        if (!provider.pullOnly) {
          continue;
        }

        this._log.info("Shutting down pull-only provider: " +
                       provider.name);

        try {
          yield provider.shutdown();
        } catch (ex) {
          this._recordProviderError(provider.name,
                                    "Error when shutting down provider",
                                    ex);
        } finally {
          this.unregisterProvider(provider.name);
        }
      }

      if (this._pullOnlyProvidersState == this.PULL_ONLY_UNREGISTERING) {
        this._pullOnlyProvidersState = this.PULL_ONLY_NOT_REGISTERED;
        this._pullOnlyProvidersCurrentPromise = null;
      }
    }.bind(this));
    return this._pullOnlyProvidersCurrentPromise;
  },

  _popAndInitProvider: function () {
    if (!this._providerInitQueue.length || this._providerInitializing) {
      return;
    }

    let [provider, deferred] = this._providerInitQueue.shift();
    this._providerInitializing = true;

    this._log.info("Initializing provider with storage: " + provider.name);

    Task.spawn(function initProvider() {
      try {
        let result = yield provider.init(this._storage);
        this._log.info("Provider successfully initialized: " + provider.name);

        this._providers.set(provider.name, {
          provider: provider,
          constantsCollected: false,
        });

        deferred.resolve(result);
      } catch (ex) {
        this._recordProviderError(provider.name, "Failed to initialize", ex);
        deferred.reject(ex);
      } finally {
        this._providerInitializing = false;
        this._popAndInitProvider();
      }
    }.bind(this));
  },

  /**
   * Collects all constant measurements from all providers.
   *
   * Returns a Promise that will be fulfilled once all data providers have
   * provided their constant data. A side-effect of this promise fulfillment
   * is that the manager is populated with the obtained collection results.
   * The resolved value to the promise is this `ProviderManager` instance.
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
        this._recordProviderError(provider.name, "Exception when calling " +
                                  "collect function: " + fnProperty, ex);
        continue;
      }

      if (!collectPromise) {
        this._recordProviderError(provider.name, "Does not return a promise " +
                                  "from " + fnProperty + "()");
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

        return CommonUtils.laterTickResolvingPromise(result);
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
    return Task.spawn(function waitForPromises() {
      for (let [name, promise] of promises) {
        try {
          yield promise;
          this._log.debug("Provider collected successfully: " + name);
        } catch (ex) {
          this._recordProviderError(name, "Failed to collect", ex);
        }
      }

      throw new Task.Result(this);
    }.bind(this));
  },

  /**
   * Record an error that occurred operating on a provider.
   */
  _recordProviderError: function (name, msg, ex) {
    let msg = "Provider error: " + name + ": " + msg;
    if (ex) {
      msg += ": " + CommonUtils.exceptionStr(ex);
    }
    this._log.warn(msg);

    if (this.onProviderError) {
      try {
        this.onProviderError(msg);
      } catch (callError) {
        this._log.warn("Exception when calling onProviderError callback: " +
                       CommonUtils.exceptionStr(callError));
      }
    }
  },
});

