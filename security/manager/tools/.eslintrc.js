"use strict";

module.exports = { // eslint-disable-line no-undef
  "globals": {
    // JS files in this folder are commonly xpcshell scripts where |arguments|
    // is defined in the global scope.
    "arguments": false
  }
};
