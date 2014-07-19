// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/SimpleServiceDiscovery.jsm");

function discovery_observer(subject, topic, data) {
  do_print("Observer: " + data);

  let service = SimpleServiceDiscovery.findServiceForID(data);
  if (!service)
    return;

  do_check_eq(service.friendlyName, "Pretend Device");
  do_check_eq(service.uuid, "uuid:5ec9ff92-e8b2-4a94-a72c-76b34e6dabb1");
  do_check_eq(service.manufacturer, "Copy Cat Inc.");
  do_check_eq(service.modelName, "Eureka Dongle");

  run_next_test();
};

var testTarget = {
  target: "test:service",
  factory: function(service) { /* dummy */  },
  types: ["video/mp4"],
  extensions: ["mp4"]
};

add_test(function test_default() {
  do_register_cleanup(function cleanup() {
    SimpleServiceDiscovery.unregisterTarget(testTarget);
    Services.obs.removeObserver(discovery_observer, "ssdp-service-found");
  });

  Services.obs.addObserver(discovery_observer, "ssdp-service-found", false);

  // We need to register a target or processService will ignore us
  SimpleServiceDiscovery.registerTarget(testTarget);

  // Create a pretend service
  let service = {
    location: "http://mochi.test:8888/tests/robocop/simpleservice.xml",
    target: "test:service"
  };

  do_print("Force a detailed ping from a pretend service");

  // Poke the service directly to get the discovery to happen
  SimpleServiceDiscovery._processService(service);
});

run_next_test();
