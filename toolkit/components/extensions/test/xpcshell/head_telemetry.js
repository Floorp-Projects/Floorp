/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported IS_OOP, valueSum, clearHistograms, getSnapshots, promiseTelemetryRecorded,
            assertDNRTelemetryMetricsDefined, assertDNRTelemetryMetricsNoSamples, assertDNRTelemetryMetricsGetValueEq,
            assertDNRTelemetryMetricsSamplesCount, resetTelemetryData, setupTelemetryForTests */

ChromeUtils.defineESModuleGetters(this, {
  ContentTaskUtils: "resource://testing-common/ContentTaskUtils.sys.mjs",
});

// Allows to run xpcshell telemetry test also on products (e.g. Thunderbird) where
// that telemetry wouldn't be actually collected in practice (but to be sure
// that it will work on those products as well by just adding the product in
// the telemetry metric definitions if it turns out we want to).
Services.prefs.setBoolPref(
  "toolkit.telemetry.testing.overrideProductsCheck",
  true
);

const IS_OOP = Services.prefs.getBoolPref("extensions.webextensions.remote");

const WEBEXT_EVENTPAGE_RUNNING_TIME_MS = "WEBEXT_EVENTPAGE_RUNNING_TIME_MS";
const WEBEXT_EVENTPAGE_RUNNING_TIME_MS_BY_ADDONID =
  "WEBEXT_EVENTPAGE_RUNNING_TIME_MS_BY_ADDONID";
const WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT = "WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT";
const WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT_BY_ADDONID =
  "WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT_BY_ADDONID";

// Keep this in sync with the order in Histograms.json for "WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT":
// the position of the category string determines the index of the values collected in the categorial
// histogram and so the existing labels should be kept in the exact same order and any new category
// to be added in the future should be appended to the existing ones.
const HISTOGRAM_EVENTPAGE_IDLE_RESULT_CATEGORIES = [
  "suspend",
  "reset_other",
  "reset_event",
  "reset_listeners",
  "reset_nativeapp",
  "reset_streamfilter",
  "reset_parentapicall",
];

const GLEAN_EVENTPAGE_IDLE_RESULT_CATEGORIES = [
  ...HISTOGRAM_EVENTPAGE_IDLE_RESULT_CATEGORIES,
  "__other__",
];

function valueSum(arr) {
  return Object.values(arr).reduce((a, b) => a + b, 0);
}

function clearHistograms() {
  Services.telemetry.getSnapshotForHistograms("main", true /* clear */);
  Services.telemetry.getSnapshotForKeyedHistograms("main", true /* clear */);
}

function clearScalars() {
  Services.telemetry.getSnapshotForScalars("main", true /* clear */);
  Services.telemetry.getSnapshotForKeyedScalars("main", true /* clear */);
}

function getSnapshots(process) {
  return Services.telemetry.getSnapshotForHistograms("main", false /* clear */)[
    process
  ];
}

function getKeyedSnapshots(process) {
  return Services.telemetry.getSnapshotForKeyedHistograms(
    "main",
    false /* clear */
  )[process];
}

// TODO Bug 1357509: There is no good way to make sure that the parent received
// the histogram entries from the extension and content processes.  Let's stick
// to the ugly, spinning the event loop until we have a good approach.
function promiseTelemetryRecorded(id, process, expectedCount) {
  let condition = () => {
    let snapshot = Services.telemetry.getSnapshotForHistograms(
      "main",
      false /* clear */
    )[process][id];
    return snapshot && valueSum(snapshot.values) >= expectedCount;
  };
  return ContentTaskUtils.waitForCondition(condition);
}

function promiseKeyedTelemetryRecorded(
  id,
  process,
  expectedKey,
  expectedCount
) {
  let condition = () => {
    let snapshot = Services.telemetry.getSnapshotForKeyedHistograms(
      "main",
      false /* clear */
    )[process][id];
    return (
      snapshot &&
      snapshot[expectedKey] &&
      valueSum(snapshot[expectedKey].values) >= expectedCount
    );
  };
  return ContentTaskUtils.waitForCondition(condition);
}

function assertHistogramSnapshot(
  histogramId,
  { keyed, processSnapshot, expectedValue },
  msg
) {
  let histogram;

  if (keyed) {
    histogram = Services.telemetry.getKeyedHistogramById(histogramId);
  } else {
    histogram = Services.telemetry.getHistogramById(histogramId);
  }

  let res = processSnapshot(histogram.snapshot());
  Assert.deepEqual(res, expectedValue, msg);
  return res;
}

