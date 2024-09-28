import { execa } from "execa";
import chalk from "chalk";

export async function runBrowser(port = 5180) {
  const processBrowser = execa(
    {},
  )`./_dist/bin/noraneko.exe --profile ./_dist/profile/test --remote-debugging-port ${port}`;

  const stdout = processBrowser.readable({ from: "stdout" });
  const stderr = processBrowser.readable({ from: "stderr" });

  let logStatusForFollowingLine: "error" | "warn" | "info" | "debug" = "info";
  /**
   *
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
          console.error(chalk.hex("#E67373")(str));
          break;
        }
        case "warn": {
          console.error(chalk.hex("#D1D13F")(str));
          break;
        }
        case "info": {
          console.error(chalk.white(str));
          break;
        }
        case "debug": {
          console.error(chalk.cyan(str));
          break;
        }
      }
    }
  }

  /**
   * In sub-thread, Firefox seems to use stdout
   */
  stdout.on("readable", () => {
    const temp = stdout.read();
    // if the buffer is null, the process is on exit.
    if (!temp) {
      return;
    }
    const str = temp.toString() as string;
    printFirefoxLog(str.split("\n"));
  });

  let resolve: () => void;
  const ret = new Promise<void>((_resolve, _reject) => {
    resolve = _resolve;
  });

  /**
   * Firefox's log seems to be outputted in stderr mainly.
   */
  stderr.on("readable", () => {
    const temp = stderr.read();
    // if the buffer is null, the process is on exit.
    if (!temp) {
      return;
    }
    const str = temp.toString() as string;

    const WEBDRIVER_BIDI_WEBSOCKET_ENDPOINT_REGEX =
      /^WebDriver BiDi listening on (ws:\/\/.*)/;
    if (WEBDRIVER_BIDI_WEBSOCKET_ENDPOINT_REGEX.test(str.replace("\n", ""))) {
      resolve();
    }

    printFirefoxLog(str.split("\n"));
  });
  return ret;
}
