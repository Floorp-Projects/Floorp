// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { BIN_PATH_EXE, PATHS, PROJECT_ROOT } from "./defines.ts";
import { ProcessUtils } from "./utils.ts";

export const MARIONETTE_STATE_FILE = path.join(PROJECT_ROOT, "_dist", "marionette-port.txt");

/**
 * Minimal port of tools/lib/browser_launcher.rb
 */

function printFirefoxLog(line: string) {
  if (
    /MOZ_CRASH|JavaScript error:|console\.error|\] Errors|\[fluent\] Couldn't find a message:|\[fluent\] Missing|EGL Error:/.test(
      line,
    )
  ) {
    console.log(`\x1b[31m${line}\x1b[0m`);
  } else if (/console\.warn|WARNING:|\[WARN|JavaScript warning:/.test(line)) {
    console.log(`\x1b[33m${line}\x1b[0m`);
  } else if (/console\.debug/.test(line)) {
    console.log(`\x1b[36m${line}\x1b[0m`);
  } else {
    console.log(line);
  }
}

export interface BrowserLaunchOptions {
  port?: number;
  marionette?: boolean;
}

export async function browserCommand(options: BrowserLaunchOptions = {}): Promise<string[]> {
  const { marionette = true } = options;
  const args = [
    BIN_PATH_EXE,
    "--profile",
    PATHS.profile_test,
  ];
  if (marionette) {
    args.push("--marionette", "--remote-allow-system-access");
  }
  return args;
}

export async function run(portOrOptions: number | BrowserLaunchOptions = 5180): Promise<void> {
  const options: BrowserLaunchOptions = typeof portOrOptions === "number"
    ? { port: portOrOptions, marionette: true }
    : portOrOptions;
  const port = options.port ?? 5180;
  const marionette = options.marionette ?? true;
  const cmd = await browserCommand({ port, marionette });

  console.log("[launcher] Launching browser with command: " + cmd.join(" "));

  await ProcessUtils.runCommandWithLogging(
    cmd,
    (stream: "stdout" | "stderr", line: string) => {
      const m = line.match(/Marionette\tINFO\tListening on port (\d+)/);
      if (m) {
        console.log("nora-{bbd11c51-3be9-4676-b912-ca4c0bdcab94}-webdriver");
        Deno.writeTextFileSync(MARIONETTE_STATE_FILE, m[1]);
        console.log(`[launcher] Marionette port saved to ${MARIONETTE_STATE_FILE}`);
      }
      printFirefoxLog(line.trim());
    },
  );

  try {
    Deno.removeSync(MARIONETTE_STATE_FILE);
  } catch {
    // ignore if already removed
  }
  console.log("[launcher] Browser Closed");
}
