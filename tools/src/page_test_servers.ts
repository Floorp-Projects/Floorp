// SPDX-License-Identifier: MPL-2.0
import * as path from "@std/path";
import { PROJECT_ROOT } from "./defines.ts";

// These origins deliberately do not match the privileged page actor allowlist.
// UI fixtures install in-memory Services/actor doubles before loading React.
export async function startPageTestServers(targets: string[]) {
  const children: Deno.ChildProcess[] = [];
  const stop = async () => {
    for (const child of children) {
      try {
        child.kill("SIGTERM");
      } catch { /* Already exited. */ }
      await child.status;
    }
  };
  try {
    for (
      const [page, port] of [["pages-settings", 5196], [
        "pages-welcome",
        5197,
      ]] as const
    ) {
      if (
        !targets.some((target) =>
          target.replaceAll("\\", "/").startsWith(
            `browser-features/${page}/test/integration/`,
          )
        )
      ) continue;
      const url = `http://localhost:${port}/test/integration/index.html`;
      const ready = async () => {
        try {
          const response = await fetch(url, {
            signal: AbortSignal.timeout(1000),
          });
          return response.ok && (await response.text()).includes("./entry.ts");
        } catch {
          return false;
        }
      };
      if (await ready()) continue;
      const child = new Deno.Command(Deno.execPath(), {
        args: [
          "run",
          "-A",
          "vite",
          "--config",
          "test/integration/vite.config.ts",
        ],
        cwd: path.join(PROJECT_ROOT, "browser-features", page),
        stdin: "null",
        stdout: "null",
        stderr: "inherit",
      }).spawn();
      children.push(child);
      let exited = false;
      void child.status.then(() => {
        exited = true;
      });
      const deadline = Date.now() + 30_000;
      while (!(await ready())) {
        if (exited || Date.now() >= deadline) {
          throw new Error(`UI test server failed to start: ${url}`);
        }
        await new Promise((resolve) => setTimeout(resolve, 100));
      }
    }
    return stop;
  } catch (error) {
    await stop();
    throw error;
  }
}
