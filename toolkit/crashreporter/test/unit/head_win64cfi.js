/* import-globals-from head_crashreporter.js */

let gTestCrasherSyms = null;
let gModules = null;

// Returns the offset (int) of an IP with a given base address.
// This is effectively (ip - base), except a bit more complication due to
// Javascript's shaky handling of 64-bit integers.
// base & ip are passed as hex strings.
function getModuleOffset(base, ip) {
  let i = 0;
  // Find where the two addresses diverge, which enables us to perform a 32-bit
  // subtraction.
  // e.g.    "0x1111111111112222"
  //       - "0x1111111111111111"
  // becomes 2222 - 1111
  for (; i < base.length; ++i) {
    if (base[i] != ip[i]) {
      break;
    }
  }
  if (i == base.length) {
    return 0;
  }
  let lhs2 = "0x" + base.substring(i);
  let rhs2 = "0x" + ip.substring(i);
  return parseInt(rhs2) - parseInt(lhs2);
}

// Uses gTestCrasherSyms to convert an address to a symbol.
function findNearestTestCrasherSymbol(addr) {
  addr += 1; // Breakpad sometimes offsets addresses; correct for this.
  let closestDistance = null;
  let closestSym = null;
  for (let sym in gTestCrasherSyms) {
    if (addr >= gTestCrasherSyms[sym]) {
      let thisDistance = addr - gTestCrasherSyms[sym];
      if (closestDistance === null || thisDistance < closestDistance) {
        closestDistance = thisDistance;
        closestSym = sym;
      }
    }
  }
  if (closestSym === null) {
    return null;
  }
  return { symbol: closestSym, offset: closestDistance };
}

// Populate known symbols for testcrasher.dll.
// Use the same prop names as from CrashTestUtils to avoid the need for mapping.
function initTestCrasherSymbols() {
  gTestCrasherSyms = { };
  for (let k in CrashTestUtils) {
    // Not all keys here are valid symbol names. getWin64CFITestFnAddrOffset
    // will return 0 in those cases, no need to filter here.
    if (Number.isInteger(CrashTestUtils[k])) {
      let t = CrashTestUtils.getWin64CFITestFnAddrOffset(CrashTestUtils[k]);
      if (t > 0) {
        gTestCrasherSyms[k] = t;
      }
    }
  }
}

function stackFrameToString(frameIndex, frame) {
  // Calculate the module offset.
  let ip = frame.ip;
  let symbol = "";
  let moduleOffset = "unknown_offset";
  let filename = "unknown_module";

  if (typeof frame.module_index !== "undefined" && (frame.module_index >= 0)
    && (frame.module_index < gModules.length)) {

    let base = gModules[frame.module_index].base_addr;
    moduleOffset = getModuleOffset(base, ip);
    filename = gModules[frame.module_index].filename;

    if (filename === "testcrasher.dll") {
      let nearestSym = findNearestTestCrasherSymbol(moduleOffset);
      if (nearestSym !== null) {
        symbol = nearestSym.symbol + "+" + nearestSym.offset.toString(16);
      }
    }
  }

  let ret = "frames[" + frameIndex + "] ip=" + ip +
    " " + symbol +
    ", module:" + filename +
    ", trust:" + frame.trust +
    ", moduleOffset:" + moduleOffset.toString(16);
  return ret;
}

function dumpStackFrames(frames, maxFrames) {
  for (let i = 0; i < Math.min(maxFrames, frames.length); ++i) {
    info(stackFrameToString(i, frames[i]));
  }
}

// Test that the top of the given stack (from extra data) matches the given
// expected frames.
//
// expected is { symbol: "", trust: "" }
function assertStack(stack, expected) {
  for (let i = 0; i < stack.length; ++i) {
    if (i >= expected.length) {
      ok("Top stack frames were expected");
      return;
    }
    let frame = stack[i];
    let expectedFrame = expected[i];
    let dumpThisFrame = function() {
      info("  Actual frame: " + stackFrameToString(i, frame));
      info("Expected { symbol: " + expectedFrame.symbol + ", trust: " + expectedFrame.trust + "}");
    };

    if (expectedFrame.trust) {
      if (frame.trust !== expectedFrame.trust) {
        dumpThisFrame();
        info("Expected frame trust did not match.");
        ok(false);
      }
    }

    if (expectedFrame.symbol) {
      if (typeof frame.module_index === "undefined") {
        // Without a module_index, it happened in an unknown module. Currently
        // you can't specify an expected "unknown" module.
        info("Unknown symbol in unknown module.");
        ok(false);
      }
      if (frame.module_index < 0 || frame.module_index >= gModules.length) {
        dumpThisFrame();
        info("Unknown module.");
        ok(false);
        return;
      }
      let base = gModules[frame.module_index].base_addr;
      let moduleOffset = getModuleOffset(base, frame.ip);
      let filename = gModules[frame.module_index].filename;
      if (filename == "testcrasher.dll") {
        let nearestSym = findNearestTestCrasherSymbol(moduleOffset);
        if (nearestSym === null) {
          dumpThisFrame();
          info("Unknown symbol.");
          ok(false);
          return;
        }

        if (nearestSym.symbol !== expectedFrame.symbol) {
          dumpThisFrame();
          info("Mismatching symbol.");
          ok(false);
        }
      }
    }
  }
}

// Performs a crash, runs minidump-analyzer, and checks expected stack analysis.
//
// how: The crash to perform. Constants defined in both CrashTestUtils.jsm
//   and nsTestCrasher.cpp (i.e. CRASH_X64CFI_PUSH_NONVOL)
// expectedStack: An array of {"symbol", "trust"} where trust is "cfi",
//   "context", "scan", et al. May be null if you don't need to check the stack.
// minidumpAnalyzerArgs: An array of additional arguments to pass to
//   minidump-analyzer.exe.
function do_x64CFITest(how, expectedStack, minidumpAnalyzerArgs) {

  // Setup is run in the subprocess so we cannot use any closures.
  let setupFn = "crashType = CrashTestUtils." + how + ";";

  let callbackFn = function(minidumpFile, extra, extraFile) {
    runMinidumpAnalyzer(minidumpFile, minidumpAnalyzerArgs);

    // Refresh updated extra data
    extra = parseKeyValuePairsFromFile(extraFile);

    initTestCrasherSymbols();
    let stackTraces = JSON.parse(extra.StackTraces);
    let crashingThreadIndex = stackTraces.crash_info.crashing_thread;
    gModules = stackTraces.modules;
    let crashingFrames = stackTraces.threads[crashingThreadIndex].frames;

    dumpStackFrames(crashingFrames, 10);

    assertStack(crashingFrames, expectedStack);
  };

  do_crash(setupFn, callbackFn, true, true);
}
