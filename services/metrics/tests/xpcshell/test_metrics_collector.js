/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://testing-common/services/metrics/mocks.jsm");

function run_test() {
  run_next_test();
};

add_task(function test_constructor() {
  let storage = yield Metrics.Storage("constructor");
  let collector = new Metrics.Collector(storage);

  yield storage.close();
});

add_task(function test_register_provider() {
  let storage = yield Metrics.Storage("register_provider");

  let collector = new Metrics.Collector(storage);
  let dummy = new DummyProvider();

  yield collector.registerProvider(dummy);
  do_check_eq(collector._providers.size, 1);
  yield collector.registerProvider(dummy);
  do_check_eq(collector._providers.size, 1);
  do_check_eq(collector.providerErrors.size, 1);

  let failed = false;
  try {
    collector.registerProvider({});
  } catch (ex) {
    do_check_true(ex.message.startsWith("Argument must be a Provider"));
    failed = true;
  } finally {
    do_check_true(failed);
    failed = false;
  }

  collector.unregisterProvider(dummy.name);
  do_check_eq(collector._providers.size, 0);

  yield storage.close();
});

add_task(function test_collect_constant_data() {
  let storage = yield Metrics.Storage("collect_constant_data");
  let collector = new Metrics.Collector(storage);
  let provider = new DummyProvider();
  yield collector.registerProvider(provider);

  do_check_eq(provider.collectConstantCount, 0);

  yield collector.collectConstantData();
  do_check_eq(provider.collectConstantCount, 1);

  do_check_true(collector._providers.get("DummyProvider").constantsCollected);
  do_check_eq(collector.providerErrors.get("DummyProvider").length, 0);

  yield storage.close();
});

add_task(function test_collect_constant_throws() {
  let storage = yield Metrics.Storage("collect_constant_throws");
  let collector = new Metrics.Collector(storage);
  let provider = new DummyProvider();
  provider.throwDuringCollectConstantData = "Fake error during collect";
  yield collector.registerProvider(provider);

  yield collector.collectConstantData();
  do_check_true(collector.providerErrors.has(provider.name));
  let errors = collector.providerErrors.get(provider.name);
  do_check_eq(errors.length, 1);
  do_check_eq(errors[0].message, provider.throwDuringCollectConstantData);

  yield storage.close();
});

add_task(function test_collect_constant_populate_throws() {
  let storage = yield Metrics.Storage("collect_constant_populate_throws");
  let collector = new Metrics.Collector(storage);
  let provider = new DummyProvider();
  provider.throwDuringConstantPopulate = "Fake error during constant populate";
  yield collector.registerProvider(provider);

  yield collector.collectConstantData();

  let errors = collector.providerErrors.get(provider.name);
  do_check_eq(errors.length, 1);
  do_check_eq(errors[0].message, provider.throwDuringConstantPopulate);
  do_check_false(collector._providers.get(provider.name).constantsCollected);

  yield storage.close();
});

add_task(function test_collect_constant_onetime() {
  let storage = yield Metrics.Storage("collect_constant_onetime");
  let collector = new Metrics.Collector(storage);
  let provider = new DummyProvider();
  yield collector.registerProvider(provider);

  yield collector.collectConstantData();
  do_check_eq(provider.collectConstantCount, 1);

  yield collector.collectConstantData();
  do_check_eq(provider.collectConstantCount, 1);

  yield storage.close();
});

add_task(function test_collect_multiple() {
  let storage = yield Metrics.Storage("collect_multiple");
  let collector = new Metrics.Collector(storage);

  for (let i = 0; i < 10; i++) {
    yield collector.registerProvider(new DummyProvider("provider" + i));
  }

  do_check_eq(collector._providers.size, 10);

  yield collector.collectConstantData();

  yield storage.close();
});

add_task(function test_collect_daily() {
  let storage = yield Metrics.Storage("collect_daily");
  let collector = new Metrics.Collector(storage);

  let provider1 = new DummyProvider("DP1");
  let provider2 = new DummyProvider("DP2");

  yield collector.registerProvider(provider1);
  yield collector.registerProvider(provider2);

  yield collector.collectDailyData();
  do_check_eq(provider1.collectDailyCount, 1);
  do_check_eq(provider2.collectDailyCount, 1);

  yield collector.collectDailyData();
  do_check_eq(provider1.collectDailyCount, 2);
  do_check_eq(provider2.collectDailyCount, 2);

  yield storage.close();
});

