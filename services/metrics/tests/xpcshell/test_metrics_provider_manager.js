/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://testing-common/services/metrics/mocks.jsm");

const PULL_ONLY_TESTING_CATEGORY = "testing-only-pull-only-providers";

function run_test() {
  let cm = Cc["@mozilla.org/categorymanager;1"]
             .getService(Ci.nsICategoryManager);
  cm.addCategoryEntry(PULL_ONLY_TESTING_CATEGORY, "DummyProvider",
                      "resource://testing-common/services/metrics/mocks.jsm",
                      false, true);
  cm.addCategoryEntry(PULL_ONLY_TESTING_CATEGORY, "DummyConstantProvider",
                      "resource://testing-common/services/metrics/mocks.jsm",
                      false, true);

  run_next_test();
};

add_task(function test_constructor() {
  let storage = yield Metrics.Storage("constructor");
  let manager = new Metrics.ProviderManager(storage);

  yield storage.close();
});

add_task(function test_register_provider() {
  let storage = yield Metrics.Storage("register_provider");

  let manager = new Metrics.ProviderManager(storage);
  let dummy = new DummyProvider();

  yield manager.registerProvider(dummy);
  do_check_eq(manager._providers.size, 1);
  yield manager.registerProvider(dummy);
  do_check_eq(manager._providers.size, 1);
  do_check_eq(manager.getProvider(dummy.name), dummy);

  let failed = false;
  try {
    manager.registerProvider({});
  } catch (ex) {
    do_check_true(ex.message.startsWith("Provider is not valid"));
    failed = true;
  } finally {
    do_check_true(failed);
    failed = false;
  }

  manager.unregisterProvider(dummy.name);
  do_check_eq(manager._providers.size, 0);
  do_check_null(manager.getProvider(dummy.name));

  yield storage.close();
});

add_task(function test_register_providers_from_category_manager() {
  const category = "metrics-providers-js-modules";

  let cm = Cc["@mozilla.org/categorymanager;1"]
             .getService(Ci.nsICategoryManager);
  cm.addCategoryEntry(category, "DummyProvider",
                      "resource://testing-common/services/metrics/mocks.jsm",
                      false, true);

  let storage = yield Metrics.Storage("register_providers_from_category_manager");
  let manager = new Metrics.ProviderManager(storage);
  try {
    do_check_eq(manager._providers.size, 0);
    yield manager.registerProvidersFromCategoryManager(category);
    do_check_eq(manager._providers.size, 1);
  } finally {
    yield storage.close();
  }
});

add_task(function test_collect_constant_data() {
  let storage = yield Metrics.Storage("collect_constant_data");
  let errorCount = 0;
  let manager= new Metrics.ProviderManager(storage);
  manager.onProviderError = function () { errorCount++; }
  let provider = new DummyProvider();
  yield manager.registerProvider(provider);

  do_check_eq(provider.collectConstantCount, 0);

  yield manager.collectConstantData();
  do_check_eq(provider.collectConstantCount, 1);

  do_check_true(manager._providers.get("DummyProvider").constantsCollected);

  yield storage.close();
  do_check_eq(errorCount, 0);
});

add_task(function test_collect_constant_throws() {
  let storage = yield Metrics.Storage("collect_constant_throws");
  let manager = new Metrics.ProviderManager(storage);
  let errors = [];
  manager.onProviderError = function (error) { errors.push(error); };

  let provider = new DummyProvider();
  provider.throwDuringCollectConstantData = "Fake error during collect";
  yield manager.registerProvider(provider);

  yield manager.collectConstantData();
  do_check_eq(errors.length, 1);
  do_check_true(errors[0].includes(provider.throwDuringCollectConstantData));

  yield storage.close();
});

add_task(function test_collect_constant_populate_throws() {
  let storage = yield Metrics.Storage("collect_constant_populate_throws");
  let manager = new Metrics.ProviderManager(storage);
  let errors = [];
  manager.onProviderError = function (error) { errors.push(error); };

  let provider = new DummyProvider();
  provider.throwDuringConstantPopulate = "Fake error during constant populate";
  yield manager.registerProvider(provider);

  yield manager.collectConstantData();

  do_check_eq(errors.length, 1);
  do_check_true(errors[0].includes(provider.throwDuringConstantPopulate));
  do_check_false(manager._providers.get(provider.name).constantsCollected);

  yield storage.close();
});

