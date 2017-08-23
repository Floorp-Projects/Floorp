"use strict";

module.exports = {
  "globals": {
    // JS files in this folder are commonly xpcshell scripts where |arguments|
    // and |__LOCATION__| are defined in the global scope.
    "__LOCATION__": false,
    "arguments": false
  }
};
