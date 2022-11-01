export const RegionTestUtils = Object.freeze({
  REGION_URL_PREF: "browser.region.network.url",

  setNetworkRegion(region) {
    Services.prefs.setCharPref(
      this.REGION_URL_PREF,
      `data:application/json,{"country_code": "${region}"}`
    );
  },
});
