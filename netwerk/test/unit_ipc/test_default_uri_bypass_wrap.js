function run_test() {
  Services.prefs.setBoolPref("network.url.useDefaultURI", true);
  Services.prefs.setBoolPref(
    "network.url.some_schemes_bypass_defaultURI_fallback",
    true
  );
  Services.prefs.setCharPref(
    "network.url.simple_uri_schemes",
    "simpleprotocol,otherproto"
  );

  run_test_in_child("../unit/test_default_uri_bypass.js");
}
