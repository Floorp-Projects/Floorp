import { withMenuCleanup } from "../../browser-features/chrome/common/workspaces/test/workspace-development-menu-cleanup.ts";

for (const bodyFails of [true, false]) {
  Deno.test(`menu cleanup failure retains the original error (body failed=${bodyFails})`, async () => {
    const original = new Error("original menu assertion");
    const cleanup = new Error("native menu did not close");
    const prior = console.error;
    const logs: string[] = [];
    console.error = (...args: unknown[]) =>
      logs.push(args.map(String).join(" "));
    let actual: unknown;
    try {
      try {
        await withMenuCleanup(
          () => bodyFails ? Promise.reject(original) : Promise.resolve(),
          () => Promise.reject(cleanup),
        );
      } catch (error) {
        actual = error;
      }
      if (
        actual !== (bodyFails ? original : cleanup) || logs.length !== 1 ||
        JSON.parse(decodeURIComponent(logs[0].split(" ")[1])).error !==
          cleanup.message
      ) {
        throw new Error(
          "Original failure was replaced or separate cleanup evidence is missing",
        );
      }
    } finally {
      console.error = prior;
    }
  });
}