function assertHistogramEmpty(histogramId) {
  assertHistogramSnapshot(
    histogramId,
    {
      processSnapshot: snapshot => snapshot.sum,
      expectedValue: 0,
    },
    `No data recorded for histogram: ${histogramId}.`
  );
}

function assertKeyedHistogramEmpty(histogramId) {
  assertHistogramSnapshot(
    histogramId,
    {
      keyed: true,
      processSnapshot: snapshot => Object.keys(snapshot).length,
      expectedValue: 0,
    },
    `No data recorded for histogram: ${histogramId}.`
  );
}

function assertHistogramCategoryNotEmpty(
  histogramId,
  { category, categories, keyed, key },
  msg
) {
  let message = msg;

  if (!msg) {
    message = `Data recorded for histogram: ${histogramId}, category "${category}"`;
    if (keyed) {
      message += `, key "${key}"`;
    }
  }

  assertHistogramSnapshot(
    histogramId,
    {
      keyed,
      processSnapshot: snapshot => {
        const categoryIndex = categories.indexOf(category);
        if (keyed) {
          return {
            [key]: snapshot[key]
              ? snapshot[key].values[categoryIndex] > 0
              : null,
          };
        }
        return snapshot.values[categoryIndex] > 0;
      },
      expectedValue: keyed ? { [key]: true } : true,
    },
    message
  );
}

function setupTelemetryForTests() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();
  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
}

function resetTelemetryData() {
  Services.fog.testResetFOG();

  // Clear histograms data recorded in the unified telemetry
  // (needed to make sure we can keep asserting that the same
  // amount of samples collected by Glean should also be found
  // in the related mirrored unified telemetry probe after we
  // have reset Glean metrics data using testResetFOG).
  clearHistograms();
  clearScalars();
}

function assertValidGleanMetric({
  metricId,
  gleanMetric,
  gleanMetricConstructor,
  msg,
}) {
  const { GleanMetric } = globalThis;
  if (!(gleanMetric instanceof GleanMetric)) {
    throw new Error(
      `gleanMetric "${metricId}" ${gleanMetric} should be an instance of GleanMetric ${msg}`
    );
  }

  if (
    gleanMetricConstructor &&
    !(gleanMetric instanceof gleanMetricConstructor)
  ) {
    throw new Error(
      `gleanMetric "${metricId}" should be an instance of the given GleanMetric constructor: ${gleanMetric} not an instance of ${gleanMetricConstructor} ${msg}`
    );
  }
}

// TODO reuse this helper inside the DNR specific test helper which would be doing
// a similar assertion on DNR metrics.
function assertGleanMetricsNoSamples({
  metricId,
  gleanMetric,
  gleanMetricConstructor,
  message,
}) {
  const msg = message ? `(${message})` : "";
  assertValidGleanMetric({
    metricId,
    gleanMetric,
    gleanMetricConstructor,
    msg,
  });
  const gleanData = gleanMetric.testGetValue();
  Assert.deepEqual(
    gleanData,
    undefined,
    `Got no sample for Glean metric ${metricId} ${msg}`
  );
}

// TODO reuse this helper inside the DNR specific test helper which would be doing
// a similar assertion on DNR metrics.
function assertGleanMetricsSamplesCount({
  metricId,
  gleanMetric,
  gleanMetricConstructor,
  expectedSamplesCount,
  message,
}) {
  const msg = message ? `(${message})` : "";
  assertValidGleanMetric({
    metricId,
    gleanMetric,
    gleanMetricConstructor,
    msg,
  });
  const gleanData = gleanMetric.testGetValue();
  Assert.notEqual(
    gleanData,
    undefined,
    `Got some sample for Glean metric ${metricId} ${msg}`
  );
  Assert.equal(
    valueSum(gleanData.values),
    expectedSamplesCount,
    `Got the expected number of samples for Glean metric ${metricId} ${msg}`
  );
}

