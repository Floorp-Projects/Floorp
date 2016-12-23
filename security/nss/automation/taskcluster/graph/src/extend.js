/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import merge from "./merge";
import * as queue from "./queue";

const LINUX_IMAGE = {name: "linux", path: "automation/taskcluster/docker"};
const FUZZ_IMAGE = {name: "fuzz", path: "automation/taskcluster/docker-fuzz"};

const WINDOWS_CHECKOUT_CMD =
  "bash -c \"hg clone -r $NSS_HEAD_REVISION $NSS_HEAD_REPOSITORY nss || " +
    "(sleep 2; hg clone -r $NSS_HEAD_REVISION $NSS_HEAD_REPOSITORY nss) || " +
    "(sleep 5; hg clone -r $NSS_HEAD_REVISION $NSS_HEAD_REPOSITORY nss)\"";

/*****************************************************************************/

queue.filter(task => {
  if (task.group == "Builds") {
    // Remove extra builds on {A,UB}San and ARM.
    if (task.collection == "asan" || task.collection == "arm-debug" ||
        task.collection == "gyp-asan") {
      return false;
    }

    // Remove extra builds w/o libpkix for non-linux64-debug.
    if (task.symbol == "noLibpkix" &&
        (task.platform != "linux64" || task.collection != "debug")) {
      return false;
    }
  }

  if (task.tests == "bogo") {
    // No BoGo tests on Windows.
    if (task.platform == "windows2012-64") {
      return false;
    }

    // No BoGo tests on ARM.
    if (task.collection == "arm-debug") {
      return false;
    }
  }

  // GYP builds with -Ddisable_libpkix=1 by default.
  if ((task.collection == "gyp" || task.collection == "gyp-asan") &&
      task.tests == "chains") {
    return false;
  }

  return true;
});

queue.map(task => {
  if (task.collection == "asan" || task.collection == "gyp-asan") {
    // CRMF and FIPS tests still leak, unfortunately.
    if (task.tests == "crmf" || task.tests == "fips") {
      task.env.ASAN_OPTIONS = "detect_leaks=0";
    }
  }

  if (task.collection == "arm-debug") {
    // These tests take quite some time on our poor ARM devices.
    if (task.tests == "chains" || (task.tests == "ssl" && task.cycle == "standard")) {
      task.maxRunTime = 14400;
    }
  }

  // Windows is slow.
  if (task.platform == "windows2012-64" && task.tests == "chains") {
    task.maxRunTime = 7200;
  }

  return task;
});

/*****************************************************************************/

export default async function main() {
  await scheduleLinux("Linux 32 (opt)", {
    env: {BUILD_OPT: "1"},
    platform: "linux32",
    image: LINUX_IMAGE
  });

  await scheduleLinux("Linux 32 (debug)", {
    platform: "linux32",
    collection: "debug",
    image: LINUX_IMAGE
  });

  await scheduleLinux("Linux 64 (opt)", {
    env: {USE_64: "1", BUILD_OPT: "1"},
    platform: "linux64",
    image: LINUX_IMAGE
  });

  await scheduleLinux("Linux 64 (debug)", {
    env: {USE_64: "1"},
    platform: "linux64",
    collection: "debug",
    image: LINUX_IMAGE
  });

  await scheduleLinux("Linux 64 (debug, gyp)", {
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/build_gyp.sh"
    ],
    platform: "linux64",
    collection: "gyp",
    image: LINUX_IMAGE
  });

  await scheduleLinux("Linux 64 (debug, gyp, asan, ubsan)", {
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/build_gyp.sh -g -v --ubsan --asan"
    ],
    env: {
      ASAN_OPTIONS: "detect_odr_violation=0", // bug 1316276
      UBSAN_OPTIONS: "print_stacktrace=1",
      NSS_DISABLE_ARENA_FREE_LIST: "1",
      NSS_DISABLE_UNLOAD: "1",
      CC: "clang",
      CCC: "clang++"
    },
    platform: "linux64",
    collection: "gyp-asan",
    image: LINUX_IMAGE
  });

  await scheduleLinux("Linux 64 (ASan, debug)", {
    env: {
      UBSAN_OPTIONS: "print_stacktrace=1",
      NSS_DISABLE_ARENA_FREE_LIST: "1",
      NSS_DISABLE_UNLOAD: "1",
      CC: "clang",
      CCC: "clang++",
      USE_UBSAN: "1",
      USE_ASAN: "1",
      USE_64: "1"
    },
    platform: "linux64",
    collection: "asan",
    image: LINUX_IMAGE
  });

  await scheduleWindows("Windows 2012 64 (opt)", {
    env: {BUILD_OPT: "1"}
  });

  await scheduleWindows("Windows 2012 64 (debug)", {
    collection: "debug"
  });

  await scheduleFuzzing();

  await scheduleTestBuilds();

  await scheduleTools();

  await scheduleLinux("Linux 32 (ARM, debug)", {
    image: "franziskus/nss-arm-ci",
    provisioner: "localprovisioner",
    collection: "arm-debug",
    workerType: "nss-rpi",
    platform: "linux32",
    maxRunTime: 7200,
    tier: 3
  });
}