add_task(function test_collect_constant_onetime() {
  let storage = yield Metrics.Storage("collect_constant_onetime");
  let manager = new Metrics.ProviderManager(storage);
  let provider = new DummyProvider();
  yield manager.registerProvider(provider);

  yield manager.collectConstantData();
  do_check_eq(provider.collectConstantCount, 1);

  yield manager.collectConstantData();
  do_check_eq(provider.collectConstantCount, 1);

  yield storage.close();
});

add_task(function test_collect_multiple() {
  let storage = yield Metrics.Storage("collect_multiple");
  let manager = new Metrics.ProviderManager(storage);

  for (let i = 0; i < 10; i++) {
    yield manager.registerProvider(new DummyProvider("provider" + i));
  }

  do_check_eq(manager._providers.size, 10);

  yield manager.collectConstantData();

  yield storage.close();
});

add_task(function test_collect_daily() {
  let storage = yield Metrics.Storage("collect_daily");
  let manager = new Metrics.ProviderManager(storage);

  let provider1 = new DummyProvider("DP1");
  let provider2 = new DummyProvider("DP2");

  yield manager.registerProvider(provider1);
  yield manager.registerProvider(provider2);

  yield manager.collectDailyData();
  do_check_eq(provider1.collectDailyCount, 1);
  do_check_eq(provider2.collectDailyCount, 1);

  yield manager.collectDailyData();
  do_check_eq(provider1.collectDailyCount, 2);
  do_check_eq(provider2.collectDailyCount, 2);

  yield storage.close();
});

add_task(function test_pull_only_not_initialized() {
  let storage = yield Metrics.Storage("pull_only_not_initialized");
  let manager = new Metrics.ProviderManager(storage);
  yield manager.registerProvidersFromCategoryManager(PULL_ONLY_TESTING_CATEGORY);
  do_check_eq(manager.providers.length, 1);
  do_check_eq(manager.providers[0].name, "DummyProvider");
  yield storage.close();
});

add_task(function test_pull_only_registration() {
  let storage = yield Metrics.Storage("pull_only_registration");
  let manager = new Metrics.ProviderManager(storage);
  yield manager.registerProvidersFromCategoryManager(PULL_ONLY_TESTING_CATEGORY);
  do_check_eq(manager.providers.length, 1);

  // Simple registration and unregistration.
  yield manager.ensurePullOnlyProvidersRegistered();
  do_check_eq(manager.providers.length, 2);
  do_check_neq(manager.getProvider("DummyConstantProvider"), null);
  yield manager.ensurePullOnlyProvidersUnregistered();
  do_check_eq(manager.providers.length, 1);
  do_check_null(manager.getProvider("DummyConstantProvider"));

  // Multiple calls to register work.
  yield manager.ensurePullOnlyProvidersRegistered();
  do_check_eq(manager.providers.length, 2);
  yield manager.ensurePullOnlyProvidersRegistered();
  do_check_eq(manager.providers.length, 2);

  // Unregister with 2 requests for registration should not unregister.
  yield manager.ensurePullOnlyProvidersUnregistered();
  do_check_eq(manager.providers.length, 2);

  // But the 2nd one will.
  yield manager.ensurePullOnlyProvidersUnregistered();
  do_check_eq(manager.providers.length, 1);

  yield storage.close();
});

add_task(function test_pull_only_register_while_registering() {
  let storage = yield Metrics.Storage("pull_only_register_will_registering");
  let manager = new Metrics.ProviderManager(storage);
  yield manager.registerProvidersFromCategoryManager(PULL_ONLY_TESTING_CATEGORY);

  manager.ensurePullOnlyProvidersRegistered();
  manager.ensurePullOnlyProvidersRegistered();
  yield manager.ensurePullOnlyProvidersRegistered();
  do_check_eq(manager.providers.length, 2);

  manager.ensurePullOnlyProvidersUnregistered();
  manager.ensurePullOnlyProvidersUnregistered();
  yield manager.ensurePullOnlyProvidersUnregistered();
  do_check_eq(manager.providers.length, 1);

  yield storage.close();
});

