/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var intersect = require("intersect");
var parse_args = require("minimist");

function parseOptions(opts) {
  opts = parse_args(opts.split(/\s+/), {
    default: {build: "do", platform: "all", unittests: "none", tools: "none"},
    alias: {b: "build", p: "platform", u: "unittests", t: "tools", e: "extra-builds"},
    string: ["build", "platform", "unittests", "tools", "extra-builds"]
  });

  // Parse build types (d=debug, o=opt).
  var builds = intersect(opts.build.split(""), ["d", "o"]);

  // If the given value is nonsense default to debug and opt builds.
  if (builds.length == 0) {
    builds = ["d", "o"];
  }

  // Parse platforms.
  var allPlatforms = ["linux", "linux64", "linux64-asan", "win64", "arm"];
  var platforms = intersect(opts.platform.split(/\s*,\s*/), allPlatforms);

  // If the given value is nonsense or "none" default to all platforms.
  if (platforms.length == 0 && opts.platform != "none") {
    platforms = allPlatforms;
  }

  // Parse unit tests.
  var allUnitTests = ["crmf", "chains", "cipher", "db", "ec", "fips", "gtest",
                      "lowhash", "merge", "sdr", "smime", "tools", "ssl"];
  var unittests = intersect(opts.unittests.split(/\s*,\s*/), allUnitTests);

  // If the given value is "all" run all tests.
  // If it's nonsense then don't run any tests.
  if (opts.unittests == "all") {
    unittests = allUnitTests;
  } else if (unittests.length == 0) {
    unittests = [];
  }

  // Parse tools.
  var allTools = ["clang-format", "scan-build"];
  var tools = intersect(opts.tools.split(/\s*,\s*/), allTools);

  // If the given value is "all" run all tools.
  // If it's nonsense then don't run any tools.
  if (opts.tools == "all") {
    tools = allTools;
  } else if (tools.length == 0) {
    tools = [];
  }

  return {
    builds: builds,
    platforms: platforms,
    unittests: unittests,
    extra: (opts.e == "all"),
    tools: tools
  };
}

function filterTasks(tasks, comment) {
  // Check for try syntax in changeset comment.
  var match = comment.match(/^\s*try:\s*(.*)\s*$/);
  if (!match) {
    return tasks;
  }

  var opts = parseOptions(match[1]);

  return tasks.filter(function (task) {
    var env = task.task.payload.env || {};
    var th = task.task.extra.treeherder;
    var machine = th.machine.platform;
    var coll = th.collection || {};
    var found;

    // Filter tools. We can immediately return here as those
    // are not affected by platform or build type selectors.
    if (machine == "nss-tools") {
      return opts.tools.some(function (tool) {
        var symbol = th.symbol.toLowerCase();
        return symbol.startsWith(tool);
      });
    }

    // Filter unit tests.
    if (env.NSS_TESTS && env.TC_PARENT_TASK_ID) {
      found = opts.unittests.some(function (test) {
        var symbol = (th.groupSymbol || th.symbol).toLowerCase();
        return symbol.startsWith(test);
      });

      if (!found) {
        return false;
      }
    }

    // Filter extra builds.
    if (th.groupSymbol == "Builds" && !opts.extra) {
      return false;
    }

    // Filter by platform.
    found = opts.platforms.some(function (platform) {
      var aliases = {
        "linux": "linux32",
        "linux64-asan": "linux64",
        "win64": "windows2012-64",
        "arm": "linux32"
      };

      // Check the platform name.
      var keep = machine == (aliases[platform] || platform);

      // Additional checks.
      if (platform == "linux64-asan") {
        keep &= coll.asan;
      } else if (platform == "arm") {
        keep &= (coll["arm-opt"] || coll["arm-debug"]);
      } else {
        keep &= (coll.opt || coll.debug);
      }

      return keep;
    });

    if (!found) {
      return false;
    }

    // Finally, filter by build type.
    var isDebug = coll.debug || coll.asan || coll["arm-debug"];
    return (isDebug && opts.builds.indexOf("d") > -1) ||
           (!isDebug && opts.builds.indexOf("o") > -1);
  });
}

module.exports.filterTasks = filterTasks;
