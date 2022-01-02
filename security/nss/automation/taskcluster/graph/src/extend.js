/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import merge from "./merge";
import * as queue from "./queue";

const LINUX_IMAGE = {
  name: "linux",
  path: "automation/taskcluster/docker"
};

const LINUX_BUILDS_IMAGE = {
  name: "linux-builds",
  path: "automation/taskcluster/docker-builds"
};

const LINUX_INTEROP_IMAGE = {
  name: "linux-interop",
  path: "automation/taskcluster/docker-interop"
};

const CLANG_FORMAT_IMAGE = {
  name: "clang-format",
  path: "automation/taskcluster/docker-clang-format"
};

const LINUX_GCC44_IMAGE = {
  name: "linux-gcc-4.4",
  path: "automation/taskcluster/docker-gcc-4.4"
};

const FUZZ_IMAGE = {
  name: "fuzz",
  path: "automation/taskcluster/docker-fuzz"
};

// Bug 1488148 - temporary image for fuzzing 32-bit builds.
const FUZZ_IMAGE_32 = {
  name: "fuzz32",
  path: "automation/taskcluster/docker-fuzz32"
};

const SAW_IMAGE = {
  name: "saw",
  path: "automation/taskcluster/docker-saw"
};

const WINDOWS_CHECKOUT_CMD =
  "bash -c \"hg clone -r $NSS_HEAD_REVISION $NSS_HEAD_REPOSITORY nss || " +
    "(sleep 2; hg clone -r $NSS_HEAD_REVISION $NSS_HEAD_REPOSITORY nss) || " +
    "(sleep 5; hg clone -r $NSS_HEAD_REVISION $NSS_HEAD_REPOSITORY nss)\"";
const MAC_CHECKOUT_CMD = ["bash", "-c",
            "hg clone -r $NSS_HEAD_REVISION $NSS_HEAD_REPOSITORY nss || " +
            "(sleep 2; hg clone -r $NSS_HEAD_REVISION $NSS_HEAD_REPOSITORY nss) || " +
            "(sleep 5; hg clone -r $NSS_HEAD_REVISION $NSS_HEAD_REPOSITORY nss)"];

/*****************************************************************************/

queue.filter(task => {
  if (task.group == "Builds") {
    // Remove extra builds on {A,UB}San and ARM.
    if (task.collection == "asan" || task.platform == "aarch64") {
      return false;
    }

    // Make modular builds only on Linux make.
    if (task.symbol == "modular" && task.collection != "make") {
      return false;
    }
  }

  if (task.tests == "bogo" || task.tests == "interop" || task.tests == "tlsfuzzer") {
    // No windows
    if (task.platform == "windows2012-64" ||
        task.platform == "windows2012-32") {
      return false;
    }

    // No ARM; TODO: enable
    if (task.platform == "aarch64") {
      return false;
    }

    // No mac
    if (task.platform == "mac") {
      return false;
    }
  }

  if (task.tests == "fips" &&
     (task.platform == "mac" || task.platform == "aarch64")) {
    return false;
  }

  // Only old make builds have -Ddisable_libpkix=0 and can run chain tests.
  if (task.tests == "chains" && task.collection != "make") {
    return false;
  }

  // Don't run all additional hardware tests on ARM.
  if (task.group == "Cipher" && task.platform == "aarch64" && task.env &&
      (task.env.NSS_DISABLE_PCLMUL == "1" || task.env.NSS_DISABLE_SSE4_1 == "1"
       || task.env.NSS_DISABLE_AVX == "1" || task.env.NSS_DISABLE_AVX2 == "1")) {
    return false;
  }

  // Don't run ARM specific hardware tests on non-ARM.
  // TODO: our server that runs task cluster doesn't support Intel SHA extensions.
  if (task.group == "Cipher" && task.platform != "aarch64" && task.env &&
      (task.env.NSS_DISABLE_HW_SHA1 == "1" || task.env.NSS_DISABLE_HW_SHA2 == "1")) {
    return false;
  }

  // Don't run DBM builds on aarch64.
  if (task.group == "DBM" && task.platform == "aarch64") {
    return false;
  }

  return true;
});

queue.map(task => {
  if (task.collection == "asan") {
    // CRMF and FIPS tests still leak, unfortunately.
    if (task.tests == "crmf") {
      task.env.ASAN_OPTIONS = "detect_leaks=0";
    }
  }

  if (task.tests == "ssl") {
    if (!task.env) {
      task.env = {};
    }

    // Stress tests to not include other SSL tests
    if (task.symbol == "stress") {
      task.env.NSS_SSL_TESTS = "normal_normal";
    } else {
      task.env.NSS_SSL_TESTS = "crl iopr policy normal_normal";
    }

    // FIPS runs
    if (task.collection == "fips") {
      task.env.NSS_SSL_TESTS += " fips_fips fips_normal normal_fips";
    }

    if (task.platform == "mac") {
      task.maxRunTime = 7200;
    }
  }

  // Windows is slow.
  if ((task.platform == "windows2012-32" || task.platform == "windows2012-64") &&
      task.tests == "chains") {
    task.maxRunTime = 7200;
  }

  if (task.platform == "mac" && task.tests == "tools") {
      task.maxRunTime = 7200;
  }
  return task;
});

