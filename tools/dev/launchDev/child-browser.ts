import chalk from "chalk";
import { brandingBaseName, brandingName } from "../../build/defines.ts";

chalk.level = 3;

let logStatusForFollowingLine: "error" | "warn" | "info" | "debug" = "info";
/**
 * @param lines array of strings seperated by newlines (not includes newlines)
 */
function printFirefoxLog(lines: string[]) {
  const MOZ_CRASH = lines.some((v) => v.includes("MOZ_CRASH"));
  for (const str of lines) {
    if (str.replaceAll(" ", "") === "") {
      continue;
    }
    if (!str.startsWith(" ")) {
      if (
        str.includes("JavaScript error:") ||
        str.includes("console.error") ||
        str.includes("] Errors") ||
        str.includes("[fluent] Couldn't find a message:") ||
        str.includes("[fluent] Missing") ||
        str.includes("EGL Error:") ||
        MOZ_CRASH
      ) {
        logStatusForFollowingLine = "error";
      } else if (
        str.includes("console.warn") ||
        str.includes("WARNING:") ||
        str.includes("[WARN") ||
        str.includes("JavaScript warning:")
      ) {
        logStatusForFollowingLine = "warn";
      } else if (str.includes("console.log")) {
        logStatusForFollowingLine = "info";
      } else if (str.includes("console.debug")) {
        logStatusForFollowingLine = "debug";
      } else {
        logStatusForFollowingLine = "info";
      }
    }
    switch (logStatusForFollowingLine) {
      case "error": {
        console.log(chalk.hex("#E67373")(str));
        break;
      }
      case "warn": {
        console.log(chalk.hex("#D1D13F")(str));
        break;
      }
      case "info": {
        console.log(chalk.white(str));
        break;
      }
      case "debug": {
        console.log(chalk.cyan(str));
        break;
      }
    }
  }
}

let processBrowser: Deno.ChildProcess | null = null;
let intendedShutdown = false;

export async function runBrowser(port = 5180) {
  // https://wiki.mozilla.org/Firefox/CommandLineOptions
  let command: Deno.Command;

  switch (Deno.build.os) {
    case "windows":
      command = new Deno.Command(
        `./_dist/bin/${brandingBaseName}/${brandingBaseName}.exe`,
        {
          args: [
            "--profile",
            "./_dist/profile/test",
            "--remote-debugging-port",
            port.toString(),
            "--wait-for-browser",
            "--jsdebugger",
          ],
          stdout: "piped",
          stderr: "piped",
        },
      );
      break;

    case "linux":
      command = new Deno.Command(
        `./_dist/bin/${brandingBaseName}/${brandingBaseName}`,
        {
          args: [
            "--profile",
            "./_dist/profile/test",
            "--remote-debugging-port",
            port.toString(),
            "--wait-for-browser",
            "--jsdebugger",
          ],
          stdout: "piped",
          stderr: "piped",
        },
      );
      break;

    case "darwin":
      command = new Deno.Command(
        `./_dist/bin/${brandingBaseName}/${brandingName}.app/Contents/MacOS/${brandingBaseName}`,
        {
          args: [
            "--profile",
            "./_dist/profile/test",
            "--remote-debugging-port",
            port.toString(),
            "--wait-for-browser",
            "--jsdebugger",
          ],
          stdout: "piped",
          stderr: "piped",
        },
      );
      break;
    default:
      throw new Error(`Unsupported platform: ${Deno.build.os}`);
  }

  processBrowser = command.spawn();

  // Process stdout
  (async () => {
    if (processBrowser?.stdout) {
      const reader = processBrowser.stdout.getReader();
      const decoder = new TextDecoder();

      try {
        while (true) {
          const { done, value } = await reader.read();
          if (done) break;

          const str = decoder.decode(value);
          printFirefoxLog(str.split("\n"));
        }
      } catch (error) {
        console.error("Error reading stdout:", error);
      } finally {
        reader.releaseLock();
      }
    }
  })();

  // Process stderr
  (async () => {
    if (processBrowser?.stderr) {
      const reader = processBrowser.stderr.getReader();
      const decoder = new TextDecoder();

      try {
        while (true) {
          const { done, value } = await reader.read();
          if (done) break;

          const str = decoder.decode(value);
          const WEBDRIVER_BIDI_WEBSOCKET_ENDPOINT_REGEX =
            /^WebDriver BiDi listening on (ws:\/\/.*)/;
          if (
            WEBDRIVER_BIDI_WEBSOCKET_ENDPOINT_REGEX.test(str.replace("\n", ""))
          ) {
            console.log(
              "nora-{bbd11c51-3be9-4676-b912-ca4c0bdcab94}-webdriver",
            );
          }

          printFirefoxLog(str.split("\n"));
        }
      } catch (error) {
        console.error("Error reading stderr:", error);
      } finally {
        reader.releaseLock();
      }
    }
  })();

  await processBrowser.status;

  /**
   * Kill nodejs process gratefully
   */
  if (!intendedShutdown) {
    console.log("[child-browser] Browser Closed");
    Deno.exit(0);
  }
}

{
  //* main
  const decoder = new TextDecoder();
  (async () => {
    for await (const chunk of Deno.stdin.readable) {
      const text = decoder.decode(chunk);
      if (text.startsWith("q")) {
        console.log("[child-browser] Shutdown Browser");
        intendedShutdown = true;
        if (processBrowser != null) {
          try {
            (processBrowser as Deno.ChildProcess).kill("SIGTERM");
          } catch (error) {
            console.error("Error killing browser process:", error);
          }
        }
        Deno.exit(0);
      }
    }
  })();

  await runBrowser();
}
