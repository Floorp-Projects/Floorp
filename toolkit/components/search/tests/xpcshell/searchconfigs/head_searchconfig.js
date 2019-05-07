/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  AddonTestUtils: "resource://testing-common/AddonTestUtils.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  SearchTestUtils: "resource://testing-common/SearchTestUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

const GLOBAL_SCOPE = this;

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

    // This is intended for development-only. Setting it to true restricts the
    // set of locales and regions that are covered, to provide tests that are
    // quicker to run.
    // Turning it on will generate one error at the end of the test, as a reminder
    // that it needs to be changed back before shipping.
    this._testDebug = false;
  }

  /**
   * Sets up the test.
   */
  async setup() {
    AddonTestUtils.init(GLOBAL_SCOPE);
    AddonTestUtils.createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42", "42");

    // Disable region checks.
    Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", false);
    Services.prefs.setCharPref("browser.search.geoip.url", "");

    await AddonTestUtils.promiseStartupManager();
    await Services.search.init();

    Assert.ok(Services.search.isInitialized,
      "Should have correctly initialized the search service");
  }

  /**
   * Runs the test.
   */
  async run() {
    const locales = await this._getLocales();
    const regions = this._regions;

    // We loop on region and then locale, so that we always cause a re-init
    // when updating the requested/available locales.
    for (let region of regions) {
      for (let locale of locales) {
        const infoString = `region: "${region}" locale: "${locale}"`;
        info(`Checking ${infoString}`);
        await this._reinit(region, locale);

        this._assertDefaultEngines(region, locale, infoString);
      }
    }

    Assert.ok(!this._testDebug, "Should not have test debug turned on in production");
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
    const reinitCompletePromise =
      SearchTestUtils.promiseSearchNotification("reinit-complete");
    Services.locale.availableLocales = [locale];
    Services.locale.requestedLocales = [locale];
    Services.search.reInit();
    await reinitCompletePromise;

    Assert.ok(Services.search.isInitialized,
      "Should have completely re-initialization, if it fails check logs for if reinit was successful");
  }

  /**
   * @returns {Set} the list of regions for the tests to run with.
   */
  get _regions() {
    if (this._testDebug) {
      return new Set(["by", "cn", "kz", "us", "ru", "tr"]);
    }
    return Services.intl.getAvailableLocaleDisplayNames("region");
  }

  /**
   * @returns {array} the list of locales for the tests to run with.
   */
  async _getLocales() {
    if (this._testDebug) {
      return ["be", "en-US", "kk", "tr", "ru", "zh-CN"];
    }
    const data = await OS.File.read(do_get_file("all-locales").path, {encoding: "utf-8"});
    return data.split("\n").filter(e => e != "");
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
    if ("matches" in locales &&
        locales.matches.includes(locale)) {
      return true;
    }
    if ("startsWith" in locales) {
      return locales.startsWith.find(element => element.startsWith(locale));
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
    for (const {regions, locales} of section) {
      if (regions.includes(region) &&
          this._localeIncludes(locales, locale)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Asserts whether the engine is correctly set as default or not.
   *
   * @param {string} region
   *   The two-letter region code.
   * @param {string} locale
   *   The two-letter locale code.
   * @param {string} infoString
   *   An additional message to output with assertions.
   */
  _assertDefaultEngines(region, locale, infoString) {
    // We use the originalDefaultEngine here as due to the way we run the tests
    // the default engine may not have changed.
    const identifier = Services.search.originalDefaultEngine.identifier;

    // If there's not included/excluded, then this shouldn't be the default anywhere.
    if (!("included" in this._config.default) &&
        !("excluded" in this._config.default)) {
      Assert.notEqual(identifier,
        this._config.identifier,
        `Should not be set as the default engine for any locale/region,
         currently set for ${infoString}`);
      return;
    }

    // If there's no included section, we assume the engine is default everywhere
    // and we should apply the exclusions instead.
    if (("included" in this._config.default &&
        this._localeRegionInSection(this._config.default.included, region, locale)) ||
        ("excluded" in this._config.default &&
         !this._localeRegionInSection(this._config.default.excluded, region, locale))) {
      Assert.equal(identifier,
        this._config.identifier,
        `Should be set as the default engine for ${infoString}`);
      return;
    }
    Assert.notEqual(identifier,
      this._config.identifier,
      `Should not be set as the default engine for ${infoString}`);
  }
}
