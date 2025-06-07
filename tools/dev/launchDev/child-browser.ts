import { $, ProcessPromise } from "zx";
import chalk from "chalk";
import { usePwsh } from "zx";

switch (process.platform) {
  case "win32":
    usePwsh();
}

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

import { log, type LogEntry } from "zx/core";
import { brandingBaseName, brandingName } from "../../build/defines.ts";

$.log = (entry: LogEntry) => {
  // if the buffer is null, the process is on exit.
  switch (entry.kind) {
    case "stdout": {
      /**
       * In sub-thread, Firefox seems to use stdout
       */
      const str = entry.data.toString();
      printFirefoxLog(str.split("\n"));
      break;
    }
    case "stderr": {
      /**
       * Firefox's log seems to be outputted in stderr mainly.
       */
      const str = entry.data.toString();
      const WEBDRIVER_BIDI_WEBSOCKET_ENDPOINT_REGEX =
        /^WebDriver BiDi listening on (ws:\/\/.*)/;
      if (WEBDRIVER_BIDI_WEBSOCKET_ENDPOINT_REGEX.test(str.replace("\n", ""))) {
        console.log("nora-{bbd11c51-3be9-4676-b912-ca4c0bdcab94}-webdriver");
      }

      printFirefoxLog(str.split("\n"));
      break;
    }
    default:
      log(entry);
  }
};

let processBrowser: ProcessPromise | null = null;
let intendedShutdown = false;

export async function runBrowser(port = 5180) {
  // https://wiki.mozilla.org/Firefox/CommandLineOptions
  switch (Deno.build.os) {
    case "windows":
      processBrowser =
        $`./_dist/bin/${brandingBaseName}/${brandingBaseName}.exe --profile ./_dist/profile/test --remote-debugging-port ${port} --wait-for-browser --jsdebugger`
          .stdio("pipe");
      break;

    case "linux":
      processBrowser =
        $`./_dist/bin/${brandingBaseName}/${brandingBaseName} --profile ./_dist/profile/test --remote-debugging-port ${port} --wait-for-browser --jsdebugger`
          .stdio("pipe");
      break;

    case "darwin":
      processBrowser =
        $`./_dist/bin/${brandingBaseName}/${brandingName}.app/Contents/MacOS/${brandingBaseName} --profile ./_dist/profile/test --remote-debugging-port ${port} --wait-for-browser --jsdebugger`
          .stdio("pipe");
      break;
  }

  //processBrowser = $`${binPath()} --profile ./_dist/profile/test --remote-debugging-port ${port} --wait-for-browser --jsdebugger`;

  // processBrowser!.stdout.on("readable",()=>{
  //   const temp = processBrowser?.stdout.read()
  //   // if the buffer is null, the process is on exit.
  //   if (!temp) {
  //     return;
  //   }
  //   /**
  //      * In sub-thread, Firefox seems to use stdout
  //      */
  //   const str = temp.toString();
  //   printFirefoxLog(str.split("\n"));
  // });
  // processBrowser!.stderr.on("readable",()=>{
  //   const temp = processBrowser?.stderr.read()
  //   // if the buffer is null, the process is on exit.
  //   if (!temp) {
  //     return;
  //   }
  //   /**
  //    * Firefox's log seems to be outputted in stderr mainly.
  //    */
  //   const str = temp.toString();
  //   const WEBDRIVER_BIDI_WEBSOCKET_ENDPOINT_REGEX =
  //   /^WebDriver BiDi listening on (ws:\/\/.*)/;
  //   if (WEBDRIVER_BIDI_WEBSOCKET_ENDPOINT_REGEX.test(str.replace("\n", ""))) {
  //     console.log("nora-{bbd11c51-3be9-4676-b912-ca4c0bdcab94}-webdriver")
  //   }

  //   printFirefoxLog(str.split("\n"));
  // });

  await processBrowser;
  /**
   * Kill nodejs process gratefully
   */
  if (!intendedShutdown) {
    console.log("[child-browser] Browser Closed");
    Deno.exit(0);
  }
}

{ //* main
  const decoder = new TextDecoder();
  (async () => {
    for await (const chunk of Deno.stdin.readable) {
      const text = decoder.decode(chunk);
      if (text.startsWith("q")) {
        console.log("[child-browser] Shutdown Browser");
        intendedShutdown = true;
        if (processBrowser != null) {
          await processBrowser.kill();
        }
        Deno.exit(0);
      }
    }
  })();

  await runBrowser();
}