/*****************************************************************************/

async function scheduleLinux(name, base) {
  // Build base definition.
  let build_base = merge({
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/build.sh"
    ],
    artifacts: {
      public: {
        expires: 24 * 7,
        type: "directory",
        path: "/home/worker/artifacts"
      }
    },
    kind: "build",
    symbol: "B"
  }, base);

  // The task that builds NSPR+NSS.
  let task_build = queue.scheduleTask(merge(build_base, {name}));

  // The task that generates certificates.
  let task_cert = queue.scheduleTask(merge(build_base, {
    name: "Certificates",
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/gen_certs.sh"
    ],
    parent: task_build,
    symbol: "Certs"
  }));

  // Schedule tests.
  scheduleTests(task_build, task_cert, merge(base, {
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/run_tests.sh"
    ]
  }));

  // Extra builds.
  let extra_base = merge({group: "Builds"}, build_base);
  queue.scheduleTask(merge(extra_base, {
    name: `${name} w/ clang-3.9`,
    env: {
      CC: "clang",
      CCC: "clang++",
    },
    symbol: "clang-3.9"
  }));

  queue.scheduleTask(merge(extra_base, {
    name: `${name} w/ gcc-4.8`,
    env: {
      CC: "gcc-4.8",
      CCC: "g++-4.8"
    },
    symbol: "gcc-4.8"
  }));

  queue.scheduleTask(merge(extra_base, {
    name: `${name} w/ gcc-6.1`,
    env: {
      CC: "gcc-6",
      CCC: "g++-6"
    },
    symbol: "gcc-6.1"
  }));

  queue.scheduleTask(merge(extra_base, {
    name: `${name} w/ NSS_DISABLE_LIBPKIX=1`,
    env: {NSS_DISABLE_LIBPKIX: "1"},
    symbol: "noLibpkix"
  }));

  return queue.submit();
}

/*****************************************************************************/

