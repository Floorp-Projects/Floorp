/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This is intended for development-only. Setting it to true restricts the
// set of locales and regions that are covered, to provide tests that are
// quicker to run.
// Turning it on will generate one error at the end of the test, as a reminder
// that it needs to be changed back before shipping.
const TEST_DEBUG = false;

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AddonTestUtils: "resource://testing-common/AddonTestUtils.jsm",
  ObjectUtils: "resource://gre/modules/ObjectUtils.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  SearchEngine: "resource://gre/modules/SearchEngine.jsm",
  SearchEngineSelector: "resource://testing-common/SearchEngineSelector.jsm",
  SearchTestUtils: "resource://testing-common/SearchTestUtils.jsm",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

const GLOBAL_SCOPE = this;

const URLTYPE_SUGGEST_JSON = "application/x-suggestions+json";
const URLTYPE_SEARCH_HTML = "text/html";
const SUBMISSION_PURPOSES = [
  "searchbar",
  "keyword",
  "contextmenu",
  "homepage",
  "newtab",
];

const engineSelector = new SearchEngineSelector();

/**
 * This class implements the test harness for search configuration tests.
 * These tests are designed to ensure that the correct search engines are
 * loaded for the various region/locale configurations.
 *
 * The configuration for each test is represented by an object having the
 * following properties:
 *
 * - identifier (string)
 *   The identifier for the search engine under test.
 * - default (object)
 *   An inclusion/exclusion configuration (see below) to detail when this engine
 *   should be listed as default.
 *
 * The inclusion/exclusion configuration is represented as an object having the
 * following properties:
 *
 * - included (array)
 *   An optional array of region/locale pairs.
 * - excluded (array)
 *   An optional array of region/locale pairs.
 *
 * If the object is empty, the engine is assumed not to be part of any locale/region
 * pair.
 * If the object has `excluded` but not `included`, then the engine is assumed to
 * be part of every locale/region pair except for where it matches the exclusions.
 *
 * The region/locale pairs are represented as an object having the following
 * properties:
 *
 * - region (array)
 *   An array of two-letter region codes.
 * - locale (object)
 *   A locale object which may consist of:
 *   - matches (array)
 *     An array of locale strings which should exactly match the locale.
 *   - startsWith (array)
 *     An array of locale strings which the locale should start with.
 */
class SearchConfigTest {
  /**
   * @param {object} config
   *   The initial configuration for this test, see above.
   */
  constructor(config = {}) {
    this._config = config;
  }

  /**
   * Sets up the test.
   */
  async setup() {
    AddonTestUtils.init(GLOBAL_SCOPE);
    AddonTestUtils.createAppInfo(
      "xpcshell@tests.mozilla.org",
      "XPCShell",
      "42",
      "42"
    );

    await engineSelector.init();

    // Disable region checks.
    Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", false);
    Services.prefs.setCharPref("browser.search.geoip.url", "");

    // Enable separatePrivateDefault testing. We test with this on, as we have
    // separate tests for ensuring the normal = private when this is off.
    Services.prefs.setBoolPref(
      SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
      true
    );

    await AddonTestUtils.promiseStartupManager();
    await Services.search.init();

    // Note: we don't use the helper function here, so that we have at least
    // one message output per process.
    Assert.ok(
      Services.search.isInitialized,
      "Should have correctly initialized the search service"
    );
  }

  /**
   * Runs the test.
   * @param {boolean} useEngineSelector
   *   Flag to run the tests against the SearchEngineSelector or use
   *   existing list.json.
   */
  async run(useEngineSelector = false) {
    const locales = await this._getLocales();
    const regions = this._regions;

    // We loop on region and then locale, so that we always cause a re-init
    // when updating the requested/available locales.
    for (let region of regions) {
      for (let locale of locales) {
        if (!useEngineSelector) {
          await this._reinit(region, locale);
        }
        const engines = await this._getEngines(
          useEngineSelector,
          region,
          locale
        );
        this._assertEngineRules([engines[0]], region, locale, "default");
        const isPresent = this._assertAvailableEngines(region, locale, engines);
        if (isPresent) {
          this._assertEngineDetails(region, locale, engines);
        }
      }
    }

    this.assertOk(
      !TEST_DEBUG,
      "Should not have test debug turned on in production"
    );
  }

  async _getEngines(useEngineSelector, region, locale) {
    if (useEngineSelector) {
      let engines = [];
      let configs = await engineSelector.fetchEngineConfiguration(
        region,
        locale
      );
      for (let config of configs.engines) {
        let engine = await this._getExtensionEngine(config);
        engines.push(engine);
      }
      return engines;
    }
    return Services.search.getVisibleEngines();
  }