/*****************************************************************************/

export default async function main() {
  await scheduleLinux("Linux 32 (opt)", {
    platform: "linux32",
    image: LINUX_IMAGE
  }, "-t ia32 --opt");

  await scheduleLinux("Linux 32 (debug)", {
    platform: "linux32",
    collection: "debug",
    image: LINUX_IMAGE
  }, "-t ia32");

  await scheduleLinux("Linux 64 (opt)", {
    platform: "linux64",
    image: LINUX_IMAGE
  }, "--opt");

  await scheduleLinux("Linux 64 (debug)", {
    platform: "linux64",
    collection: "debug",
    image: LINUX_IMAGE
  });

  await scheduleLinux("Linux 64 (debug, make)", {
    env: {USE_64: "1"},
    platform: "linux64",
    image: LINUX_IMAGE,
    collection: "make",
    command: [
       "/bin/bash",
       "-c",
       "bin/checkout.sh && nss/automation/taskcluster/scripts/build.sh"
    ],
  });

  await scheduleLinux("Linux 64 (opt, make)", {
    env: {USE_64: "1", BUILD_OPT: "1"},
    platform: "linux64",
    image: LINUX_IMAGE,
    collection: "make",
    command: [
       "/bin/bash",
       "-c",
       "bin/checkout.sh && nss/automation/taskcluster/scripts/build.sh"
    ],
  });

  await scheduleLinux("Linux 32 (debug, make)", {
    platform: "linux32",
    image: LINUX_IMAGE,
    collection: "make",
    command: [
       "/bin/bash",
       "-c",
       "bin/checkout.sh && nss/automation/taskcluster/scripts/build.sh"
    ],
  });

  await scheduleLinux("Linux 64 (ASan, debug)", {
    env: {
      UBSAN_OPTIONS: "print_stacktrace=1",
      NSS_DISABLE_ARENA_FREE_LIST: "1",
      NSS_DISABLE_UNLOAD: "1",
      CC: "clang",
      CCC: "clang++",
    },
    platform: "linux64",
    collection: "asan",
    image: LINUX_IMAGE,
    features: ["allowPtrace"],
  }, "--ubsan --asan");

  await scheduleLinux("Linux 64 (FIPS opt)", {
    platform: "linux64",
    collection: "fips",
    image: LINUX_IMAGE,
  }, "--enable-fips --opt");

  await scheduleWindows("Windows 2012 64 (debug, make)", {
    platform: "windows2012-64",
    collection: "make",
    env: {USE_64: "1"}
  }, "build.sh");

  await scheduleWindows("Windows 2012 32 (debug, make)", {
    platform: "windows2012-32",
    collection: "make"
  }, "build.sh");

  await scheduleWindows("Windows 2012 64 (opt)", {
    platform: "windows2012-64",
  }, "build_gyp.sh --opt");

  await scheduleWindows("Windows 2012 64 (debug)", {
    platform: "windows2012-64",
    collection: "debug"
  }, "build_gyp.sh");

  await scheduleWindows("Windows 2012 32 (opt)", {
    platform: "windows2012-32",
  }, "build_gyp.sh --opt -t ia32");

  await scheduleWindows("Windows 2012 32 (debug)", {
    platform: "windows2012-32",
    collection: "debug"
  }, "build_gyp.sh -t ia32");

  await scheduleFuzzing();
  await scheduleFuzzing32();

  await scheduleTools();

  let aarch64_base = {
    image: "franziskus/nss-aarch64-ci",
    provisioner: "localprovisioner",
    workerType: "nss-aarch64",
    platform: "aarch64",
    maxRunTime: 7200
  };

  await scheduleLinux("Linux AArch64 (debug)",
    merge(aarch64_base, {
      command: [
        "/bin/bash",
        "-c",
        "bin/checkout.sh && nss/automation/taskcluster/scripts/build_gyp.sh"
      ],
      collection: "debug",
    })
  );

  await scheduleLinux("Linux AArch64 (opt)",
    merge(aarch64_base, {
      command: [
        "/bin/bash",
        "-c",
        "bin/checkout.sh && nss/automation/taskcluster/scripts/build_gyp.sh --opt"
      ],
      collection: "opt",
    })
  );

  await scheduleLinux("Linux AArch64 (debug, make)",
    merge(aarch64_base, {
      env: {USE_64: "1"},
      command: [
         "/bin/bash",
         "-c",
         "bin/checkout.sh && nss/automation/taskcluster/scripts/build.sh"
      ],
      collection: "make",
    })
  );

  await scheduleMac("Mac (opt)", {collection: "opt"}, "--opt");
  await scheduleMac("Mac (debug)", {collection: "debug"});

  // Must be executed after all other tasks are scheduled
  queue.clearFilters();
  await scheduleCodeReview();
}


