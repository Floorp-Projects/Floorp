// Appease eslint.
/* import-globals-from ../head_addons.js */
{
  let {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
  Services.prefs.setBoolPref("extensions.blocklist.useXML", true);
}