async function scheduleFuzzing() {
  let base = {
    env: {
       // bug 1316276
      ASAN_OPTIONS: "allocator_may_return_null=1:detect_odr_violation=0",
      UBSAN_OPTIONS: "print_stacktrace=1",
      NSS_DISABLE_ARENA_FREE_LIST: "1",
      NSS_DISABLE_UNLOAD: "1",
      CC: "clang",
      CCC: "clang++"
    },
    platform: "linux64",
    collection: "fuzz",
    image: FUZZ_IMAGE
  };

  // Build base definition.
  let build_base = merge({
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && " +
      "nss/automation/taskcluster/scripts/build_gyp.sh -g -v --fuzz"
    ],
    artifacts: {
      public: {
        expires: 24 * 7,
        type: "directory",
        path: "/home/worker/artifacts"
      }
    },
    kind: "build",
    symbol: "B"
  }, base);

  // The task that builds NSPR+NSS.
  let task_build = queue.scheduleTask(merge(build_base, {
    name: "Linux x64 (debug, fuzz)"
  }));

  // Schedule tests.
  queue.scheduleTask(merge(base, {
    parent: task_build,
    name: "Gtests",
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/run_tests.sh"
    ],
    env: {GTESTFILTER: "*Fuzz*"},
    tests: "ssl_gtests gtests",
    cycle: "standard",
    symbol: "Gtest",
    kind: "test"
  }));

  queue.scheduleTask(merge(base, {
    parent: task_build,
    name: "Cert",
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/fuzz.sh " +
        "cert nss/fuzz/corpus/cert -max_total_time=300"
    ],
    // Need a privileged docker container to remove this.
    env: {ASAN_OPTIONS: "detect_leaks=0"},
    symbol: "SCert",
    kind: "test"
  }));

  queue.scheduleTask(merge(base, {
    parent: task_build,
    name: "SPKI",
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/fuzz.sh " +
        "spki nss/fuzz/corpus/spki -max_total_time=300"
    ],
    // Need a privileged docker container to remove this.
    env: {ASAN_OPTIONS: "detect_leaks=0"},
    symbol: "SPKI",
    kind: "test"
  }));

  return queue.submit();
}

/*****************************************************************************/

async function scheduleTestBuilds() {
  let base = {
    platform: "linux64",
    collection: "gyp",
    group: "Test",
    image: LINUX_IMAGE
  };

  // Build base definition.
  let build = merge({
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && " +
      "nss/automation/taskcluster/scripts/build_gyp.sh -g -v --test"
    ],
    artifacts: {
      public: {
        expires: 24 * 7,
        type: "directory",
        path: "/home/worker/artifacts"
      }
    },
    kind: "build",
    symbol: "B",
    name: "Linux 64 (debug, gyp, test)"
  }, base);

  // The task that builds NSPR+NSS.
  let task_build = queue.scheduleTask(build);

  // Schedule tests.
  queue.scheduleTask(merge(base, {
    parent: task_build,
    name: "mpi",
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/run_tests.sh"
    ],
    tests: "mpi",
    cycle: "standard",
    symbol: "mpi",
    kind: "test"
  }));

  return queue.submit();
}


/*****************************************************************************/

async function scheduleWindows(name, base) {
  base = merge(base, {
    workerType: "nss-win2012r2",
    platform: "windows2012-64",
    env: {
      PATH: "c:\\mozilla-build\\python;c:\\mozilla-build\\msys\\local\\bin;" +
            "c:\\mozilla-build\\7zip;c:\\mozilla-build\\info-zip;" +
            "c:\\mozilla-build\\python\\Scripts;c:\\mozilla-build\\yasm;" +
            "c:\\mozilla-build\\msys\\bin;c:\\Windows\\system32;" +
            "c:\\mozilla-build\\upx391w;c:\\mozilla-build\\moztools-x64\\bin;" +
            "c:\\mozilla-build\\wget",
      DOMSUF: "localdomain",
      HOST: "localhost",
      USE_64: "1"
    }
  });

  // Build base definition.
  let build_base = merge(base, {
    command: [
      WINDOWS_CHECKOUT_CMD,
      "bash -c nss/automation/taskcluster/windows/build.sh"
    ],
    artifacts: [{
      expires: 24 * 7,
      type: "directory",
      path: "public\\build"
    }],
    kind: "build",
    symbol: "B"
  });

  // The task that builds NSPR+NSS.
  let task_build = queue.scheduleTask(merge(build_base, {name}));

  // The task that generates certificates.
  let task_cert = queue.scheduleTask(merge(build_base, {
    name: "Certificates",
    command: [
      WINDOWS_CHECKOUT_CMD,
      "bash -c nss/automation/taskcluster/windows/gen_certs.sh"
    ],
    parent: task_build,
    symbol: "Certs"
  }));

  // Schedule tests.
  scheduleTests(task_build, task_cert, merge(base, {
    command: [
      WINDOWS_CHECKOUT_CMD,
      "bash -c nss/automation/taskcluster/windows/run_tests.sh"
    ]
  }));

  return queue.submit();
}