async function scheduleMac(name, base, args = "") {
  let mac_base = merge(base, {
    env: {
      PATH: "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin",
      NSS_TASKCLUSTER_MAC: "1",
      DOMSUF: "localdomain",
      HOST: "localhost",
    },
    provisioner: "localprovisioner",
    workerType: "nss-macos-10-12",
    platform: "mac"
  });

  // Build base definition.
  let build_base_without_command_symbol = merge(mac_base, {
    provisioner: "localprovisioner",
    workerType: "nss-macos-10-12",
    platform: "mac",
    maxRunTime: 7200,
    artifacts: [{
      expires: 24 * 7,
      type: "directory",
      path: "public"
    }],
    kind: "build",
  });

  let gyp_cmd = "nss/automation/taskcluster/scripts/build_gyp.sh ";

  if (!("collection" in base) ||
      (base.collection != "make" &&
       base.collection != "asan" &&
       base.collection != "fips" &&
       base.collection != "fuzz")) {
    let nspr_gyp = gyp_cmd + "--nspr-only --nspr-test-build --nspr-test-run ";
    let nspr_build = merge(build_base_without_command_symbol, {
      command: [
        MAC_CHECKOUT_CMD,
        ["bash", "-c",
         nspr_gyp + args]
      ],
      symbol: "NSPR"
    });
    // The task that tests NSPR.
    let nspr_task_build = queue.scheduleTask(merge(nspr_build, {name}));
  }

  let build_base = merge(build_base_without_command_symbol, {
    command: [
      MAC_CHECKOUT_CMD,
      ["bash", "-c",
       gyp_cmd + args]
    ],
    symbol: "B"
  });

  // The task that builds NSPR+NSS.
  let task_build = queue.scheduleTask(merge(build_base, {name}));

  // The task that generates certificates.
  let task_cert = queue.scheduleTask(merge(build_base, {
    name: "Certificates",
    command: [
      MAC_CHECKOUT_CMD,
      ["bash", "-c",
       "nss/automation/taskcluster/scripts/gen_certs.sh"]
    ],
    parent: task_build,
    symbol: "Certs"
  }));

  // Schedule tests.
  scheduleTests(task_build, task_cert, merge(mac_base, {
    command: [
      MAC_CHECKOUT_CMD,
      ["bash", "-c",
       "nss/automation/taskcluster/scripts/run_tests.sh"]
    ]
  }));

  return queue.submit();
}

/*****************************************************************************/

