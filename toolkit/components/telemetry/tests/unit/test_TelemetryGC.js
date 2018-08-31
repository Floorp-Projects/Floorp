/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

"use strict";

ChromeUtils.import("resource://gre/modules/GCTelemetry.jsm", this);

function do_register_cleanup() {
    GCTelemetry.shutdown();
}

/*
 * These tests are very basic, my goal was to add enough testing to support
 * a change made in Bug 1424760.  TODO Bug 1429635 for adding more extensive
 * tests and deleting this comment.
 */

function run_test() {
  // Test initialisation
  Assert.ok(GCTelemetry.init(), "Initialize success");
  Assert.ok(!GCTelemetry.init(), "Wont initialize twice");

  // Test the basic success path.

  // There are currently no entries
  assert_num_entries(0, false);

  // Add an entry
  GCTelemetry.observeRaw(make_gc());
  // Get it back.
  assert_num_entries(1, false);
  Assert.equal(19, Object.keys(get_entry()).length);
  // "true" will cause the entry to be clared.
  assert_num_entries(1, true);
  // There are currently no entries.
  assert_num_entries(0, false);

  // Test too many fields
  let my_big_gc = make_gc();
  for (let i = 0; i < 100; i++) {
      my_big_gc["new_property_" + i] = "Data";
  }
  GCTelemetry.observeRaw(my_big_gc);
  // Assert that it was recorded but has only 7 fields.
  Assert.equal(7, Object.keys(get_entry()).length);
  assert_num_entries(1, true);
  assert_num_entries(0, false);

  // Exactly the limit of fields.
  let my_gc_24 = make_gc();
  for (let i = 0; i < 5; i++) {
      my_gc_24["new_property_" + i] = "Data";
  }
  GCTelemetry.observeRaw(my_gc_24);
  // Assert that it was recorded has all 24 fields.
  Assert.equal(24, Object.keys(get_entry()).length);
  assert_num_entries(1, true);
  assert_num_entries(0, false);

  // Exactly too many fields.
  let my_gc_25 = make_gc();
  for (let i = 0; i < 6; i++) {
      my_gc_25["new_property_" + i] = "Data";
  }
  GCTelemetry.observeRaw(my_gc_25);
  // Assert that it was recorded but has only 7 fields.
  Assert.equal(7, Object.keys(get_entry()).length);
  assert_num_entries(1, true);
  assert_num_entries(0, false);
}

function assert_num_entries(expect, clear) {
  let entries = GCTelemetry.entries("main", clear);
  Assert.equal(expect, entries.worst.length, expect + " worst entries");
  // Randomly sampled GCs are only recorded for content processes
  Assert.equal(0, entries.random.length, expect + " random entries");
}

function get_entry() {
  let entries = GCTelemetry.entries("main", false);
  Assert.ok(entries, "Got entries object");
  Assert.ok(entries.random, "Has random property");
  Assert.ok(entries.worst, "Has worst property");
  let entry = entries.worst[0];
  Assert.ok(entry, "Got worst entry");
  return entry;
}

/*
 * These events are not exactly well-formed, but good enough.  For example
 * there's no guantee that all the pause times add up to total time, or
 * that max_pause is correct.
 */
function make_gc() {
  // Timestamps are in milliseconds since startup. All the times here
  // are wall-clock times, which may not be monotonically increasing.
  let timestamp = Math.random() * 1000000;

  let gc = {
    "status": "completed",
    "timestamp": timestamp,
    // All durations are in milliseconds.
    "max_pause": Math.random() * 95 + 5,
    "total_time": Math.random() * 500 + 500, // Sum of all slice times.
    "zones_collected": 9,
    "total_zones": 9,
    "total_compartments": 309,
    "minor_gcs": 44,
    "store_buffer_overflows": 19,
    "mmu_20ms": 0,
    "mmu_50ms": 0,
    "nonincremental_reason": "GCBytesTrigger",
    "allocated_bytes": 38853696, // in bytes
    "added_chunks": 54,
    "removed_chunks": 12,
    "slices": 15,
    "slice_number": 218, // The first slice number for this GC event.
    "slices_list": [
      {
        "slice": 218,  // The global index of this slice.
        "pause": Math.random() * 2 + 28,
        "reason": "SET_NEW_DOCUMENT",
        "initial_state": "NotActive",
        "final_state": "Mark",
        "budget": "10ms",
        "page_faults": 1,
        "start_timestamp": timestamp + Math.random() * 50000,
        "times": {
          "wait_background_thread": 0.012,
          "mark_discard_code": 2.845,
          "purge": 0.723,
          "mark": 9.831,
          "mark_roots": 0.102,
          "buffer_gray_roots": 3.095,
          "mark_cross_compartment_wrappers": 0.039,
          "mark_c_and_js_stacks": 0.005,
          "mark_runtime_wide_data": 2.313,
          "mark_embedding": 0.117,
          "mark_compartments": 2.27,
          "unmark": 1.063,
          "minor_gcs_to_evict_nursery": 8.701,
        },
      },
    ],
    "totals": {
      "wait_background_thread": 0.012,
      "mark_discard_code": 2.845,
      "purge": 0.723,
      "mark": 9.831,
      "mark_roots": 0.102,
      "buffer_gray_roots": 3.095,
      "mark_cross_compartment_wrappers": 0.039,
      "mark_c_and_js_stacks": 0.005,
      "mark_runtime_wide_data": 2.313,
      "mark_embedding": 0.117,
      "mark_compartments": 2.27,
      "unmark": 1.063,
      "minor_gcs_to_evict_nursery": 8.701,
    },
  };
  return gc;
}

