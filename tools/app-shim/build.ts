// SPDX-License-Identifier: MPL-2.0

import { parseArgs } from "@std/cli/parse-args";
import { dirname, fromFileUrl, join, resolve } from "@std/path";

const root = resolve(dirname(fromFileUrl(import.meta.url)), "../..");

export async function run(command: string, args: string[]): Promise<void> {
  const child = new Deno.Command(command, {
    args,
    cwd: root,
    stdout: "inherit",
    stderr: "inherit",
  }).spawn();
  const status = await child.status;
  if (!status.success) {
    throw new Error(`${command} failed with exit status ${status.code}`);
  }
}

async function main(): Promise<void> {
  const args = parseArgs(Deno.args, {
    boolean: ["help", "test", "universal"],
    string: ["out"],
    unknown(argument) {
      throw new Error(`Unknown argument: ${argument}`);
    },
  });
  if (args.help) {
    console.log(
      "Build the experimental macOS App Shim core.\n" +
        "Usage: deno task app-shim:build [--test] [--universal] [--out path]\n" +
        "This does not install a Web App or enable the browser integration.",
    );
    return;
  }
  if (Deno.build.os !== "darwin") {
    throw new Error("The native App Shim requires the macOS SDK and AppKit.");
  }
  const output = resolve(root, args.out ?? "_dist/app-shim-native");
  await Deno.mkdir(output, { recursive: true });
  const configure = [
    "-S",
    join(root, "native/macos-app-shim"),
    "-B",
    output,
    "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
    `-DBUILD_TESTING=${args.test ? "ON" : "OFF"}`,
  ];
  if (args.universal) {
    configure.push("-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64");
  }
  await run("cmake", configure);
  await run("cmake", ["--build", output, "--parallel", "4"]);
  if (args.test) {
    await run("ctest", [
      "--test-dir",
      output,
      "--output-on-failure",
      "--timeout",
      "30",
    ]);
  }
}

if (import.meta.main) {
  await main();
}