  async _getExtensionEngine(config) {
    let id = config.webExtensionId;
    let policy = WebExtensionPolicy.getByID(id);
    if (!policy) {
      let idPrefix = id.split("@")[0];
      let path = `resource://search-extensions/${idPrefix}/`;
      await AddonManager.installBuiltinAddon(path);
      policy = WebExtensionPolicy.getByID(id);
    }
    let params = {
      code: config.searchUrlGetExtraCodes,
    };

    let locale = config.webExtensionLocale || "default";
    // On startup the extension may have not finished parsing the
    // manifest, wait for that here.
    await policy.readyPromise;

    let manifest = policy.extension.manifest;
    if (locale != "default") {
      manifest = await policy.extension.getLocalizedManifest(locale);
    }

    let engineParams = await Services.search.getEngineParams(
      policy.extension,
      manifest,
      locale,
      params
    );

    let engine = new SearchEngine({
      name: engineParams.name,
      readOnly: engineParams.isBuiltin,
      sanitizeName: true,
    });
    engine._initFromMetadata(engineParams.name, engineParams);
    engine._loadPath = "[other]addEngineWithDetails";
    if (engineParams.extensionID) {
      engine._loadPath += ":" + engineParams.extensionID;
    }
    return engine;
  }

  /**
   * Causes re-initialization of the SearchService with the new region and locale.
   *
   * @param {string} region
   *   The two-letter region code.
   * @param {string} locale
   *   The two-letter locale code.
   */
  async _reinit(region, locale) {
    Services.prefs.setStringPref("browser.search.region", region.toUpperCase());
    const reinitCompletePromise = SearchTestUtils.promiseSearchNotification(
      "reinit-complete"
    );
    Services.locale.availableLocales = [locale];
    Services.locale.requestedLocales = [locale];
    Services.search.reInit();
    await reinitCompletePromise;

    this.assertOk(
      Services.search.isInitialized,
      "Should have completely re-initialization, if it fails check logs for if reinit was successful"
    );
  }

  /**
   * @returns {Set} the list of regions for the tests to run with.
   */
  get _regions() {
    if (TEST_DEBUG) {
      return new Set(["by", "cn", "kz", "us", "ru", "tr"]);
    }
    const chunk =
      Services.prefs.getIntPref("browser.search.config.test.section", -1) - 1;
    const regions = Services.intl.getAvailableLocaleDisplayNames("region");
    const chunkSize = Math.ceil(regions.length / 4);
    const startPoint = chunk * chunkSize;
    return regions.slice(startPoint, startPoint + chunkSize);
  }

  /**
   * @returns {array} the list of locales for the tests to run with.
   */
  async _getLocales() {
    if (TEST_DEBUG) {
      return ["be", "en-US", "kk", "tr", "ru", "zh-CN", "ach"];
    }
    const data = await OS.File.read(do_get_file("all-locales").path, {
      encoding: "utf-8",
    });
    // "en-US" is not in all-locales as it is the default locale
    // add it manually to ensure it is tested.
    return [...data.split("\n").filter(e => e != ""), "en-US"];
  }

  /**
   * Determines if a locale matches with a locales section in the configuration.
   *
   * @param {object} locales
   * @param {array} [locales.matches]
   *   Array of locale names to match exactly.
   * @param {array} [locales.startsWith]
   *   Array of locale names to match the start.
   * @param {string} locale
   *   The two-letter locale code.
   * @returns {boolean}
   *   True if the locale matches.
   */
  _localeIncludes(locales, locale) {
    if ("matches" in locales && locales.matches.includes(locale)) {
      return true;
    }
    if ("startsWith" in locales) {
      return !!locales.startsWith.find(element => locale.startsWith(element));
    }

    return false;
  }

  /**
   * Determines if a locale/region pair match a section of the configuration.
   *
   * @param {object} section
   *   The configuration section to match against.
   * @param {string} region
   *   The two-letter region code.
   * @param {string} locale
   *   The two-letter locale code.
   * @returns {boolean}
   *   True if the locale/region pair matches the section.
   */
  _localeRegionInSection(section, region, locale) {
    for (const { regions, locales } of section) {
      // If we only specify a regions or locales section then
      // it is always considered included in the other section.
      const inRegions = !regions || regions.includes(region);
      const inLocales = !locales || this._localeIncludes(locales, locale);
      if (inRegions && inLocales) {
        return true;
      }
    }
    return false;
  }