/*****************************************************************************/

function scheduleTests(task_build, task_cert, test_base) {
  test_base = merge({kind: "test"}, test_base);

  // Schedule tests that do NOT need certificates.
  let no_cert_base = merge(test_base, {parent: task_build});
  queue.scheduleTask(merge(no_cert_base, {
    name: "Gtests", symbol: "Gtest", tests: "ssl_gtests gtests", cycle: "standard"
  }));
  queue.scheduleTask(merge(no_cert_base, {
    name: "Bogo tests", symbol: "Bogo", tests: "bogo", cycle: "standard"
  }));
  queue.scheduleTask(merge(no_cert_base, {
    name: "Chains tests", symbol: "Chains", tests: "chains"
  }));
  queue.scheduleTask(merge(no_cert_base, {
    name: "Cipher tests", symbol: "Cipher", tests: "cipher"
  }));
  queue.scheduleTask(merge(no_cert_base, {
    name: "EC tests", symbol: "EC", tests: "ec"
  }));
  queue.scheduleTask(merge(no_cert_base, {
    name: "Lowhash tests", symbol: "Lowhash", tests: "lowhash"
  }));
  queue.scheduleTask(merge(no_cert_base, {
    name: "SDR tests", symbol: "SDR", tests: "sdr"
  }));

  // Schedule tests that need certificates.
  let cert_base = merge(test_base, {parent: task_cert});
  queue.scheduleTask(merge(cert_base, {
    name: "CRMF tests", symbol: "CRMF", tests: "crmf"
  }));
  queue.scheduleTask(merge(cert_base, {
    name: "DB tests", symbol: "DB", tests: "dbtests"
  }));
  queue.scheduleTask(merge(cert_base, {
    name: "FIPS tests", symbol: "FIPS", tests: "fips"
  }));
  queue.scheduleTask(merge(cert_base, {
    name: "Merge tests", symbol: "Merge", tests: "merge"
  }));
  queue.scheduleTask(merge(cert_base, {
    name: "S/MIME tests", symbol: "SMIME", tests: "smime"
  }));
  queue.scheduleTask(merge(cert_base, {
    name: "Tools tests", symbol: "Tools", tests: "tools"
  }));

  // SSL tests, need certificates too.
  let ssl_base = merge(cert_base, {tests: "ssl", group: "SSL"});
  queue.scheduleTask(merge(ssl_base, {
    name: "SSL tests (standard)", symbol: "standard", cycle: "standard"
  }));
  queue.scheduleTask(merge(ssl_base, {
    name: "SSL tests (pkix)", symbol: "pkix", cycle: "pkix"
  }));
  queue.scheduleTask(merge(ssl_base, {
    name: "SSL tests (sharedb)", symbol: "sharedb", cycle: "sharedb"
  }));
  queue.scheduleTask(merge(ssl_base, {
    name: "SSL tests (upgradedb)", symbol: "upgradedb", cycle: "upgradedb"
  }));
}

/*****************************************************************************/

async function scheduleTools() {
  let base = {
    image: LINUX_IMAGE,
    platform: "nss-tools",
    kind: "test"
  };

  queue.scheduleTask(merge(base, {
    symbol: "clang-format-3.9",
    name: "clang-format-3.9",
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/run_clang_format.sh"
    ]
  }));

  queue.scheduleTask(merge(base, {
    symbol: "scan-build-3.9",
    name: "scan-build-3.9",
    env: {
      USE_64: "1",
      CC: "clang",
      CCC: "clang++",
    },
    artifacts: {
      public: {
        expires: 24 * 7,
        type: "directory",
        path: "/home/worker/artifacts"
      }
    },
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/run_scan_build.sh"
    ]
  }));

  return queue.submit();
}
