/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import merge from "./merge";
import * as queue from "./queue";

const LINUX_IMAGE = {
  name: "linux",
  path: "automation/taskcluster/docker"
};

const LINUX_CLANG39_IMAGE = {
  name: "linux-clang-3.9",
  path: "automation/taskcluster/docker-clang-3.9"
};

const FUZZ_IMAGE = {
  name: "fuzz",
  path: "automation/taskcluster/docker-fuzz"
};

const WINDOWS_CHECKOUT_CMD =
  "bash -c \"hg clone -r $NSS_HEAD_REVISION $NSS_HEAD_REPOSITORY nss || " +
    "(sleep 2; hg clone -r $NSS_HEAD_REVISION $NSS_HEAD_REPOSITORY nss) || " +
    "(sleep 5; hg clone -r $NSS_HEAD_REVISION $NSS_HEAD_REPOSITORY nss)\"";

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

  if (task.tests == "bogo" || task.tests == "interop") {
    // No windows
    if (task.platform == "windows2012-64") {
      return false;
    }

    // No ARM; TODO: enable
    if (task.platform == "aarch64") {
      return false;
    }
  }

  // Only old make builds have -Ddisable_libpkix=0 and can run chain tests.
  if (task.tests == "chains" && task.collection != "make" &&
      task.platform != "windows2012-64") {
    return false;
  }

  if (task.group == "Test") {
    // Don't run test builds on old make platforms
    if (task.collection == "make") {
      return false;
    }
  }


  // Don't run additional hardware tests on ARM (we don't have anything there).
  if (task.group == "Cipher" && task.platform == "aarch64" && task.env &&
      (task.env.NSS_DISABLE_PCLMUL == "1" || task.env.NSS_DISABLE_HW_AES == "1"
       || task.env.NSS_DISABLE_AVX == "1")) {
    return false;
  }

  return true;
});

queue.map(task => {
  if (task.collection == "asan") {
    // CRMF and FIPS tests still leak, unfortunately.
    if (task.tests == "crmf" || task.tests == "fips") {
      task.env.ASAN_OPTIONS = "detect_leaks=0";
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
    platform: "linux32",
    image: LINUX_IMAGE
  }, "-m32 --opt");

  await scheduleLinux("Linux 32 (debug)", {
    platform: "linux32",
    collection: "debug",
    image: LINUX_IMAGE
  }, "-m32");

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

  await scheduleWindows("Windows 2012 64 (opt)", {
    env: {BUILD_OPT: "1"}
  });

  await scheduleWindows("Windows 2012 64 (debug)", {
    collection: "debug"
  });

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
    merge({
      command: [
        "/bin/bash",
        "-c",
        "bin/checkout.sh && nss/automation/taskcluster/scripts/build_gyp.sh"
      ],
      collection: "debug",
    }, aarch64_base)
  );

  await scheduleLinux("Linux AArch64 (opt)",
    merge({
      command: [
        "/bin/bash",
        "-c",
        "bin/checkout.sh && nss/automation/taskcluster/scripts/build_gyp.sh --opt"
      ],
      collection: "opt",
    }, aarch64_base)
  );
}

/*****************************************************************************/

async function scheduleLinux(name, base, args = "") {
  // Build base definition.
  let build_base = merge({
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/build_gyp.sh " + args
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
    name: `${name} w/ clang-4.0`,
    env: {
      CC: "clang",
      CCC: "clang++",
    },
    symbol: "clang-4.0"
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
    name: `${name} w/ modular builds`,
    env: {NSS_BUILD_MODULAR: "1"},
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/build.sh",
    ],
    symbol: "modular"
  }));

  await scheduleTestBuilds(merge(base, {group: "Test"}), args);

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

  // The task that builds NSPR+NSS (TLS fuzzing mode).
  let task_build_tls = queue.scheduleTask(merge(build_base, {
    name: "Linux x64 (debug, TLS fuzz)",
    symbol: "B",
    group: "TLS",
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && " +
      "nss/automation/taskcluster/scripts/build_gyp.sh -g -v --fuzz=tls"
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
    image: FUZZ_IMAGE
  };

  // Build base definition.
  let build_base = merge({
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && " +
      "nss/automation/taskcluster/scripts/build_gyp.sh -g -v --fuzz -m32"
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
      "nss/automation/taskcluster/scripts/build_gyp.sh -g -v --fuzz=tls -m32"
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

async function scheduleTestBuilds(base, args = "") {
  // Build base definition.
  let build = merge({
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && " +
      "nss/automation/taskcluster/scripts/build_gyp.sh -g -v --test --ct-verif " + args
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
    name: "Linux 64 (debug, test)"
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
  queue.scheduleTask(merge(base, {
    parent: task_build,
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/run_tests.sh"
    ],
    name: "Gtests",
    symbol: "Gtest",
    tests: "gtests",
    cycle: "standard",
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
    name: "Interop tests", symbol: "Interop", tests: "interop", cycle: "standard"
  }));
  queue.scheduleTask(merge(no_cert_base, {
    name: "Chains tests", symbol: "Chains", tests: "chains"
  }));
  queue.scheduleTask(merge(no_cert_base, {
    name: "Cipher tests", symbol: "Default", tests: "cipher", group: "Cipher"
  }));
  queue.scheduleTask(merge(no_cert_base, {
    name: "Cipher tests", symbol: "NoAESNI", tests: "cipher",
    env: {NSS_DISABLE_HW_AES: "1"}, group: "Cipher"
  }));
  queue.scheduleTask(merge(no_cert_base, {
    name: "Cipher tests", symbol: "NoPCLMUL", tests: "cipher",
    env: {NSS_DISABLE_PCLMUL: "1"}, group: "Cipher"
  }));
  queue.scheduleTask(merge(no_cert_base, {
    name: "Cipher tests", symbol: "NoAVX", tests: "cipher",
    env: {NSS_DISABLE_AVX: "1"}, group: "Cipher"
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
    platform: "nss-tools",
    kind: "test"
  };

  queue.scheduleTask(merge(base, {
    symbol: "clang-format-3.9",
    name: "clang-format-3.9",
    image: LINUX_CLANG39_IMAGE,
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/run_clang_format.sh"
    ]
  }));

  queue.scheduleTask(merge(base, {
    symbol: "scan-build-4.0",
    name: "scan-build-4.0",
    image: LINUX_IMAGE,
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