function assertGleanLabeledCounter({
  metricId,
  gleanMetric,
  gleanMetricLabels,
  expectedLabelsValue,
  ignoreNonExpectedLabels,
  ignoreUnknownLabels,
  message,
}) {
  const { GleanLabeled } = globalThis;
  const msg = message ? `(${message})` : "";
  if (!Array.isArray(gleanMetricLabels) || !gleanMetricLabels.length) {
    throw new Error(
      `Missing mandatory gleanMetricLabels property ${msg}: ${gleanMetricLabels}`
    );
  }

  if (!(gleanMetric instanceof GleanLabeled)) {
    throw new Error(
      `Glean metric "${metricId}" should be an instance of GleanLabeled: ${gleanMetric} ${msg}`
    );
  }

  for (const label of gleanMetricLabels) {
    const expectedLabelValue = expectedLabelsValue[label];
    if (ignoreNonExpectedLabels && !(label in expectedLabelsValue)) {
      continue;
    }
    Assert.deepEqual(
      gleanMetric[label].testGetValue(),
      expectedLabelValue,
      `Expect Glean "${metricId}" metric label "${label}" to be ${
        expectedLabelValue > 0 ? expectedLabelValue : "empty"
      }`
    );
  }

  if (!ignoreUnknownLabels) {
    Assert.deepEqual(
      gleanMetric["__other__"].testGetValue(), // eslint-disable-line dot-notation
      undefined,
      `Expect Glean "${metricId}" metric label "__other__" to be empty.`
    );
  }
}

function assertGleanLabeledCounterEmpty({
  metricId,
  gleanMetric,
  gleanMetricLabels,
  message,
}) {
  // All empty labels passed to the other helpers to make it
  // assert that all labels are empty.
  assertGleanLabeledCounter({
    metricId,
    gleanMetric,
    gleanMetricLabels,
    expectedLabelsValue: {},
    message,
  });
}

function assertGleanLabeledCounterNotEmpty({
  metricId,
  gleanMetric,
  expectedNotEmptyLabels,
  ignoreUnknownLabels,
  message,
}) {
  const { GleanLabeled } = globalThis;
  const msg = message ? `(${message})` : "";
  if (
    !Array.isArray(expectedNotEmptyLabels) ||
    !expectedNotEmptyLabels.length
  ) {
    throw new Error(
      `Missing mandatory expectedNotEmptyLabels property ${msg}: ${expectedNotEmptyLabels}`
    );
  }

  if (!(gleanMetric instanceof GleanLabeled)) {
    throw new Error(
      `Glean metric "${metricId}" should be an instance of GleanLabeled: ${gleanMetric} ${msg}`
    );
  }

  for (const label of expectedNotEmptyLabels) {
    Assert.notEqual(
      gleanMetric[label].testGetValue(),
      undefined,
      `Expect Glean "${metricId}" metric label "${label}" to not be empty`
    );
  }

  if (!ignoreUnknownLabels) {
    Assert.deepEqual(
      gleanMetric["__other__"].testGetValue(), // eslint-disable-line dot-notation
      undefined,
      `Expect Glean "${metricId}" metric label "__other__" to be empty.`
    );
  }
}

function assertDNRTelemetryMetricsDefined(metrics) {
  const metricsNotFound = metrics.filter(metricDetails => {
    const { metric, label } = metricDetails;
    if (!Glean.extensionsApisDnr[metric]) {
      return true;
    }
    if (label) {
      return !Glean.extensionsApisDnr[metric][label];
    }
    return false;
  });
  Assert.deepEqual(
    metricsNotFound,
    [],
    `All expected extensionsApisDnr Glean metrics should be found`
  );
}

function assertDNRTelemetryMirrored({
  gleanMetric,
  gleanLabel,
  unifiedName,
  unifiedType,
}) {
  assertDNRTelemetryMetricsDefined([
    { metric: gleanMetric, label: gleanLabel },
  ]);
  const gleanData = gleanLabel
    ? Glean.extensionsApisDnr[gleanMetric][gleanLabel].testGetValue()
    : Glean.extensionsApisDnr[gleanMetric].testGetValue();

  if (!unifiedName) {
    Assert.ok(
      false,
      `Unexpected missing unifiedName parameter on call to assertDNRTelemetryMirrored`
    );
    return;
  }

  let unifiedData;

  switch (unifiedType) {
    case "histogram": {
      let found = false;
      try {
        const histogram = Services.telemetry.getHistogramById(unifiedName);
        found = !!histogram;
      } catch (err) {
        Cu.reportError(err);
      }
      Assert.ok(found, `Expect an histogram named ${unifiedName} to be found`);
      unifiedData = Services.telemetry.getSnapshotForHistograms("main", false)
        .parent[unifiedName];
      break;
    }
    case "keyedScalar": {
      const snapshot = Services.telemetry.getSnapshotForKeyedScalars(
        "main",
        false
      );
      if (unifiedName in (snapshot?.parent || {})) {
        unifiedData = snapshot.parent[unifiedName][gleanLabel];
      }
      break;
    }
    case "scalar": {
      const snapshot = Services.telemetry.getSnapshotForScalars("main", false);
      if (unifiedName in (snapshot?.parent || {})) {
        unifiedData = snapshot.parent[unifiedName];
      }
      break;
    }
    default:
      Assert.ok(
        false,
        `Unexpected unifiedType ${unifiedType} on call to assertDNRTelemetryMirrored`
      );
      return;
  }

  if (gleanData == undefined) {
    Assert.deepEqual(
      unifiedData,
      undefined,
      `Expect mirrored unified telemetry ${unifiedType} ${unifiedName} has no samples as Glean ${gleanMetric}`
    );
  } else {
    switch (unifiedType) {
      case "histogram": {
        Assert.deepEqual(
          valueSum(unifiedData.values),
          valueSum(gleanData.values),
          `Expect mirrored unified telemetry ${unifiedType} ${unifiedName} has samples mirrored from Glean ${gleanMetric}`
        );
        break;
      }
      case "scalar":
      case "keyedScalar": {
        Assert.deepEqual(
          unifiedData,
          gleanData,
          `Expect mirrored unified telemetry ${unifiedType} ${unifiedName} has samples mirrored from Glean ${gleanMetric}`
        );
        break;
      }
    }
  }
}