async function scheduleLinux(name, overrides, args = "") {
  let checkout_and_gyp = "bin/checkout.sh && nss/automation/taskcluster/scripts/build_gyp.sh ";
  let artifacts_and_kind = {
    artifacts: {
      public: {
        expires: 24 * 7,
        type: "directory",
        path: "/home/worker/artifacts"
      }
    },
    kind: "build",
  };

  if (!("collection" in overrides) ||
       (overrides.collection != "make" &&
        overrides.collection != "asan" &&
        overrides.collection != "fips" &&
        overrides.collection != "fuzz")) {
    let nspr_gyp = checkout_and_gyp + "--nspr-only --nspr-test-build --nspr-test-run ";

    let nspr_base = merge({
      command: [
        "/bin/bash",
        "-c",
        nspr_gyp + args
      ],
    }, overrides);
    let nspr_without_symbol = merge(nspr_base, artifacts_and_kind);
    let nspr_build = merge(nspr_without_symbol, {
      symbol: "NSPR",
    });
    // The task that tests NSPR.
    let nspr_task_build = queue.scheduleTask(merge(nspr_build, {name}));
  }

  // Construct a base definition.  This takes |overrides| second because
  // callers expect to be able to overwrite the |command| key.
  let base = merge({
    command: [
      "/bin/bash",
      "-c",
      checkout_and_gyp + args
    ],
  }, overrides);

  let base_without_symbol = merge(base, artifacts_and_kind);

  // The base for building.
  let build_base = merge(base_without_symbol, {
    symbol: "B",
  });

  // The task that builds NSPR+NSS.
  let task_build = queue.scheduleTask(merge(build_base, {name}));

  // Make builds run FIPS tests, which need an extra FIPS build.
  if (base.collection == "make") {
    let extra_build = queue.scheduleTask(merge(build_base, {
      env: { NSS_FORCE_FIPS: "1" },
      group: "FIPS",
      name: `${name} w/ NSS_FORCE_FIPS`
    }));

    // The task that generates certificates.
    let task_cert = queue.scheduleTask(merge(build_base, {
      name: "Certificates",
      command: [
        "/bin/bash",
        "-c",
        "bin/checkout.sh && nss/automation/taskcluster/scripts/gen_certs.sh"
      ],
      parent: extra_build,
      symbol: "Certs-F",
      group: "FIPS",
    }));

    // Schedule FIPS tests.
    queue.scheduleTask(merge(base, {
      parent: task_cert,
      name: "FIPS",
      command: [
        "/bin/bash",
        "-c",
        "bin/checkout.sh && nss/automation/taskcluster/scripts/run_tests.sh"
      ],
      cycle: "standard",
      kind: "test",
      name: "FIPS tests",
      symbol: "Tests-F",
      tests: "fips",
      group: "FIPS"
    }));
  }

  // The task that generates certificates.
  let cert_base = merge(build_base, {
    name: "Certificates",
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/gen_certs.sh"
    ],
    parent: task_build,
    symbol: "Certs"
  });
  let task_cert = queue.scheduleTask(cert_base);

  // Schedule tests.
  scheduleTests(task_build, task_cert, merge(base, {
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/run_tests.sh"
    ]
  }));

  // Extra builds.
  let extra_base = merge(build_base, {
    group: "Builds",
    image: LINUX_BUILDS_IMAGE,
  });
  queue.scheduleTask(merge(extra_base, {
    name: `${name} w/ clang-4`,
    env: {
      CC: "clang-4.0",
      CCC: "clang++-4.0",
    },
    symbol: "clang-4"
  }));
  queue.scheduleTask(merge(extra_base, {
    name: `${name} w/ gcc-4.4`,
    image: LINUX_GCC44_IMAGE,
    env: {
      USE_64: "1",
      CC: "gcc-4.4",
      CCC: "g++-4.4",
      // gcc-4.6 introduced nullptr.
      NSS_DISABLE_GTESTS: "1",
    },
    // Use the old Makefile-based build system, GYP doesn't have a proper GCC
    // version check for __int128 support. It's mainly meant to cover RHEL6.
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/build.sh",
    ],
    symbol: "gcc-4.4"
  }));

  queue.scheduleTask(merge(extra_base, {
    name: `${name} w/ gcc-4.8`,
    env: {
      CC: "gcc-4.8",
      CCC: "g++-4.8"
    },
    // Use -Ddisable-intelhw_sha=1, GYP doesn't have a proper GCC version
    // check for Intel SHA support.
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/build_gyp.sh -Ddisable_intel_hw_sha=1"
    ],
    symbol: "gcc-4.8"
  }));

  queue.scheduleTask(merge(extra_base, {
    name: `${name} w/ gcc-5`,
    env: {
      CC: "gcc-5",
      CCC: "g++-5"
    },
    symbol: "gcc-5"
  }));

  queue.scheduleTask(merge(extra_base, {
    name: `${name} w/ gcc-6`,
    env: {
      CC: "gcc-6",
      CCC: "g++-6"
    },
    symbol: "gcc-6"
  }));

  queue.scheduleTask(merge(extra_base, {
    name: `${name} w/ modular builds`,
    image: LINUX_IMAGE,
    env: {NSS_BUILD_MODULAR: "1"},
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/build.sh",
    ],
    symbol: "modular"
  }));

  if (base.collection != "make") {
    let task_build_dbm = queue.scheduleTask(merge(extra_base, {
      name: `${name} w/ legacy-db`,
      command: [
        "/bin/bash",
        "-c",
        checkout_and_gyp + "--enable-legacy-db"
      ],
      symbol: "B",
      group: "DBM",
    }));

    let task_cert_dbm = queue.scheduleTask(merge(cert_base, {
      parent: task_build_dbm,
      group: "DBM",
      symbol: "Certs"
    }));
  }

  return queue.submit();
}

/*****************************************************************************/

function scheduleFuzzingRun(base, name, target, max_len, symbol = null, corpus = null) {
  const MAX_FUZZ_TIME = 300;

  queue.scheduleTask(merge(base, {
    name,
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/fuzz.sh " +
        `${target} nss/fuzz/corpus/${corpus || target} ` +
        `-max_total_time=${MAX_FUZZ_TIME} ` +
        `-max_len=${max_len}`
    ],
    symbol: symbol || name
  }));
}