add_task(function test_pull_only_unregister_while_registering() {
  let storage = yield Metrics.Storage("pull_only_unregister_while_registering");
  let manager = new Metrics.ProviderManager(storage);
  yield manager.registerProvidersFromCategoryManager(PULL_ONLY_TESTING_CATEGORY);

  manager.ensurePullOnlyProvidersRegistered();
  yield manager.ensurePullOnlyProvidersUnregistered();
  do_check_eq(manager.providers.length, 1);

  yield storage.close();
});

add_task(function test_pull_only_register_while_unregistering() {
  let storage = yield Metrics.Storage("pull_only_register_while_unregistering");
  let manager = new Metrics.ProviderManager(storage);
  yield manager.registerProvidersFromCategoryManager(PULL_ONLY_TESTING_CATEGORY);

  yield manager.ensurePullOnlyProvidersRegistered();
  manager.ensurePullOnlyProvidersUnregistered();
  yield manager.ensurePullOnlyProvidersRegistered();
  do_check_eq(manager.providers.length, 2);

  yield storage.close();
});

// Re-use database for perf reasons.
const REGISTRATION_ERRORS_DB = "registration_errors";

add_task(function test_category_manager_registration_error() {
  let storage = yield Metrics.Storage(REGISTRATION_ERRORS_DB);
  let manager = new Metrics.ProviderManager(storage);

  let cm = Cc["@mozilla.org/categorymanager;1"]
             .getService(Ci.nsICategoryManager);
  cm.addCategoryEntry("registration-errors", "DummyThrowOnInitProvider",
                      "resource://testing-common/services/metrics/mocks.jsm",
                      false, true);

  let deferred = Promise.defer();
  let errorCount = 0;

  manager.onProviderError = function (msg) {
    errorCount++;
    deferred.resolve(msg);
  };

  yield manager.registerProvidersFromCategoryManager("registration-errors");
  do_check_eq(manager.providers.length, 0);
  do_check_eq(errorCount, 1);

  let msg = yield deferred.promise;
  do_check_true(msg.includes("Provider error: DummyThrowOnInitProvider: "
                             + "Error registering provider from category manager: "
                             + "Error: Dummy Error"));

  yield storage.close();
});

add_task(function test_pull_only_registration_error() {
  let storage = yield Metrics.Storage(REGISTRATION_ERRORS_DB);
  let manager = new Metrics.ProviderManager(storage);

  let deferred = Promise.defer();
  let errorCount = 0;

  manager.onProviderError = function (msg) {
    errorCount++;
    deferred.resolve(msg);
  };

  yield manager.registerProviderFromType(DummyPullOnlyThrowsOnInitProvider);
  do_check_eq(errorCount, 0);

  yield manager.ensurePullOnlyProvidersRegistered();
  do_check_eq(errorCount, 1);

  let msg = yield deferred.promise;
  do_check_true(msg.includes("Provider error: DummyPullOnlyThrowsOnInitProvider: " +
                             "Error registering pull-only provider: Error: Dummy Error"));

  yield storage.close();
});

add_task(function test_error_during_shutdown() {
  let storage = yield Metrics.Storage(REGISTRATION_ERRORS_DB);
  let manager = new Metrics.ProviderManager(storage);

  let deferred = Promise.defer();
  let errorCount = 0;

  manager.onProviderError = function (msg) {
    errorCount++;
    deferred.resolve(msg);
  };

  yield manager.registerProviderFromType(DummyThrowOnShutdownProvider);
  yield manager.registerProviderFromType(DummyProvider);
  do_check_eq(errorCount, 0);
  do_check_eq(manager.providers.length, 1);

  yield manager.ensurePullOnlyProvidersRegistered();
  do_check_eq(errorCount, 0);
  yield manager.ensurePullOnlyProvidersUnregistered();
  do_check_eq(errorCount, 1);
  let msg = yield deferred.promise;
  do_check_true(msg.includes("Provider error: DummyThrowOnShutdownProvider: " +
                             "Error when shutting down provider: Error: Dummy shutdown error"));

  yield storage.close();
});