  /**
   * Helper function to find an engine from within a list.
   *
   * @param {Array} engines
   *   The list of engines to check.
   * @param {string} identifier
   *   The identifier to look for in the list.
   * @returns {Engine}
   *   Returns the engine if found, null otherwise.
   */
  _findEngine(engines, identifier) {
    return engines.find(engine => engine.identifier.startsWith(identifier));
  }

  /**
   * Asserts whether the engines rules defined in the configuration are met.
   *
   * @param {Array} engines
   *   The list of engines to check.
   * @param {string} region
   *   The two-letter region code.
   * @param {string} locale
   *   The two-letter locale code.
   * @param {string} section
   *   The section of the configuration to check.
   * @returns {boolean}
   *   Returns true if the engine is expected to be present, false otherwise.
   */
  _assertEngineRules(engines, region, locale, section) {
    const infoString = `region: "${region}" locale: "${locale}"`;
    const config = this._config[section];
    const hasIncluded = "included" in config;
    const hasExcluded = "excluded" in config;
    const identifierIncluded = !!this._findEngine(
      engines,
      this._config.identifier
    );

    // If there's not included/excluded, then this shouldn't be the default anywhere.
    if (section == "default" && !hasIncluded && !hasExcluded) {
      this.assertOk(
        !identifierIncluded,
        `Should not be ${section} for any locale/region,
         currently set for ${infoString}`
      );
      return false;
    }

    // If there's no included section, we assume the engine is default everywhere
    // and we should apply the exclusions instead.
    let included =
      hasIncluded &&
      this._localeRegionInSection(config.included, region, locale);

    let notExcluded =
      hasExcluded &&
      !this._localeRegionInSection(config.excluded, region, locale);

    if (included || notExcluded) {
      this.assertOk(
        identifierIncluded,
        `Should be ${section} for ${infoString}`
      );
      return true;
    }
    this.assertOk(
      !identifierIncluded,
      `Should not be ${section} for ${infoString}`
    );
    return false;
  }

  /**
   * Asserts whether the engine is correctly set as default or not.
   *
   * @param {string} region
   *   The two-letter region code.
   * @param {string} locale
   *   The two-letter locale code.
   */
  _assertDefaultEngines(region, locale) {
    this._assertEngineRules(
      [Services.search.originalDefaultEngine],
      region,
      locale,
      "default"
    );
    // At the moment, this uses the same section as the normal default, as
    // we don't set this differently for any region/locale.
    this._assertEngineRules(
      [Services.search.originalPrivateDefaultEngine],
      region,
      locale,
      "default"
    );
  }

  /**
   * Asserts whether the engine is correctly available or not.
   *
   * @param {string} region
   *   The two-letter region code.
   * @param {string} locale
   *   The two-letter locale code.
   * @param {array} engines
   *   The current visible engines.
   * @returns {boolean}
   *   Returns true if the engine is expected to be present, false otherwise.
   */
  _assertAvailableEngines(region, locale, engines) {
    return this._assertEngineRules(engines, region, locale, "available");
  }

  /**
   * Asserts the engine follows various rules.
   *
   * @param {string} region
   *   The two-letter region code.
   * @param {string} locale
   *   The two-letter locale code.
   * @param {array} engines
   *   The current visible engines.
   */
  _assertEngineDetails(region, locale, engines) {
    const details = this._config.details.filter(value => {
      const included = this._localeRegionInSection(
        value.included,
        region,
        locale
      );
      const excluded =
        value.excluded &&
        this._localeRegionInSection(value.excluded, region, locale);
      return included && !excluded;
    });

    const engine = this._findEngine(engines, this._config.identifier);
    this.assertOk(engine, "Should have an engine present");

    if (this._config.aliases) {
      this.assertDeepEqual(
        engine._internalAliases,
        this._config.aliases,
        "Should have the correct aliases for the engine"
      );
    }

    const location = `in region:${region}, locale:${locale}`;

    for (const rule of details) {
      this._assertCorrectDomains(location, engine, rule);
      if (rule.codes) {
        this._assertCorrectCodes(location, engine, rule);
      }
      if (rule.searchUrlCode || rule.searchFormUrlCode || rule.suggestUrlCode) {
        this._assertCorrectUrlCode(location, engine, rule);
      }
      if (rule.aliases) {
        this.assertDeepEqual(
          engine._internalAliases,
          rule.aliases,
          "Should have the correct aliases for the engine"
        );
      }
    }
  }

