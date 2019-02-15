/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as queue from "./queue";
import intersect from "intersect";
import parse_args from "minimist";
import util from "util";
import child_process from 'child_process';

let execFile = util.promisify(child_process.execFile);

function parseOptions(opts) {
  opts = parse_args(opts.split(/\s+/), {
    default: {build: "do", platform: "all", unittests: "none", tools: "none"},
    alias: {b: "build", p: "platform", u: "unittests", t: "tools", e: "extra-builds"},
    string: ["build", "platform", "unittests", "tools", "extra-builds"]
  });

  // Parse build types (d=debug, o=opt).
  let builds = intersect(opts.build.split(""), ["d", "o"]);

  // If the given value is nonsense default to debug and opt builds.
  if (builds.length == 0) {
    builds = ["d", "o"];
  }

  // Parse platforms.
  let allPlatforms = ["linux", "linux64", "linux64-asan", "linux64-fips",
                      "win", "win64", "win-make", "win64-make",
                      "linux64-make", "linux-make", "linux-fuzz",
                      "linux64-fuzz", "aarch64", "mac"];
  let platforms = intersect(opts.platform.split(/\s*,\s*/), allPlatforms);

  // If the given value is nonsense or "none" default to all platforms.
  if (platforms.length == 0 && opts.platform != "none") {
    platforms = allPlatforms;
  }

  // Parse unit tests.
  let aliases = {"gtests": "gtest"};
  let allUnitTests = ["bogo", "crmf", "chains", "cipher", "db", "ec", "fips",
                      "gtest", "interop", "lowhash", "merge", "sdr", "smime", "tools",
                      "ssl", "mpi", "scert", "spki", "policy", "tlsfuzzer"];
  let unittests = intersect(opts.unittests.split(/\s*,\s*/).map(t => {
    return aliases[t] || t;
  }), allUnitTests);

  // If the given value is "all" run all tests.
  // If it's nonsense then don't run any tests.
  if (opts.unittests == "all") {
    unittests = allUnitTests;
  } else if (unittests.length == 0) {
    unittests = [];
  }

  // Parse tools.
  let allTools = ["clang-format", "scan-build", "hacl", "saw", "abi", "coverage"];
  let tools = intersect(opts.tools.split(/\s*,\s*/), allTools);

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

function filter(opts) {
  return function (task) {
    // Filter tools. We can immediately return here as those
    // are not affected by platform or build type selectors.
    if (task.platform == "nss-tools") {
      return opts.tools.some(tool => {
        return task.symbol.toLowerCase().startsWith(tool) ||
               (task.group && task.group.toLowerCase().startsWith(tool));
      });
    }

    // Filter unit tests.
    if (task.tests) {
      let found = opts.unittests.some(test => {
        if (task.group && task.group.toLowerCase() == "ssl" && test == "ssl") {
          return true;
        }
        if (task.group && task.group.toLowerCase() == "cipher" && test == "cipher") {
          return true;
        }
        return task.symbol.toLowerCase().startsWith(test);
      });

      if (!found) {
        return false;
      }
    }

    // Filter extra builds.
    if (task.group == "Builds" && !opts.extra) {
      return false;
    }

    let coll = name => name == (task.collection || "opt");

    // Filter by platform.
    let found = opts.platforms.some(platform => {
      let aliases = {
        "linux": "linux32",
        "linux-fuzz": "linux32",
        "linux64-asan": "linux64",
        "linux64-fips": "linux64",
        "linux64-fuzz": "linux64",
        "linux64-make": "linux64",
        "linux-make": "linux32",
        "win64-make": "windows2012-64",
        "win-make": "windows2012-32",
        "win64": "windows2012-64",
        "win": "windows2012-32"
      };

      // Check the platform name.
      let keep = (task.platform == (aliases[platform] || platform));

      // Additional checks.
      if (platform == "linux64-asan") {
        keep &= coll("asan");
      } else if (platform == "linux64-fips") {
        keep &= coll("fips");
      } else if (platform == "linux64-make" || platform == "linux-make" ||
                 platform == "win64-make" || platform == "win-make") {
        keep &= coll("make");
      } else if (platform == "linux64-fuzz" || platform == "linux-fuzz") {
        keep &= coll("fuzz");
      } else {
        keep &= coll("opt") || coll("debug");
      }

      return keep;
    });

    if (!found) {
      return false;
    }

    // Finally, filter by build type.
    let isDebug = coll("debug") || coll("asan") || coll("make") ||
                  coll("fuzz");
    return (isDebug && opts.builds.includes("d")) ||
           (!isDebug && opts.builds.includes("o"));
  }
}

async function getCommitComment() {
  const res = await execFile('hg', ['log', '-r', '.', '-T', '{desc}']);
  return res.stdout;
};

export async function initFilter() {
  let comment = await getCommitComment();

  // Check for try syntax in changeset comment.
  let match = comment.match(/^\s*try:\s*(.*)\s*$/);

  // Add try syntax filter.
  if (match) {
    queue.filter(filter(parseOptions(match[1])));
  }
}
