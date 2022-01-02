/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ContentDispatchChooserTelemetry } = ChromeUtils.import(
  "resource://gre/modules/ContentDispatchChooser.jsm"
);

let telemetryLabels = Services.telemetry.getCategoricalLabels()
  .EXTERNAL_PROTOCOL_HANDLER_DIALOG_CONTEXT_SCHEME;

let schemeToLabel = ContentDispatchChooserTelemetry.SCHEME_TO_LABEL;
let schemePrefixToLabel =
  ContentDispatchChooserTelemetry.SCHEME_PREFIX_TO_LABEL;

/**
 * Test for scheme-label mappings of protocol ask dialog telemetry.
 */
add_task(async function test_telemetry_label_maps() {
  let mapValues = Object.values(schemeToLabel).concat(
    Object.values(schemePrefixToLabel)
  );

  // Scheme - label maps must have valid label values.
  mapValues.forEach(label => {
    // Mapped labels must be valid.
    Assert.ok(telemetryLabels.includes(label), `Exists label: ${label}`);
  });

  // Uppercase labels must have a mapping.
  telemetryLabels.forEach(label => {
    Assert.equal(
      label == "OTHER" || mapValues.includes(label),
      label == label.toUpperCase(),
      `Exists label: ${label}`
    );
  });

  Object.keys(schemeToLabel).forEach(key => {
    // Schemes which have a mapping must not exist as as label.
    Assert.ok(!telemetryLabels.includes(key), `Not exists label: ${key}`);

    // There must be no key duplicates across the two maps.
    Assert.ok(!schemePrefixToLabel[key], `No duplicate key: ${key}`);
  });
});

/**
 * Tests the getTelemetryLabel method.
 */
add_task(async function test_telemetry_getTelemetryLabel() {
  // Method should return the correct mapping.
  Object.keys(schemeToLabel).forEach(scheme => {
    Assert.equal(
      schemeToLabel[scheme],
      ContentDispatchChooserTelemetry._getTelemetryLabel(scheme)
    );
  });

  // Passing null to _getTelemetryLabel should throw.
  Assert.throws(() => {
    ContentDispatchChooserTelemetry._getTelemetryLabel(null);
  }, /Invalid scheme/);

  // Replace maps with test data
  ContentDispatchChooserTelemetry.SCHEME_TO_LABEL = {
    foo: "FOOLABEL",
    bar: "BARLABEL",
  };

  ContentDispatchChooserTelemetry.SCHEME_PREFIX_TO_LABEL = {
    fooPrefix: "FOOPREFIXLABEL",
    barPrefix: "BARPREFIXLABEL",
    fo: "PREFIXLABEL",
  };

  Assert.equal(
    ContentDispatchChooserTelemetry._getTelemetryLabel("foo"),
    "FOOLABEL",
    "Non prefix mapping should have priority"
  );

  Assert.equal(
    ContentDispatchChooserTelemetry._getTelemetryLabel("bar"),
    "BARLABEL",
    "Should return the correct label"
  );

  Assert.equal(
    ContentDispatchChooserTelemetry._getTelemetryLabel("fooPrefix"),
    "FOOPREFIXLABEL",
    "Should return the correct label"
  );

  Assert.equal(
    ContentDispatchChooserTelemetry._getTelemetryLabel("fooPrefix1"),
    "FOOPREFIXLABEL",
    "Should return the correct label"
  );

  Assert.equal(
    ContentDispatchChooserTelemetry._getTelemetryLabel("fooPrefix2"),
    "FOOPREFIXLABEL",
    "Should return the correct label"
  );

  Assert.equal(
    ContentDispatchChooserTelemetry._getTelemetryLabel("doesnotexist"),
    "OTHER",
    "Should return the correct label for unknown scheme"
  );

  // Restore maps
  ContentDispatchChooserTelemetry.SCHEME_TO_LABEL = schemeToLabel;
  ContentDispatchChooserTelemetry.SCHEME_PREFIX_TO_LABEL = schemePrefixToLabel;
});