async function scheduleFuzzing() {
  let base = {
    env: {
      ASAN_OPTIONS: "allocator_may_return_null=1:detect_stack_use_after_return=1",
      UBSAN_OPTIONS: "print_stacktrace=1",
      NSS_DISABLE_ARENA_FREE_LIST: "1",
      NSS_DISABLE_UNLOAD: "1",
      CC: "clang",
      CCC: "clang++"
    },
    features: ["allowPtrace"],
    platform: "linux64",
    collection: "fuzz",
    image: FUZZ_IMAGE
  };

  // Build base definition.
  let build_base = merge(base, {
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && " +
      "nss/automation/taskcluster/scripts/build_gyp.sh --fuzz"
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
  });

  // The task that builds NSPR+NSS.
  let task_build = queue.scheduleTask(merge(build_base, {
    name: "Linux x64 (debug, fuzz)"
  }));

  // The task that builds NSPR+NSS (TLS fuzzing mode).
  let task_build_tls = queue.scheduleTask(merge(build_base, {
    name: "Linux x64 (debug, TLS fuzz)",
    symbol: "B",
    group: "TLS",
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && " +
      "nss/automation/taskcluster/scripts/build_gyp.sh --fuzz=tls"
    ],
  }));

  // Schedule tests.
  queue.scheduleTask(merge(base, {
    parent: task_build_tls,
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

  // Schedule fuzzing runs.
  let run_base = merge(base, {parent: task_build, kind: "test"});
  scheduleFuzzingRun(run_base, "CertDN", "certDN", 4096);
  scheduleFuzzingRun(run_base, "QuickDER", "quickder", 10000);

  // Schedule MPI fuzzing runs.
  let mpi_base = merge(run_base, {group: "MPI"});
  let mpi_names = ["add", "addmod", "div", "mod", "mulmod", "sqr",
                   "sqrmod", "sub", "submod"];
  for (let name of mpi_names) {
    scheduleFuzzingRun(mpi_base, `MPI (${name})`, `mpi-${name}`, 4096, name);
  }
  scheduleFuzzingRun(mpi_base, `MPI (invmod)`, `mpi-invmod`, 256, "invmod");
  scheduleFuzzingRun(mpi_base, `MPI (expmod)`, `mpi-expmod`, 2048, "expmod");

  // Schedule TLS fuzzing runs (non-fuzzing mode).
  let tls_base = merge(run_base, {group: "TLS"});
  scheduleFuzzingRun(tls_base, "TLS Client", "tls-client", 20000, "client-nfm",
                     "tls-client-no_fuzzer_mode");
  scheduleFuzzingRun(tls_base, "TLS Server", "tls-server", 20000, "server-nfm",
                     "tls-server-no_fuzzer_mode");
  scheduleFuzzingRun(tls_base, "DTLS Client", "dtls-client", 20000,
                     "dtls-client-nfm", "dtls-client-no_fuzzer_mode");
  scheduleFuzzingRun(tls_base, "DTLS Server", "dtls-server", 20000,
                     "dtls-server-nfm", "dtls-server-no_fuzzer_mode");

  // Schedule TLS fuzzing runs (fuzzing mode).
  let tls_fm_base = merge(tls_base, {parent: task_build_tls});
  scheduleFuzzingRun(tls_fm_base, "TLS Client", "tls-client", 20000, "client");
  scheduleFuzzingRun(tls_fm_base, "TLS Server", "tls-server", 20000, "server");
  scheduleFuzzingRun(tls_fm_base, "DTLS Client", "dtls-client", 20000, "dtls-client");
  scheduleFuzzingRun(tls_fm_base, "DTLS Server", "dtls-server", 20000, "dtls-server");

  return queue.submit();
}

async function scheduleFuzzing32() {
  let base = {
    env: {
      ASAN_OPTIONS: "allocator_may_return_null=1:detect_stack_use_after_return=1",
      UBSAN_OPTIONS: "print_stacktrace=1",
      NSS_DISABLE_ARENA_FREE_LIST: "1",
      NSS_DISABLE_UNLOAD: "1",
      CC: "clang",
      CCC: "clang++"
    },
    features: ["allowPtrace"],
    platform: "linux32",
    collection: "fuzz",
    image: FUZZ_IMAGE_32
  };

  // Build base definition.
  let build_base = merge(base, {
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && " +
      "nss/automation/taskcluster/scripts/build_gyp.sh --fuzz -t ia32"
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
  });

  // The task that builds NSPR+NSS.
  let task_build = queue.scheduleTask(merge(build_base, {
    name: "Linux 32 (debug, fuzz)"
  }));

  // The task that builds NSPR+NSS (TLS fuzzing mode).
  let task_build_tls = queue.scheduleTask(merge(build_base, {
    name: "Linux 32 (debug, TLS fuzz)",
    symbol: "B",
    group: "TLS",
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && " +
      "nss/automation/taskcluster/scripts/build_gyp.sh --fuzz=tls -t ia32"
    ],
  }));

  // Schedule tests.
  queue.scheduleTask(merge(base, {
    parent: task_build_tls,
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

  // Schedule fuzzing runs.
  let run_base = merge(base, {parent: task_build, kind: "test"});
  scheduleFuzzingRun(run_base, "CertDN", "certDN", 4096);
  scheduleFuzzingRun(run_base, "QuickDER", "quickder", 10000);

  // Schedule MPI fuzzing runs.
  let mpi_base = merge(run_base, {group: "MPI"});
  let mpi_names = ["add", "addmod", "div", "expmod", "mod", "mulmod", "sqr",
                   "sqrmod", "sub", "submod"];
  for (let name of mpi_names) {
    scheduleFuzzingRun(mpi_base, `MPI (${name})`, `mpi-${name}`, 4096, name);
  }
  scheduleFuzzingRun(mpi_base, `MPI (invmod)`, `mpi-invmod`, 256, "invmod");

  // Schedule TLS fuzzing runs (non-fuzzing mode).
  let tls_base = merge(run_base, {group: "TLS"});
  scheduleFuzzingRun(tls_base, "TLS Client", "tls-client", 20000, "client-nfm",
                     "tls-client-no_fuzzer_mode");
  scheduleFuzzingRun(tls_base, "TLS Server", "tls-server", 20000, "server-nfm",
                     "tls-server-no_fuzzer_mode");
  scheduleFuzzingRun(tls_base, "DTLS Client", "dtls-client", 20000,
                     "dtls-client-nfm", "dtls-client-no_fuzzer_mode");
  scheduleFuzzingRun(tls_base, "DTLS Server", "dtls-server", 20000,
                     "dtls-server-nfm", "dtls-server-no_fuzzer_mode");

  // Schedule TLS fuzzing runs (fuzzing mode).
  let tls_fm_base = merge(tls_base, {parent: task_build_tls});
  scheduleFuzzingRun(tls_fm_base, "TLS Client", "tls-client", 20000, "client");
  scheduleFuzzingRun(tls_fm_base, "TLS Server", "tls-server", 20000, "server");
  scheduleFuzzingRun(tls_fm_base, "DTLS Client", "dtls-client", 20000, "dtls-client");
  scheduleFuzzingRun(tls_fm_base, "DTLS Server", "dtls-server", 20000, "dtls-server");

  return queue.submit();
}

/*****************************************************************************/

async function scheduleWindows(name, base, build_script) {
  base = merge(base, {
    workerType: "win2012r2",
    env: {
      PATH: "c:\\mozilla-build\\bin;c:\\mozilla-build\\python;" +
           "c:\\mozilla-build\\msys\\local\\bin;c:\\mozilla-build\\7zip;" +
           "c:\\mozilla-build\\info-zip;c:\\mozilla-build\\python\\Scripts;" +
           "c:\\mozilla-build\\yasm;c:\\mozilla-build\\msys\\bin;" +
           "c:\\Windows\\system32;c:\\mozilla-build\\upx391w;" +
           "c:\\mozilla-build\\moztools-x64\\bin;c:\\mozilla-build\\wget",
      DOMSUF: "localdomain",
      HOST: "localhost",
    },
    features: ["taskclusterProxy"],
    scopes: ["project:releng:services/tooltool/api/download/internal"],
  });

  let artifacts_and_kind = {
    artifacts: [{
      expires: 24 * 7,
      type: "directory",
      path: "public\\build"
    }],
    kind: "build",
  };

  let build_without_command_symbol = merge(base, artifacts_and_kind);

  // Build base definition.
  let build_base = merge(build_without_command_symbol, {
    command: [
      WINDOWS_CHECKOUT_CMD,
      `bash -c 'nss/automation/taskcluster/windows/${build_script}'`
    ],
    symbol: "B"
  });

  if (!("collection" in base) ||
      (base.collection != "make" &&
       base.collection != "asan" &&
       base.collection != "fips" &&
       base.collection != "fuzz")) {
    let nspr_gyp =
      `bash -c 'nss/automation/taskcluster/windows/${build_script} --nspr-only --nspr-test-build --nspr-test-run'`;
    let nspr_build = merge(build_without_command_symbol, {
      command: [
        WINDOWS_CHECKOUT_CMD,
        nspr_gyp
      ],
      symbol: "NSPR"
    });
    // The task that tests NSPR.
    let task_build = queue.scheduleTask(merge(nspr_build, {name}));
  }

  // Make builds run FIPS tests, which need an extra FIPS build.
  if (base.collection == "make") {
    let extra_build = queue.scheduleTask(merge(build_base, {
      env: { NSS_FORCE_FIPS: "1" },
      group: "FIPS",
      name: `${name} w/ NSS_FORCE_FIPS`
    }));

    // The task that generates certificates.
    let task_cert = queue.scheduleTask(merge(build_base, {
      name: "Certificates",
      command: [
        WINDOWS_CHECKOUT_CMD,
        "bash -c nss/automation/taskcluster/windows/gen_certs.sh"
      ],
      parent: extra_build,
      symbol: "Certs-F",
      group: "FIPS",
    }));

    // Schedule FIPS tests.
    queue.scheduleTask(merge(base, {
      parent: task_cert,
      name: "FIPS",
      command: [
        WINDOWS_CHECKOUT_CMD,
        "bash -c nss/automation/taskcluster/windows/run_tests.sh"
      ],
      cycle: "standard",
      kind: "test",
      name: "FIPS tests",
      symbol: "Tests-F",
      tests: "fips",
      group: "FIPS"
    }));
  }

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
  test_base = merge(test_base, {kind: "test"});
  let no_cert_base = merge(test_base, {parent: task_build});
  let cert_base = merge(test_base, {parent: task_cert});
  let cert_base_long = merge(cert_base, {maxRunTime: 7200});

  // Schedule tests that do NOT need certificates. This is defined as
  // the test itself not needing certs AND not running under the upgradedb
  // cycle (which itself needs certs). If cycle is not defined, default is all.
  queue.scheduleTask(merge(no_cert_base, {
    name: "Gtests", symbol: "Gtest", tests: "ssl_gtests gtests", cycle: "standard"
  }));
  queue.scheduleTask(merge(no_cert_base, {
    name: "Bogo tests",
    symbol: "Bogo",
    tests: "bogo",
    cycle: "standard",
    image: LINUX_INTEROP_IMAGE,
  }));
  queue.scheduleTask(merge(no_cert_base, {
    name: "Interop tests",
    symbol: "Interop",
    tests: "interop",
    cycle: "standard",
    image: LINUX_INTEROP_IMAGE,
  }));
  queue.scheduleTask(merge(no_cert_base, {
    name: "tlsfuzzer tests", symbol: "tlsfuzzer", tests: "tlsfuzzer", cycle: "standard"
  }));
  queue.scheduleTask(merge(no_cert_base, {
    name: "MPI tests", symbol: "MPI", tests: "mpi", cycle: "standard"
  }));
  queue.scheduleTask(merge(cert_base, {
    name: "Chains tests", symbol: "Chains", tests: "chains"
  }));
  queue.scheduleTask(merge(cert_base_long, {
    name: "Cipher tests", symbol: "Default", tests: "cipher", group: "Cipher"
  }));
  queue.scheduleTask(merge(cert_base_long, {
    name: "Cipher tests", symbol: "NoAES", tests: "cipher",
    env: {NSS_DISABLE_HW_AES: "1"}, group: "Cipher"
  }));
  queue.scheduleTask(merge(cert_base_long, {
    name: "Cipher tests", symbol: "NoSHA", tests: "cipher",
    env: {
      NSS_DISABLE_HW_SHA1: "1",
      NSS_DISABLE_HW_SHA2: "1"
    }, group: "Cipher"
  }));
  queue.scheduleTask(merge(cert_base_long, {
    name: "Cipher tests", symbol: "NoPCLMUL", tests: "cipher",
    env: {NSS_DISABLE_PCLMUL: "1"}, group: "Cipher"
  }));
  queue.scheduleTask(merge(cert_base_long, {
    name: "Cipher tests", symbol: "NoAVX", tests: "cipher",
    env: {NSS_DISABLE_AVX: "1"}, group: "Cipher"
  }));
  queue.scheduleTask(merge(cert_base_long, {
    name: "Cipher tests", symbol: "NoAVX2", tests: "cipher",
    env: {NSS_DISABLE_AVX2: "1"}, group: "Cipher"
  }));
  queue.scheduleTask(merge(cert_base_long, {
    name: "Cipher tests", symbol: "NoSSSE3|NEON", tests: "cipher",
    env: {
      NSS_DISABLE_ARM_NEON: "1",
      NSS_DISABLE_SSSE3: "1"
    }, group: "Cipher"
  }));
  queue.scheduleTask(merge(cert_base_long, {
    name: "Cipher tests", symbol: "NoSSE4.1", tests: "cipher",
    env: {NSS_DISABLE_SSE4_1: "1"}, group: "Cipher"
  }));
  queue.scheduleTask(merge(cert_base, {
    name: "EC tests", symbol: "EC", tests: "ec"
  }));
  queue.scheduleTask(merge(cert_base, {
    name: "Lowhash tests", symbol: "Lowhash", tests: "lowhash"
  }));
  queue.scheduleTask(merge(cert_base, {
    name: "SDR tests", symbol: "SDR", tests: "sdr"
  }));
  queue.scheduleTask(merge(cert_base, {
    name: "Policy tests", symbol: "Policy", tests: "policy"
  }));

  // Schedule tests that need certificates.
  queue.scheduleTask(merge(cert_base, {
    name: "CRMF tests", symbol: "CRMF", tests: "crmf"
  }));
  queue.scheduleTask(merge(cert_base, {
    name: "DB tests", symbol: "DB", tests: "dbtests"
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
    name: "SSL tests (stress)", symbol: "stress", cycle: "sharedb",
    env: {NSS_SSL_RUN: "stress"}
  }));
}

/*****************************************************************************/

async function scheduleTools() {
  let base = {
    platform: "nss-tools",
    kind: "test"
  };

  // ABI check task
  queue.scheduleTask(merge(base, {
    symbol: "abi",
    name: "abi",
    image: LINUX_BUILDS_IMAGE,
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/check_abi.sh"
    ],
  }));

  queue.scheduleTask(merge(base, {
    symbol: "clang-format",
    name: "clang-format",
    image: CLANG_FORMAT_IMAGE,
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/clang-format/run_clang_format.sh"
    ]
  }));

  queue.scheduleTask(merge(base, {
    symbol: "scan-build",
    name: "scan-build",
    image: FUZZ_IMAGE,
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

  queue.scheduleTask(merge(base, {
    symbol: "hacl",
    name: "hacl",
    image: LINUX_BUILDS_IMAGE,
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/run_hacl.sh"
    ]
  }));

  let task_saw = queue.scheduleTask(merge(base, {
    symbol: "B",
    group: "SAW",
    name: "LLVM bitcode build (32 bit)",
    image: SAW_IMAGE,
    kind: "build",
    env: {
      AR: "llvm-ar-3.8",
      CC: "clang-3.8",
      CCC: "clang++-3.8"
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
      "bin/checkout.sh && nss/automation/taskcluster/scripts/build_gyp.sh --disable-tests --emit-llvm -t ia32"
    ]
  }));

  queue.scheduleTask(merge(base, {
    parent: task_saw,
    symbol: "bmul",
    group: "SAW",
    name: "bmul.saw",
    image: SAW_IMAGE,
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/run_saw.sh bmul"
    ]
  }));

  // TODO: The ChaCha20 saw verification is currently disabled because the new
  //       HACL 32-bit code can't be verified by saw right now to the best of
  //       my knowledge.
  //       Bug 1604130
  // queue.scheduleTask(merge(base, {
  //   parent: task_saw,
  //   symbol: "ChaCha20",
  //   group: "SAW",
  //   name: "chacha20.saw",
  //   image: SAW_IMAGE,
  //   command: [
  //     "/bin/bash",
  //     "-c",
  //     "bin/checkout.sh && nss/automation/taskcluster/scripts/run_saw.sh chacha20"
  //   ]
  // }));

  queue.scheduleTask(merge(base, {
    parent: task_saw,
    symbol: "Poly1305",
    group: "SAW",
    name: "poly1305.saw",
    image: SAW_IMAGE,
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/run_saw.sh poly1305"
    ]
  }));

  queue.scheduleTask(merge(base, {
    symbol: "Coverage",
    name: "Coverage",
    image: FUZZ_IMAGE,
    type: "other",
    features: ["allowPtrace"],
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
      "bin/checkout.sh && nss/automation/taskcluster/scripts/gen_coverage_report.sh"
    ]
  }));

  return queue.submit();
}

async function scheduleCodeReview() {
  let tasks = queue.taggedTasks("code-review");
  if(! tasks) {
    console.debug("No code review tasks, skipping ending task");
    return
  }

  // From https://hg.mozilla.org/mozilla-central/file/tip/taskcluster/ci/code-review/kind.yml
  queue.scheduleTask({
    platform: "nss-tools",
    name: "code-review-issues",
    description: "List all issues found in static analysis and linting tasks",

    // No logic on that task
    image: LINUX_IMAGE,
    command: ["/bin/true"],

    // This task must run after all analyzer tasks are completed
    parents: tasks,

    // This option permits to run the task
    // regardless of the analyzers tasks exit status
    // as we are interested in the task failures
    requires: "all-resolved",

    // Publish code review trigger on pulse
    routes: ["project.relman.codereview.v1.try_ending"],

    kind: "code-review",
    symbol: "E"
  });

  return queue.submit();
};
