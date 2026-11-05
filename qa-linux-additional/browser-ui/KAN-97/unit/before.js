var qaScrollTests = (() => {
  var __defProp = Object.defineProperty;
  var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
  var __getOwnPropNames = Object.getOwnPropertyNames;
  var __hasOwnProp = Object.prototype.hasOwnProperty;
  var __export = (target, all) => {
    for (var name in all)
      __defProp(target, name, { get: all[name], enumerable: true });
  };
  var __copyProps = (to, from, except, desc) => {
    if (from && typeof from === "object" || typeof from === "function") {
      for (let key of __getOwnPropNames(from))
        if (!__hasOwnProp.call(to, key) && key !== except)
          __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
    }
    return to;
  };
  var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);

  // floorp-kan97/browser-features/chrome/common/mouse-gesture/test/scrollTarget.test.ts
  var scrollTarget_test_exports = {};
  __export(scrollTarget_test_exports, {
    runAllTests: () => runAllTests
  });

  // floorp-kan97/browser-features/chrome/common/mouse-gesture/utils/scroll-target.ts
  function getScrollTargetContext(selected2, _focused) {
    return selected2;
  }

  // floorp-kan97/browser-features/chrome/test/utils/test_harness.ts
  function assertEquals(actual, expected, message) {
    if (actual !== expected) {
      throw new Error(
        `${message} (expected: ${String(expected)}, actual: ${String(actual)})`
      );
    }
  }
  async function runTests(moduleName, tests2) {
    const failures = [];
    for (const [index, test] of tests2.entries()) {
      globalThis.__NORA_TEST_PROGRESS__ = {
        moduleName,
        testName: test.name,
        status: "running",
        index: index + 1,
        total: tests2.length,
        startedAtMs: Date.now()
      };
      try {
        await test.fn();
        const startedAtMs = globalThis.__NORA_TEST_PROGRESS__?.startedAtMs ?? Date.now();
        globalThis.__NORA_TEST_PROGRESS__ = {
          moduleName,
          testName: test.name,
          status: "passed",
          index: index + 1,
          total: tests2.length,
          startedAtMs
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        failures.push(`${test.name}: ${message}`);
        const startedAtMs = globalThis.__NORA_TEST_PROGRESS__?.startedAtMs ?? Date.now();
        globalThis.__NORA_TEST_PROGRESS__ = {
          moduleName,
          testName: test.name,
          status: "failed",
          index: index + 1,
          total: tests2.length,
          startedAtMs
        };
      }
    }
    globalThis.__NORA_TEST_PROGRESS__ = {
      moduleName,
      testName: "",
      status: "done",
      index: tests2.length,
      total: tests2.length,
      startedAtMs: Date.now()
    };
    if (failures.length > 0) {
      throw new Error(`${moduleName} failures: ${failures.join(" | ")}`);
    }
  }

  // floorp-kan97/browser-features/chrome/common/mouse-gesture/test/scrollTarget.test.ts
  function rootContext() {
    return {
      get top() {
        return this;
      }
    };
  }
  var selected = rootContext();
  var foreign = rootContext();
  var frame = { top: selected };
  var nestedFrame = { top: frame.top };
  var tests = [
    {
      name: "no focused content falls back to the selected tab",
      fn: () => assertEquals(
        getScrollTargetContext(selected, null),
        selected,
        "null focus"
      )
    },
    {
      name: "unavailable focus falls back to the selected tab",
      fn: () => assertEquals(
        getScrollTargetContext(selected, void 0),
        selected,
        "undefined focus"
      )
    },
    {
      name: "focused selected document remains the target",
      fn: () => assertEquals(
        getScrollTargetContext(selected, selected),
        selected,
        "root focus"
      )
    },
    {
      name: "focused iframe receives scroll commands",
      fn: () => assertEquals(
        getScrollTargetContext(selected, frame),
        frame,
        "focused frame"
      )
    },
    {
      name: "focused nested descendant receives scroll commands",
      fn: () => assertEquals(
        getScrollTargetContext(selected, nestedFrame),
        nestedFrame,
        "nested frame"
      )
    },
    {
      name: "another tab or window cannot redirect the command",
      fn: () => assertEquals(
        getScrollTargetContext(selected, foreign),
        selected,
        "foreign root"
      )
    },
    {
      name: "a descendant of another tab is not a target",
      fn: () => assertEquals(
        getScrollTargetContext(selected, { top: foreign }),
        selected,
        "foreign frame"
      )
    }
  ];
  async function runAllTests() {
    await runTests("scrollTarget.test.ts", tests);
  }
  return __toCommonJS(scrollTarget_test_exports);
})();
