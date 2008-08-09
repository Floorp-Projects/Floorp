(function() {

  // NOTE: You *must* also include this line in any test that uses this file:
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);

  function determinePrefKind(branch, prefName) {
    switch (branch.getPrefType(prefName)) {
    case branch.PREF_STRING:    return "CharPref";
    case branch.PREF_INT:       return "IntPref";
    case branch.PREF_BOOL:      return "BoolPref";
    default: /* PREF_INVALID */ return "ComplexValue";
    }
  }
  
  function memoize(fn, obj) {
    var cache = {}, sep = '___',
        join = Array.prototype.join;
    return function() {
      var key = join.call(arguments, sep);
      if (!(key in cache))
        cache[key] = fn.apply(obj, arguments);
      return cache[key];
    };
  }
  
  var makeAccessor = memoize(function(pref) {
    var splat = pref.split('.'),
        basePref = splat.pop(),
        branch, kind;
    
    try {
      branch = prefService.getBranch(splat.join('.') + '.')
    } catch (e) {
      alert("Calling prefService.getBranch failed: " + 
        "did you read the NOTE in mozprefs.js?");
      throw e;
    }
    
    kind = determinePrefKind(branch, basePref);
    
    return function(value) {
      var oldValue = branch['get' + kind](basePref);
      if (arguments.length > 0)
        branch['set' + kind](basePref, value);
      return oldValue;
    };
  });

  /* function pref(name[, value[, fn[, obj]]])
   * -----------------------------------------
   * Use cases:
   *
   *   1. Get the value of the dom.disable_open_during_load preference:
   *
   *      pref('dom.disable_open_during_load')
   *
   *   2. Set the preference to true, returning the old value:
   *
   *      var oldValue = pref('dom.disable_open_during_load', true);
   *
   *   3. Set the value of the preference to true just for the duration
   *      of the specified function's execution:
   *
   *      pref('dom.disable_open_during_load', true, function() {
   *        window.open(this.getUrl()); // fails if still loading
   *      }, this); // for convenience, allow binding
   *
   *      Rationale: Unless a great deal of care is taken to catch all
   *                 exceptions and restore original preference values,
   *                 manually setting & restoring preferences can lead
   *                 to unpredictable test behavior.  The try-finally
   *                 block below eliminates that risk.
   */
  function pref(name, /*optional:*/ value, fn, obj) {
    var acc = makeAccessor(name);
    switch (arguments.length) {
    case 1: return acc();
    case 2: return acc(value);
    default:
      var oldValue = acc(value),
          extra_args = [].slice.call(arguments, 4);
      try { return fn.apply(obj, extra_args) }
      finally { acc(oldValue) } // reset no matter what
    }
  };
  
  window.pref = pref; // export
 
})();
