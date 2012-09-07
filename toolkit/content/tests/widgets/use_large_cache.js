(function() {
  // Set cache size to something large
  var prefService = SpecialPowers.wrap(Components).classes["@mozilla.org/preferences-service;1"]
                               .getService(Components.interfaces.nsIPrefService);
  var branch = prefService.getBranch("media.");
  var oldSize = branch.getIntPref("cache_size");
  branch.setIntPref("cache_size", 40000);

  window.addEventListener("unload", function() {
    branch.setIntPref("cache_size", oldSize);
  }, false);
 })();
