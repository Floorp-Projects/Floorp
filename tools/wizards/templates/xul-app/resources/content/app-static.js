${license_c}

/* Place JavaScript functions that are *not* event handlers in this file.
 * Use ${filename:app-handlers.js} for JavaScript event handlers.
 */

if (DEBUG)
    dd = function (msg) { dumpln("-*- ${app_name_short}: " + msg); }
else
    dd = function (){};

dd ("sample debug message from ${app_name_long}.");