function assertDNRTelemetryMetricsNoSamples(metrics, msg) {
  assertDNRTelemetryMetricsDefined(metrics);
  for (const metricDetails of metrics) {
    const { metric, label } = metricDetails;

    const gleanData = label
      ? Glean.extensionsApisDnr[metric][label].testGetValue()
      : Glean.extensionsApisDnr[metric].testGetValue();
    Assert.deepEqual(
      gleanData,
      undefined,
      `Expect no sample for Glean metric extensionApisDnr.${metric} (${msg}): ${gleanData}`
    );

    if (metricDetails.mirroredName) {
      const { mirroredName, mirroredType } = metricDetails;
      assertDNRTelemetryMirrored({
        gleanMetric: metric,
        gleanLabel: label,
        unifiedName: mirroredName,
        unifiedType: mirroredType,
      });
    }
  }
}

function assertDNRTelemetryMetricsGetValueEq(metrics, msg) {
  assertDNRTelemetryMetricsDefined(metrics);
  for (const metricDetails of metrics) {
    const { metric, label, expectedGetValue } = metricDetails;

    const gleanData = label
      ? Glean.extensionsApisDnr[metric][label].testGetValue()
      : Glean.extensionsApisDnr[metric].testGetValue();
    Assert.deepEqual(
      gleanData,
      expectedGetValue,
      `Got expected value set on Glean metric extensionApisDnr.${metric}${
        label ? `.${label}` : ""
      } (${msg})`
    );

    if (metricDetails.mirroredName) {
      const { mirroredName, mirroredType } = metricDetails;
      assertDNRTelemetryMirrored({
        gleanMetric: metric,
        gleanLabel: label,
        unifiedName: mirroredName,
        unifiedType: mirroredType,
      });
    }
  }
}

function assertDNRTelemetryMetricsSamplesCount(metrics, msg) {
  assertDNRTelemetryMetricsDefined(metrics);

  // This assertion helpers doesn't currently handle labeled metrics,
  // raise an explicit error to catch if one is included by mistake.
  const labeledMetricsFound = metrics.filter(metric => !!metric.label);
  if (labeledMetricsFound.length) {
    throw new Error(
      `Unexpected labeled metrics in call to assertDNRTelemetryMetricsSamplesCount: ${labeledMetricsFound}`
    );
  }

  for (const metricDetails of metrics) {
    const { metric, expectedSamplesCount } = metricDetails;

    const gleanData = Glean.extensionsApisDnr[metric].testGetValue();
    Assert.notEqual(
      gleanData,
      undefined,
      `Got some sample for Glean metric extensionApisDnr.${metric}: ${
        gleanData && JSON.stringify(gleanData)
      }`
    );
    Assert.equal(
      valueSum(gleanData.values),
      expectedSamplesCount,
      `Got the expected number of samples for Glean metric extensionsApisDnr.${metric} (${msg})`
    );
    // Make sure we are accumulating meaningfull values in the sample,
    // if we do have samples for the bucket "0" it likely means we have
    // not been collecting the value correctly (e.g. typo in the property
    // name being collected).
    Assert.ok(
      !gleanData.values["0"],
      `No sample for Glean metric extensionsApisDnr.${metric} should be collected for the bucket "0"`
    );

    if (metricDetails.mirroredName) {
      const { mirroredName, mirroredType } = metricDetails;
      assertDNRTelemetryMirrored({
        gleanMetric: metric,
        unifiedName: mirroredName,
        unifiedType: mirroredType,
      });
    }
  }
}
