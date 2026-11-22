// SPDX-License-Identifier: MPL-2.0
import { assertEquals, assertFalse, assertThrows } from "@std/assert";
import { parseViteBuildArgs, runViteBuildCommand } from "./vite_build.ts";
Deno.test("Vite build defaults explicitly disable watch", () => {
  assertEquals(parseViteBuildArgs([]), {
    kind: "build",
    config: { build: { watch: null } },
  });
});
Deno.test("Vite build preserves base and config options", () => {
  assertEquals(
    parseViteBuildArgs([
      "--base",
      "chrome://noraneko/content",
      "--config",
      "vite.config.ts",
    ]),
    {
      kind: "build",
      config: {
        base: "chrome://noraneko/content",
        configFile: "vite.config.ts",
        build: { watch: null },
      },
    },
  );
});
Deno.test("Vite build help is explicit", () => {
  assertEquals(parseViteBuildArgs(["--help"]), { kind: "help" });
});
for (
  const args of [
    ["--watch"],
    ["--base"],
    ["--config", ""],
    ["--base", "--config"],
    ["--base", "/a", "--base", "/b"],
    ["--config", "a", "--config", "b"],
    ["--help", "--base", "/"],
  ]
) {
  Deno.test("Vite build rejects " + JSON.stringify(args), () => {
    assertThrows(() => parseViteBuildArgs(args));
  });
}
Deno.test("Vite build invalid arguments never load compiler", async () => {
  let loads = 0;
  const errors: unknown[] = [];
  const status = await runViteBuildCommand(["--watch"], {
    loadBuild: () => {
      loads++;
      return Promise.resolve(() => Promise.resolve());
    },
    info: () => {},
    error: (e) => errors.push(e),
  });
  assertEquals(status, 1);
  assertEquals(loads, 0);
  assertEquals(errors.length, 1);
});
Deno.test("Vite build help never loads compiler", async () => {
  let loads = 0;
  let help = "";
  assertEquals(
    await runViteBuildCommand(["--help"], {
      loadBuild: () => {
        loads++;
        return Promise.resolve(() => Promise.resolve());
      },
      info: (s) => {
        help = s;
      },
      error: () => {
        throw Error("Unexpected error");
      },
    }),
    0,
  );
  assertEquals(loads, 0);
  assertEquals(help.includes("--config"), true);
});
Deno.test("Vite build cannot succeed before actual build promise settles", async () => {
  let complete: () => void = () => {
    throw Error("Build not invoked");
  };
  const pending = new Promise<void>((resolve) => {
    complete = resolve;
  });
  let returned = false;
  const work = runViteBuildCommand([], {
    loadBuild: () => Promise.resolve(() => pending),
    info: () => {},
    error: () => {},
  }).then((code) => {
    returned = true;
    return code;
  });
  await Promise.resolve();
  await Promise.resolve();
  assertFalse(returned);
  complete();
  assertEquals(await work, 0);
});
Deno.test("Vite build rejection remains nonzero with original failure", async () => {
  const failure = Error("synthetic compile error");
  const errors: unknown[] = [];
  assertEquals(
    await runViteBuildCommand([], {
      loadBuild: () => Promise.resolve(() => Promise.reject(failure)),
      info: () => {},
      error: (e) => errors.push(e),
    }),
    1,
  );
  assertEquals(errors, [failure]);
});
Deno.test("Vite compiler import failure remains nonzero", async () => {
  const failure = Error("synthetic import failure");
  const errors: unknown[] = [];
  assertEquals(
    await runViteBuildCommand([], {
      loadBuild: () => Promise.reject(failure),
      info: () => {},
      error: (e) => errors.push(e),
    }),
    1,
  );
  assertEquals(errors, [failure]);
});
Deno.test("Vite API receives non-watch config and exact caller options", async () => {
  const calls: unknown[] = [];
  assertEquals(
    await runViteBuildCommand([
      "--config",
      "vite.config.ts",
      "--base",
      "chrome://noraneko/content",
    ], {
      loadBuild: () =>
        Promise.resolve((config) => {
          calls.push(config);
          return Promise.resolve();
        }),
      info: () => {},
      error: () => {},
    }),
    0,
  );
  assertEquals(calls, [{
    base: "chrome://noraneko/content",
    configFile: "vite.config.ts",
    build: { watch: null },
  }]);
});
Deno.test("Builder routes build-only Vite commands through the terminating API entry", async () => {
  const builder = await Deno.readTextFile(
    new URL("./builder.ts", import.meta.url),
  );
  assertEquals(builder.includes('"vite",\n        "build",'), false);
  assertEquals(
    builder.split('path.join(PROJECT_ROOT, "tools/vite-build.ts")').length - 1,
    9,
  );
  const entry = await Deno.readTextFile(
    new URL("../vite-build.ts", import.meta.url),
  );
  assertEquals(entry.includes("Deno.exit(code)"), true);
  assertEquals(
    entry.indexOf('await import("vite")') > entry.indexOf("loadBuild:"),
    true,
  );
});