  /**
   * Asserts whether the engine is using the correct domains or not.
   *
   * @param {string} location
   *   Debug string with locale + region information.
   * @param {object} engine
   *   The engine being tested.
   * @param {object} rules
   *   Rules to test.
   */
  _assertCorrectDomains(location, engine, rules) {
    this.assertOk(
      rules.domain,
      `Should have an expectedDomain for the engine ${location}`
    );

    const searchForm = new URL(engine.searchForm);
    this.assertOk(
      searchForm.host.endsWith(rules.domain),
      `Should have the correct search form domain ${location}.
       Got "${searchForm.host}", expected to end with "${rules.domain}".`
    );

    for (const urlType of [URLTYPE_SUGGEST_JSON, URLTYPE_SEARCH_HTML]) {
      const submission = engine.getSubmission("test", urlType);
      if (
        urlType == URLTYPE_SUGGEST_JSON &&
        (this._config.noSuggestionsURL || rules.noSuggestionsURL)
      ) {
        this.assertOk(!submission, "Should not have a submission url");
      } else if (this._config.searchUrlBase) {
        this.assertEqual(
          submission.uri.prePath + submission.uri.filePath,
          this._config.searchUrlBase + rules.searchUrlEnd,
          `Should have the correct domain for type: ${urlType} ${location}.`
        );
      } else {
        this.assertOk(
          submission.uri.host.endsWith(rules.domain),
          `Should have the correct domain for type: ${urlType} ${location}.
           Got "${submission.uri.host}", expected to end with "${
            rules.domain
          }".`
        );
      }
    }
  }

  /**
   * Asserts whether the engine is using the correct codes or not.
   *
   * @param {string} location
   *   Debug string with locale + region information.
   * @param {object} engine
   *   The engine being tested.
   * @param {object} rules
   *   Rules to test.
   */
  _assertCorrectCodes(location, engine, rules) {
    for (const purpose of SUBMISSION_PURPOSES) {
      // Don't need to repeat the code if we use it for all purposes.
      const code =
        typeof rules.codes === "string" ? rules.codes : rules.codes[purpose];
      const submission = engine.getSubmission("test", "text/html", purpose);
      const submissionQueryParams = submission.uri.query.split("&");
      this.assertOk(
        submissionQueryParams.includes(code),
        `Expected "${code}" in url "${
          submission.uri.spec
        }" from purpose "${purpose}" ${location}`
      );

      const paramName = code.split("=")[0];
      this.assertOk(
        submissionQueryParams.filter(param => param.startsWith(paramName))
          .length == 1,
        `Expected only one "${paramName}" parameter in "${
          submission.uri.spec
        }" from purpose "${purpose}" ${location}`
      );
    }
  }

  /**
   * Asserts whether the engine is using the correct URL codes or not.
   *
   * @param {string} location
   *   Debug string with locale + region information.
   * @param {object} engine
   *   The engine being tested.
   * @param {object} rule
   *   Rules to test.
   */
  _assertCorrectUrlCode(location, engine, rule) {
    if (rule.searchUrlCode) {
      const submission = engine.getSubmission("test", URLTYPE_SEARCH_HTML);
      this.assertOk(
        submission.uri.query.split("&").includes(rule.searchUrlCode),
        `Expected "${rule.searchUrlCode}" in search url "${
          submission.uri.spec
        }"`
      );
    }
    if (rule.searchFormUrlCode) {
      const uri = engine.searchForm;
      this.assertOk(
        uri.includes(rule.searchFormUrlCode),
        `Expected "${rule.searchFormUrlCode}" in "${uri}"`
      );
    }
    if (rule.suggestUrlCode) {
      const submission = engine.getSubmission("test", URLTYPE_SUGGEST_JSON);
      this.assertOk(
        submission.uri.query.split("&").includes(rule.suggestUrlCode),
        `Expected "${rule.suggestUrlCode}" in suggestion url "${
          submission.uri.spec
        }"`
      );
    }
  }

  /**
   * Helper functions which avoid outputting test results when there are no
   * failures. These help the tests to run faster, and avoid clogging up the
   * python test runner process.
   */

  assertOk(value, message) {
    if (!value || TEST_DEBUG) {
      Assert.ok(value, message);
    }
  }

  assertEqual(actual, expected, message) {
    if (actual != expected || TEST_DEBUG) {
      Assert.equal(actual, expected, message);
    }
  }

  assertDeepEqual(actual, expected, message) {
    if (!ObjectUtils.deepEqual(actual, expected)) {
      Assert.deepEqual(actual, expected, message);
    }
  }
}
