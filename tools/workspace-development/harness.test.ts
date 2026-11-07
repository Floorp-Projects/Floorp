import { runTests } from "../../browser-features/chrome/test/utils/test_harness.ts";

Deno.test("development case logging preserves the first failure after later success", async () => {
  const priorLog = console.log;
  const priorProgress = globalThis.__NORA_TEST_PROGRESS__;
  const lines: string[] = [];
  console.log = (...args: unknown[]) => lines.push(args.map(String).join(" "));
  let failure: unknown;
  try {
    try {
      await runTests("nativeLastTabWorkspacesEnabled.test.ts", [
        {
          name: "first",
          fn: () => {
            throw new Error("original assertion");
          },
        },
        { name: "later", fn: () => {} },
      ]);
    } catch (error) {
      failure = error;
    }
    const events = lines.map((line) =>
      JSON.parse(decodeURIComponent(line.split(" ")[1]))
    );
    if (
      !(failure instanceof Error) ||
      !failure.message.includes("original assertion") || events.length !== 4 ||
      events[1].status !== "failed" ||
      events[1].error !== "original assertion" || events[3].status !== "passed"
    ) {
      throw new Error(
        "Original failure was lost or overwritten by later success",
      );
    }
  } finally {
    console.log = priorLog;
    globalThis.__NORA_TEST_PROGRESS__ = priorProgress;
  }
});
