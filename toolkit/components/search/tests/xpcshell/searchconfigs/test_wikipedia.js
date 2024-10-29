/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const testConfiguration = {
  identifier: "wikipedia",
  default: {
    // Not default anywhere.
  },
  available: {
    excluded: [
      // Should be available everywhere.
    ],
  },
  details: [
    // Details generated below.
  ],
};

/**
 * Generates the expected details for the given locales and inserts
 * them into the testConfiguration.
 *
 * @param {string[]} locales
 *   The locales for this details entry - which locales this variant of
 *   Wikipedia is expected to be deployed to.
 * @param {string} [subDomainName]
 *   The expected sub domain name for this variant of Wikipedia. If not
 *   specified, defaults to the first item in the locales array.
 * @param {string} [telemetrySuffix]
 *   The expected suffix used when this variant is reported via telemetry. If
 *   not specified, defaults to the first item in the array. If this is the
 *   empty string, then it "wikipedia" (i.e. no suffix) will be the expected
 *   value.
 */
function generateExpectedDetails(locales, subDomainName, telemetrySuffix) {
  if (!subDomainName) {
    subDomainName = locales[0];
  }
  if (telemetrySuffix == undefined) {
    telemetrySuffix = locales[0];
  }
  testConfiguration.details.push({
    domain: `${subDomainName}.wikipedia.org`,
    telemetryId: telemetrySuffix ? `wikipedia-${telemetrySuffix}` : "wikipedia",
    required_aliases: ["@wikipedia"],
    included: [{ locales }],
  });
}

// This is an array of an array of arguments to be passed to generateExpectedDetails().
// These are the locale, sub domain name and telemetry id suffix expectations for
// the test to check.
// Note that the expectations for en.wikipedia.com are generated in add_setup.
const LOCALES_INFO = [
  [["af"]],
  [["an"]],
  [["ar"]],
  [["ast"]],
  [["az"]],
  [["be"]],
  [["bg"]],
  [["bn"]],
  [["br"]],
  [["bs"]],
  [["ca", "ca-valencia"], "ca", "ca"],
  [["cs"], "cs", "cz"],
  [["cy"]],
  [["da"]],
  [["de"]],
  [["dsb"]],
  [["el"]],
  [["eo"]],
  [["cak", "es-AR", "es-CL", "es-ES", "es-MX", "trs"], "es", "es"],
  [["et"]],
  [["eu"]],
  [["fa"]],
  [["fi"]],
  [["fr", "ff", "son"], "fr", "fr"],
  [["fy-NL"], "fy", "fy-NL"],
  [["ga-IE"], "ga", "ga-IE"],
  [["gd"]],
  [["gl"]],
  [["gn"]],
  [["gu-IN"], "gu", "gu"],
  [["hi-IN"], "hi", "hi"],
  [["he"]],
  [["hr"]],
  [["hsb"]],
  [["hu"]],
  [["hy-AM"], "hy", "hy"],
  [["ia"]],
  [["id"]],
  [["is"]],
  [["ja", "ja-JP-macos"], "ja", "ja"],
  [["ka"]],
  [["kab"]],
  [["kk"]],
  [["km"]],
  [["kn"]],
  [["ko"], "ko", "kr"],
  [["it", "fur", "sc"], "it", "it"],
  [["lij"]],
  [["lo"]],
  [["lt"]],
  [["ltg"]],
  [["lv"]],
  [["mk"]],
  [["mr"]],
  [["ms"]],
  [["my"]],
  [["nb-NO"], "no", "NO"],
  [["ne-NP"], "ne", "ne"],
  [["nl"]],
  [["nn-NO"], "nn", "NN"],
  [["oc"]],
  [["pa-IN"], "pa", "pa"],
  [["pl", "szl"], "pl", "pl"],
  [["pt-BR", "pt-PT"], "pt", "pt"],
  [["rm"]],
  [["ro"]],
  [["ru"]],
  [["si"]],
  [["sk"]],
  [["sl"]],
  [["sq"]],
  [["sr"]],
  [["sv-SE"], "sv", "sv-SE"],
  [["ta"]],
  [["te"]],
  [["th"]],
  [["tl"]],
  [["tr"]],
  [["uk"]],
  [["ur"]],
  [["uz"]],
  [["vi"]],
  [["wo"]],
  [["zh-CN"], "zh", "zh-CN"],
  [["zh-TW"], "zh", "zh-TW"],
];

const test = new SearchConfigTest(testConfiguration);

add_setup(async function () {
  const allLocales = await test.getLocales();

  // For the "en" version of Wikipedia, we ship it to all locales where other
  // Wikipedias are not shipped. We form the list based on all-locales to avoid
  // needing to update the test whenever all-locales is updated.
  let enLocales = [];
  for (let locale of allLocales) {
    if (!LOCALES_INFO.find(d => d[0].includes(locale))) {
      enLocales.push(locale);
    }
  }

  console.log("en.wikipedia.org expected locales are:", enLocales);
  generateExpectedDetails(enLocales, "en", "");

  for (let details of LOCALES_INFO) {
    generateExpectedDetails(...details);
  }

  await test.setup();
});

add_task(async function test_searchConfig_wikipedia() {
  await test.run();
});
