// The files in this directory are shared by mochitest-browser-chrome,
// mochitest-chrome, and xpcshell tests. At this time it is simpler to just use
// plugin:mozilla/xpcshell-test for eslint.

"use strict";

module.exports = {
  extends: ["plugin:mozilla/xpcshell-test"],
};
