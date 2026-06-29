// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals } from "@std/assert";
import {
  extractBridgeLoader,
  extractChromeFeatureDiscovery,
  extractOsApiRouteEntries,
  extractSettingsRoutes,
  extractSharedOsApiRouteEntries,
  extractWindowActors,
  parseDenoTasksFromText,
  parseFelesCommandsFromText,
  parseWorkflowRunCommands,
} from "./collector.ts";
import {
  codexAuditSchema,
  seedCodexDocs,
  verifyCodexAuditResult,
} from "./codex.ts";
import {
  escapeMdxText,
  generateDocsPayload,
  LLM_AUTHORED_PAGE_PATHS,
  normalizeDuplicateSourceExtensions,
  normalizeGeneratedBody,
  normalizeSourceMarkdownLinks,
  readLlmConfig,
  writeGeneratedDocs,
} from "./generator.ts";
import {
  readAuditLlmConfig,
  runLlmAudit,
  verifyLlmAuditResult,
} from "./llm_audit.ts";
import { verifyDocsPipeline } from "./verifier.ts";
import { REQUIRED_GENERATED_PAGE_PATHS } from "./types.ts";
import type { DocsInventory, GeneratedPage } from "./types.ts";

Deno.test("parseDenoTasksFromText reads sorted task entries", () => {
  const tasks = parseDenoTasksFromText(
    JSON.stringify({
      tasks: {
        "test:smoke": "deno run -A tools/src/smoke_runner.ts",
        "feles-build": "deno run -A tools/feles-build.ts",
      },
    }),
  );

  assertEquals(tasks.map((task) => task.name), ["feles-build", "test:smoke"]);
  assertEquals(tasks[0].command, "deno run -A tools/feles-build.ts");
});

Deno.test("parseFelesCommandsFromText reads command switch cases", () => {
  const commands = parseFelesCommandsFromText(`
    switch (command) {
      case "dev":
        console.log("Usage: feles-build dev");
        break;
      case "build":
        console.log("Usage: feles-build build --phase <before-mach|after-mach>");
        break;
      case "--help":
        break;
    }
  `);

  assertEquals(commands.map((command) => command.name), ["build", "dev"]);
  assertEquals(
    commands[0].usage,
    "Usage: feles-build build --phase <before-mach|after-mach>",
  );
});

Deno.test("extractChromeFeatureDiscovery reads import.meta.glob pattern", () => {
  const discovery = extractChromeFeatureDiscovery(`
    export function getFeaturesCommonEntries() {
      return import.meta.glob("./*/index.ts");
    }
  `);

  assertEquals(discovery.globPattern, "./*/index.ts");
});

Deno.test("extractSettingsRoutes normalizes displayed route paths", () => {
  const routes = extractSettingsRoutes(`
    import MouseGesture from "@/app/gesture/page.tsx";
    <Route path="features/gesture" element={<MouseGesture />} />
  `);

  assertEquals(routes[0].route, "/features/gesture");
  assertEquals(
    routes[0].source.path,
    "browser-features/pages-settings/src/App.tsx",
  );
});

Deno.test("extractBridgeLoader reads loader URLs from chrome root source", () => {
  const loader = extractBridgeLoader(`
    const dev = "http://localhost:5181/loader/index.ts";
    const test = "http://localhost:5181/loader/test/index.ts";
    const prod = "chrome://noraneko/content/core.js";
  `);

  assertEquals(loader.devLoaderUrl, "http://localhost:5181/loader/index.ts");
  assertEquals(
    loader.testLoaderUrl,
    "http://localhost:5181/loader/test/index.ts",
  );
  assertEquals(loader.productionLoader, "chrome://noraneko/content/core.js");
});

Deno.test("extractWindowActors reads addJSWindowActors and actor count", () => {
  const actors = extractWindowActors(`
    const JS_WINDOW_ACTORS: {
      [k: string]: WindowActorOptions;
    } = {
      NRSettings: {
        parent: {},
      },
      NRPanelSidebar: {
        child: {},
      },
    };
    ActorManagerParent.addJSWindowActors(JS_WINDOW_ACTORS);
  `);

  assertEquals(actors.registrationApi, "ActorManagerParent.addJSWindowActors");
  assertEquals(actors.actorCount, 2);
});

Deno.test("extractOsApiRouteEntries reads namespaced local HTTP routes", () => {
  const routes = extractOsApiRouteEntries(
    `
    export function registerTabRoutes(api: NamespaceBuilder): void {
      api.namespace("/tabs", (t: NamespaceBuilder) => {
        t.get("/list", async () => ({ status: 200, body: [] }));
        t.post<{ url: string }, { instanceId: string }>(
          "/instances",
          async () => ({ status: 200, body: { instanceId: "id" } }),
        );
        t.delete("/instances/:id", async () => ({
          status: 200,
          body: { ok: true },
        }));
      });
    }
  `,
    "browser-features/modules/modules/os-server/tabs/routes.sys.mts",
    "/tabs",
  );

  assertEquals(
    routes.map((route) => `${route.method} ${route.path}`),
    [
      "POST /tabs/instances",
      "DELETE /tabs/instances/:id",
      "GET /tabs/list",
    ],
  );
  assertEquals(
    routes[0].source.path,
    "browser-features/modules/modules/os-server/tabs/routes.sys.mts",
  );
});

Deno.test("extractSharedOsApiRouteEntries expands conditional shared routes", () => {
  const text = `
    export function registerCommonAutomationRoutes(
      ns: NamespaceBuilder,
      options: { includeGetElement?: boolean } = {},
    ): void {
      ns.get("/instances/:id/text", async () => ({ status: 200, body: {} }));
      if (options.includeGetElement) {
        ns.get<unknown, ElementResponse>(
          "/instances/:id/element",
          async () => ({ status: 200, body: {} }),
        );
      }
    }
  `;

  const tabsRoutes = extractSharedOsApiRouteEntries(
    text,
    "browser-features/modules/modules/os-server/shared/routes.sys.mts",
    "/tabs",
    { includeGetElement: true },
  );
  const scraperRoutes = extractSharedOsApiRouteEntries(
    text,
    "browser-features/modules/modules/os-server/shared/routes.sys.mts",
    "/scraper",
    { includeGetElement: false },
  );

  assert(
    tabsRoutes.some((route) => route.path === "/tabs/instances/:id/element"),
  );
  assert(
    !scraperRoutes.some((route) =>
      route.path === "/scraper/instances/:id/element"
    ),
  );
  assert(
    scraperRoutes.some((route) => route.path === "/scraper/instances/:id/text"),
  );
});

Deno.test("parseWorkflowRunCommands reads inline and block run commands", () => {
  const commands = parseWorkflowRunCommands(`
    steps:
      - run: deno task test:smoke
      - name: Start browser and run integration tests
        run: |
          set -euo pipefail
          mkdir -p _dist
          deno install
          deno task feles-build test > _dist/ci.log 2>&1 &
          deno task docs-pipeline generate \\
            --inventory _dist/docs-pipeline/inventory.json \\
            --out _dist/docs-pipeline/generated
          test -n "\${DOCS_PUBLISH_TOKEN}"
          for i in {1..300}; do
            sleep 2
          done
          deno task test --no-autostart
      - run: deno test -A tools/src/colocated_test_runner.test.ts
  `);

  assertEquals(commands, [
    "deno install",
    "deno task docs-pipeline generate --inventory _dist/docs-pipeline/inventory.json --out _dist/docs-pipeline/generated",
    "deno task feles-build test > _dist/ci.log 2>&1 &",
    "deno task test --no-autostart",
    "deno task test:smoke",
    "deno test -A tools/src/colocated_test_runner.test.ts",
    "mkdir -p _dist",
    "sleep 2",
  ]);
});

Deno.test("readLlmConfig omits empty API keys for local compatible endpoints", () => {
  const config = readLlmConfig({
    DOCS_LLM_BASE_URL: "http://localhost:11434/v1",
    DOCS_LLM_MODEL: "llama3.1",
    DOCS_LLM_API_KEY: "",
  });

  assertEquals(config.apiKey, undefined);
  assertEquals(config.temperature, 0);
});

Deno.test("readLlmConfig rejects invalid temperature", () => {
  let rejected = false;
  try {
    readLlmConfig({
      DOCS_LLM_BASE_URL: "http://localhost:11434/v1",
      DOCS_LLM_MODEL: "llama3.1",
      DOCS_LLM_TEMPERATURE: "not-a-number",
    });
  } catch {
    rejected = true;
  }

  assert(rejected, "invalid generation temperature should be rejected");
});

Deno.test("readAuditLlmConfig falls back to generation config", () => {
  const config = readAuditLlmConfig({
    DOCS_LLM_BASE_URL: "https://ollama.com/v1",
    DOCS_LLM_MODEL: "glm-5.2",
    DOCS_LLM_API_KEY: "docs-key",
    DOCS_AUDIT_LLM_BASE_URL: "",
    DOCS_AUDIT_LLM_MODEL: "kimi-k2.7-code",
  });

  assertEquals(config.baseUrl, "https://ollama.com/v1");
  assertEquals(config.model, "kimi-k2.7-code");
  assertEquals(config.apiKey, "docs-key");
});

Deno.test("readAuditLlmConfig rejects invalid audit temperature", () => {
  let rejected = false;
  try {
    readAuditLlmConfig({
      DOCS_LLM_BASE_URL: "https://ollama.com/v1",
      DOCS_LLM_MODEL: "glm-5.2",
      DOCS_AUDIT_LLM_TEMPERATURE: "warm",
    });
  } catch {
    rejected = true;
  }

  assert(rejected, "invalid audit temperature should be rejected");
});

Deno.test("normalizeGeneratedBody converts escaped newlines to real newlines", () => {
  assertEquals(
    normalizeGeneratedBody("# Title\\n\\nBody\\ttext"),
    "# Title\n\nBody  text",
  );
});

Deno.test("escapeMdxText escapes placeholders outside fenced code", () => {
  assertEquals(
    escapeMdxText([
      "> quoted source note",
      "| Command |",
      "| `feles-build build --phase <before-mach|after-mach>` |",
      "Literal expression {danger}",
      "```bash",
      "feles-build build --phase <before-mach|after-mach>",
      "echo {safe in fence}",
      "```",
    ].join("\n")),
    [
      "> quoted source note",
      "| Command |",
      "| `feles-build build --phase &lt;before-mach|after-mach&gt;` |",
      "Literal expression &#123;danger&#125;",
      "```bash",
      "feles-build build --phase <before-mach|after-mach>",
      "echo {safe in fence}",
      "```",
    ].join("\n"),
  );
});

Deno.test("normalizeSourceMarkdownLinks converts source links to code citations", () => {
  const text = [
    "Use [deno.json](deno.json) and [feles-build](tools/feles-build.ts).",
    "Avoid [generated versions](static/gecko/config/version.txt) and `static/gecko/config/autogenerated/`.",
    "Fix collector drift from `tools/src/colocated_test_collector.ts` and workflow directory drift from `.github/workflows/mozconfigs/`.",
    "Fix Firefox ESM drift from `browser-features/modules/modules/os-server/server.mts`.",
    "Keep [external](https://example.com) links unchanged.",
  ].join("\n");

  assertEquals(
    normalizeSourceMarkdownLinks(text, sampleInventory()),
    [
      "Use `deno.json` and feles-build (`tools/feles-build.ts`).",
      "Avoid generated versions (`static/gecko/config/README.md`) and `static/gecko/config/README.md`.",
      "Fix collector drift from `tools/src/browser_test_collector.ts` and workflow directory drift from `.github/workflows/package.yml`.",
      "Fix Firefox ESM drift from `browser-features/modules/modules/os-server/server.sys.mts`.",
      "Keep [external](https://example.com) links unchanged.",
    ].join("\n"),
  );
});

Deno.test("normalizeSourceMarkdownLinks ignores links inside fenced code", () => {
  const text = [
    "```md",
    "[deno.json](deno.json)",
    "```",
  ].join("\n");

  assertEquals(
    normalizeSourceMarkdownLinks(text, sampleInventory()),
    text,
  );
});

Deno.test("normalizeDuplicateSourceExtensions fixes repeated TypeScript extensions", () => {
  assertEquals(
    normalizeDuplicateSourceExtensions(
      "See `browser-features/chrome/static/overrides/index.ts.ts` and `browser-features/pages-settings/src/App.tsx.tsx`.",
    ),
    "See `browser-features/chrome/static/overrides/index.ts` and `browser-features/pages-settings/src/App.tsx`.",
  );
});

Deno.test("generateDocsPayload uses OpenAI-compatible chat completions", async () => {
  let sawAuthorizationHeader = false;
  let requestCount = 0;
  const server = Deno.serve(
    { hostname: "127.0.0.1", port: 0 },
    async (request) => {
      requestCount++;
      sawAuthorizationHeader = request.headers.has("Authorization");
      const body = await request.json() as {
        model?: string;
        messages?: Array<{ role: string; content: string }>;
        response_format?: unknown;
      };

      assertEquals(body.model, "mock-model");
      assert(Array.isArray(body.messages), "messages must be an array");
      assertEquals(body.response_format, undefined);

      return Response.json({
        choices: [
          {
            message: {
              content:
                "# Page\n\nChrome features are discovered from `browser-features/chrome/common/mod.ts`.",
            },
          },
        ],
      });
    },
  );

  try {
    const payload = await generateDocsPayload(sampleInventory(), {
      baseUrl: `http://127.0.0.1:${server.addr.port}/v1`,
      model: "mock-model",
      temperature: 0,
      useJsonResponseFormat: true,
    });

    assertEquals(sawAuthorizationHeader, false);
    assertEquals(requestCount, LLM_AUTHORED_PAGE_PATHS.length);
    assertEquals(payload.pages.length, REQUIRED_GENERATED_PAGE_PATHS.length);
    assertEquals(
      payload.pages[0].path,
      "development/directories/bridge.mdx",
    );
    assert(
      payload.pages[0].body.includes(
        "browser-features/chrome/common/mod.ts",
      ),
    );
  } finally {
    await server.shutdown();
  }
});

Deno.test("generateDocsPayload retries missing page content", async () => {
  let requestCount = 0;
  const server = Deno.serve(
    { hostname: "127.0.0.1", port: 0 },
    () => {
      requestCount++;
      if (requestCount === 1) {
        return Response.json({
          choices: [{ message: {} }],
        });
      }

      return Response.json({
        choices: [
          {
            message: {
              content:
                "Chrome features are discovered from `browser-features/chrome/common/mod.ts`.",
            },
          },
        ],
      });
    },
  );

  try {
    const payload = await generateDocsPayload(sampleInventory(), {
      baseUrl: `http://127.0.0.1:${server.addr.port}/v1`,
      model: "mock-model",
      temperature: 0,
      useJsonResponseFormat: true,
    });

    assertEquals(payload.pages.length, REQUIRED_GENERATED_PAGE_PATHS.length);
    assertEquals(requestCount, LLM_AUTHORED_PAGE_PATHS.length + 1);
  } finally {
    await server.shutdown();
  }
});

Deno.test("generateDocsPayload escapes raw MDX body from model", async () => {
  const server = Deno.serve(
    { hostname: "127.0.0.1", port: 0 },
    () =>
      Response.json({
        choices: [
          {
            message: {
              content:
                "Use `browser-features/chrome/common/mod.ts` with <unsafe> and {expr}.",
            },
          },
        ],
      }),
  );

  try {
    const payload = await generateDocsPayload(sampleInventory(), {
      baseUrl: `http://127.0.0.1:${server.addr.port}/v1`,
      model: "mock-model",
      temperature: 0,
      useJsonResponseFormat: true,
    });

    assert(payload.pages[0].body.includes("&lt;unsafe&gt;"));
    assert(payload.pages[0].body.includes("&#123;expr&#125;"));
  } finally {
    await server.shutdown();
  }
});

Deno.test("runLlmAudit uses OpenAI-compatible chat completions", async () => {
  const dir = await Deno.makeTempDir();
  let sawAuthorizationHeader = false;
  const server = Deno.serve(
    { hostname: "127.0.0.1", port: 0 },
    async (request) => {
      sawAuthorizationHeader = request.headers.has("Authorization");
      const body = await request.json() as {
        model?: string;
        messages?: unknown;
        response_format?: unknown;
      };

      assertEquals(body.model, "audit-model");
      assert(Array.isArray(body.messages), "messages must be an array");
      assert(
        JSON.stringify(body.messages).includes(
          "MCP servers, and other automation clients",
        ),
      );
      assertEquals(body.response_format, { type: "json_object" });

      return Response.json({
        choices: [
          {
            message: {
              content: JSON.stringify({
                pass: true,
                blocking_findings: [],
                warnings: ["Architecture page can be expanded later."],
                recommendation: "Publish with review.",
              }),
            },
          },
        ],
      });
    },
  );

  try {
    await writeSampleGeneratedPages(dir);
    const audit = await runLlmAudit(sampleInventory(), dir, {
      baseUrl: `http://127.0.0.1:${server.addr.port}/v1`,
      model: "audit-model",
      temperature: 0,
      useJsonResponseFormat: true,
    });

    assertEquals(sawAuthorizationHeader, false);
    assertEquals(audit.pass, true);
  } finally {
    await server.shutdown();
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("verifyLlmAuditResult rejects blocking audits", () => {
  for (
    const value of [
      { pass: "true", blocking_findings: [], warnings: [], recommendation: "" },
      {
        pass: false,
        blocking_findings: ["Missing source citation"],
        warnings: [],
        recommendation: "Do not publish.",
      },
      {
        pass: true,
        blocking_findings: ["Invented command"],
        warnings: [],
        recommendation: "Do not publish.",
      },
    ]
  ) {
    let rejected = false;
    try {
      verifyLlmAuditResult(value);
    } catch {
      rejected = true;
    }
    assert(rejected, "invalid LLM audit should be rejected");
  }
});

function sampleInventory(): DocsInventory {
  return {
    schemaVersion: 1,
    generatedAt: "2026-06-26T00:00:00.000Z",
    floorpCommit: "abc",
    sourcePrecedence: [
      "code/config/CI/test scripts",
      "repo docs",
      "external docs",
    ],
    commands: {
      denoTasks: [
        {
          name: "feles-build",
          command: "deno run -A tools/feles-build.ts",
          source: { path: "deno.json" },
        },
        {
          name: "dev-tool",
          command: "deno run -A tools/dev-tool.ts",
          source: { path: "deno.json" },
        },
        {
          name: "docs-pipeline",
          command:
            "deno run --allow-read --allow-write --allow-env --allow-net --allow-run tools/docs-pipeline/mod.ts",
          source: { path: "deno.json" },
        },
        {
          name: "docs-pipeline:collect",
          command:
            "deno run --allow-read --allow-write=_dist --allow-run=git tools/docs-pipeline/mod.ts collect",
          source: { path: "deno.json" },
        },
        {
          name: "docs-pipeline:generate",
          command:
            "deno run --allow-read --allow-write=_dist,docs --allow-env=DOCS_LLM_BASE_URL,DOCS_LLM_MODEL,DOCS_LLM_API_KEY,DOCS_LLM_TEMPERATURE,DOCS_LLM_RESPONSE_FORMAT --allow-net tools/docs-pipeline/mod.ts generate",
          source: { path: "deno.json" },
        },
        {
          name: "docs-pipeline:verify",
          command:
            "deno run --allow-read --allow-write=_dist tools/docs-pipeline/mod.ts verify",
          source: { path: "deno.json" },
        },
        {
          name: "docs-pipeline:audit",
          command:
            "deno run --allow-read --allow-write=_dist --allow-env=DOCS_LLM_BASE_URL,DOCS_LLM_MODEL,DOCS_LLM_API_KEY,DOCS_LLM_TEMPERATURE,DOCS_LLM_RESPONSE_FORMAT,DOCS_AUDIT_LLM_BASE_URL,DOCS_AUDIT_LLM_MODEL,DOCS_AUDIT_LLM_API_KEY,DOCS_AUDIT_LLM_TEMPERATURE,DOCS_AUDIT_LLM_RESPONSE_FORMAT --allow-net tools/docs-pipeline/mod.ts audit",
          source: { path: "deno.json" },
        },
        {
          name: "test:docs-pipeline",
          command:
            "deno test --allow-read --allow-write --allow-net=127.0.0.1 tools/docs-pipeline/",
          source: { path: "deno.json" },
        },
      ],
      felesBuild: ["dev", "test", "stage", "build", "misc"].map((name) => ({
        name,
        source: { path: "tools/feles-build.ts" },
      })),
    },
    architecture: {
      layers: [],
      referenceSources: [
        {
          area: "Loader entrypoint",
          source: { path: "bridge/loader-features/loader/index.ts" },
          summary: "Loader entrypoint.",
        },
        {
          area: "Test runner",
          source: { path: "tools/src/colocated_test_runner.ts" },
          summary: "Browser test runner.",
        },
        {
          area: "Browser test result collector",
          source: { path: "tools/src/browser_test_collector.ts" },
          summary: "Browser test result collector.",
        },
        {
          area: "Gecko version configuration",
          source: { path: "static/gecko/config/README.md" },
          summary: "Documents generated Gecko version files.",
        },
      ],
      chromeFeatureDiscovery: {
        globPattern: "./*/index.ts",
        source: { path: "browser-features/chrome/common/mod.ts" },
      },
      windowActors: {
        registrationApi: "ActorManagerParent.addJSWindowActors",
        actorCount: 2,
        source: {
          path: "browser-features/modules/modules/BrowserGlue.sys.mts",
        },
      },
      bridgeLoader: {
        devLoaderUrl: "http://localhost:5181/loader/index.ts",
        testLoaderUrl: "http://localhost:5181/loader/test/index.ts",
        productionLoader: "chrome://noraneko/content/core.js",
        source: { path: "bridge/startup/src/chrome_root.ts" },
      },
      loaderDevServer: {
        port: 5181,
        source: { path: "bridge/loader-features/vite.config.ts" },
      },
    },
    ci: {
      workflows: [
        {
          name: "(A) Package",
          path: ".github/workflows/package.yml",
          triggers: ["workflow_call"],
          permissions: ["contents:read"],
          runCommands: [],
        },
      ],
    },
    features: {
      chromeCommon: [
        {
          name: "workspaces",
          source: {
            path: "browser-features/chrome/common/workspaces/index.ts",
          },
          summary: "Initializes the Workspaces chrome feature.",
          entrypoints: ["default class Workspaces", "init"],
        },
        {
          name: "statusbar",
          source: {
            path: "browser-features/chrome/common/statusbar/index.ts",
          },
          summary: "Initializes the Status Bar chrome feature.",
          entrypoints: ["default class StatusBar", "init"],
        },
      ],
      chromeStatic: [
        {
          name: "prefs",
          source: { path: "browser-features/chrome/static/prefs/index.ts" },
          summary: "Provides pre-session preference initialization.",
          entrypoints: ["initBeforeSessionStoreInit", "init"],
        },
      ],
      settingsRoutes: [
        {
          route: "/overview/home",
          component: "@/app/dashboard/page.tsx",
          source: { path: "browser-features/pages-settings/src/App.tsx" },
        },
      ],
      windowActors: [
        {
          name: "NRSettings",
          source: {
            path: "browser-features/modules/modules/BrowserGlue.sys.mts",
          },
        },
      ],
    },
    floorpOsApi: {
      server: {
        path: "browser-features/modules/modules/os-server/server.sys.mts",
      },
      router: {
        path: "browser-features/modules/modules/os-server/router.sys.mts",
      },
      sharedAutomationRoutes: {
        path:
          "browser-features/modules/modules/os-server/shared/routes.sys.mts",
      },
      automotorManager: {
        path:
          "browser-features/modules/modules/os-automotor/OSAutomotor-manager.sys.mts",
      },
      settingsPage: {
        path: "browser-features/pages-settings/src/app/floorp-os/page.tsx",
      },
      verification: [
        { path: "tools/os-test/verify_os_server_full.ts" },
        { path: "tools/os-test/run_verify_os_server_full_wrapper.ts" },
        { path: "tools/dev-tool.ts" },
      ],
      routeModules: [
        {
          namespace: "server",
          source: {
            path: "browser-features/modules/modules/os-server/server.sys.mts",
          },
          routes: [
            {
              method: "GET",
              path: "/health",
              source: {
                path:
                  "browser-features/modules/modules/os-server/server.sys.mts",
              },
              summary: "Reports local OS server health.",
            },
          ],
        },
        {
          namespace: "tabs",
          source: {
            path:
              "browser-features/modules/modules/os-server/tabs/routes.sys.mts",
          },
          routes: [
            {
              method: "POST",
              path: "/tabs/instances",
              source: {
                path:
                  "browser-features/modules/modules/os-server/tabs/routes.sys.mts",
              },
              summary: "Controls visible tab automation instances.",
            },
          ],
        },
        {
          namespace: "tabs shared automation",
          source: {
            path:
              "browser-features/modules/modules/os-server/shared/routes.sys.mts",
          },
          routes: [
            {
              method: "GET",
              path: "/tabs/instances/:id/element",
              source: {
                path:
                  "browser-features/modules/modules/os-server/shared/routes.sys.mts",
              },
              summary: "Floorp OS local HTTP API route.",
            },
            {
              method: "GET",
              path: "/tabs/instances/:id/text",
              source: {
                path:
                  "browser-features/modules/modules/os-server/shared/routes.sys.mts",
              },
              summary: "Extracts page or element text for automation clients.",
            },
          ],
        },
        {
          namespace: "scraper shared automation",
          source: {
            path:
              "browser-features/modules/modules/os-server/shared/routes.sys.mts",
          },
          routes: [
            {
              method: "GET",
              path: "/scraper/instances/:id/text",
              source: {
                path:
                  "browser-features/modules/modules/os-server/shared/routes.sys.mts",
              },
              summary: "Extracts page or element text for automation clients.",
            },
          ],
        },
      ],
    },
    knownDriftChecks: ["deno task dev", "ActorManagerParent.addActors"],
  };
}

function sampleInventoryWithCiCommands(): DocsInventory {
  const inventory = sampleInventory();
  return {
    ...inventory,
    ci: {
      workflows: [
        {
          name: "(CI) Browser Integration + Runner Tests",
          path: ".github/workflows/colocated_runner_test.yml",
          triggers: ["pull_request", "push", "workflow_dispatch"],
          permissions: ["contents:read"],
          runCommands: [
            "deno task feles-build test > _dist/ci-feles-build-test.log 2>&1 &",
            "deno task feles-build test > _dist/ci-downloaded-firefox-feles-build-test.log 2>&1 &",
            'deno task firefox-tests:collect --runtime-dir _dist/floorp-runtime --out _dist/firefox-tests --scope browser-chrome --source-repo "$FIREFOX_TEST_RUNTIME_REPOSITORY" --source-ref "$FIREFOX_TEST_RUNTIME_REF"',
            "deno task firefox-tests:prepare-browser",
            "deno task test --no-autostart",
            "deno task test --no-autostart --near browser-features/chrome/test/firefox-downloaded/generated --layer chrome > _dist/ci-downloaded-firefox-test.log 2>&1 &",
            "deno task test:smoke",
          ],
        },
      ],
    },
  };
}

function sampleGeneratedPages(): GeneratedPage[] {
  return REQUIRED_GENERATED_PAGE_PATHS.map((pagePath) => ({
    path: pagePath,
    title: pagePath.split("/").at(-1)!.replace(".mdx", ""),
    sidebar_label: pagePath.split("/").at(-1)!.replace(".mdx", ""),
    body:
      "Chrome features are discovered from `browser-features/chrome/common/mod.ts`.",
  }));
}

async function writeSampleGeneratedPages(
  dir: string,
  inventory: DocsInventory = sampleInventory(),
): Promise<void> {
  await seedCodexDocs(
    dir,
    `${dir}/_prompt/generate.md`,
    inventory,
  );
  await Deno.remove(`${dir}/_prompt`, { recursive: true });
}

Deno.test("verifyDocsPipeline rejects stale generated command examples", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeSampleGeneratedPages(dir);
    await Deno.writeTextFile(
      `${dir}/development/directories/bridge.mdx`,
      "Use `deno task dev` for setup. See tools/feles-build.ts.",
    );

    const issues = await verifyDocsPipeline(sampleInventory(), dir);
    assert(
      issues.some((issue) => issue.message.includes("stale command")),
      "expected stale command issue",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("verifyDocsPipeline does not treat dev-tool as stale dev task", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeSampleGeneratedPages(dir);
    await Deno.writeTextFile(
      `${dir}/development/directories/bridge.mdx`,
      "Use `deno task dev-tool`. See deno.json.",
    );

    const issues = await verifyDocsPipeline(sampleInventory(), dir);
    assertEquals(issues, []);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("verifyDocsPipeline rejects unknown deno task commands", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeSampleGeneratedPages(dir);
    await Deno.writeTextFile(
      `${dir}/development/directories/bridge.mdx`,
      "Run `deno task test:integration`. See deno.json.",
    );

    const issues = await verifyDocsPipeline(sampleInventory(), dir);
    assert(
      issues.some((issue) => issue.message.includes("unknown deno task")),
      "expected unknown deno task issue",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("verifyDocsPipeline ignores trailing colon after known deno task names", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeSampleGeneratedPages(dir);
    await Deno.writeTextFile(
      `${dir}/development/directories/bridge.mdx`,
      [
        "`deno task docs-pipeline:` is the root docs pipeline command label.",
        "The task is defined in `deno.json`.",
      ].join("\n"),
    );

    const issues = await verifyDocsPipeline(sampleInventory(), dir);
    assertEquals(issues, []);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("verifyDocsPipeline allows explicitly framed drift strings", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeSampleGeneratedPages(dir);
    await Deno.writeTextFile(
      `${dir}/development/directories/bridge.mdx`,
      [
        "Chrome features are discovered from `browser-features/chrome/common/mod.ts`.",
        "### Outdated Docs Drift Patterns",
        "- `deno task dev`",
        "- `deno task build`",
        "- `deno task clean`",
        "- `ActorManagerParent.addActors`",
      ].join("\n"),
    );

    const issues = await verifyDocsPipeline(sampleInventory(), dir);
    assertEquals(issues, []);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("verifyDocsPipeline rejects missing required generated pages", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await Deno.mkdir(`${dir}/development/directories`, { recursive: true });
    await Deno.writeTextFile(
      `${dir}/development/directories/bridge.mdx`,
      "Chrome features are discovered from `browser-features/chrome/common/mod.ts`.",
    );

    const issues = await verifyDocsPipeline(sampleInventory(), dir);
    assert(
      issues.some((issue) => issue.message.includes("missing required page")),
      "expected missing required page issue",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("verifyDocsPipeline rejects raw MDX angle brackets outside fences", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeSampleGeneratedPages(dir);
    await Deno.writeTextFile(
      `${dir}/development/directories/bridge.mdx`,
      "Use `feles-build build --phase <before-mach|after-mach>`. See tools/feles-build.ts.",
    );

    const issues = await verifyDocsPipeline(sampleInventory(), dir);
    assert(
      issues.some((issue) => issue.message.includes("raw angle brackets")),
      "expected raw angle bracket issue",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("verifyDocsPipeline rejects MDX executable syntax", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeSampleGeneratedPages(dir);
    await Deno.writeTextFile(
      `${dir}/development/directories/bridge.mdx`,
      [
        "import Dangerous from './Dangerous'",
        "Chrome features are discovered from `browser-features/chrome/common/mod.ts`.",
        "{process.env.SECRET}",
      ].join("\n"),
    );

    const issues = await verifyDocsPipeline(sampleInventory(), dir);
    assert(
      issues.some((issue) => issue.message.includes("MDX ESM")),
      "expected MDX ESM issue",
    );
    assert(
      issues.some((issue) => issue.message.includes("MDX expressions")),
      "expected MDX expression issue",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("verifyDocsPipeline allows public LLM env key names but rejects publish tokens", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeSampleGeneratedPages(dir);
    await Deno.writeTextFile(
      `${dir}/development/directories/bridge.mdx`,
      [
        "Chrome features are discovered from `browser-features/chrome/common/mod.ts`.",
        "Optional auth uses `DOCS_LLM_API_KEY`.",
        "Publishing uses `DOCS_PUBLISH_TOKEN`.",
      ].join("\n"),
    );

    const issues = await verifyDocsPipeline(sampleInventory(), dir);
    assert(
      issues.some((issue) =>
        issue.message.includes("secret or credential identifiers")
      ),
      "expected publish token issue",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("verifyDocsPipeline rejects literal escaped newlines", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeSampleGeneratedPages(dir);
    await Deno.writeTextFile(
      `${dir}/development/directories/bridge.mdx`,
      "# Architecture\\n\\nChrome features are discovered from `browser-features/chrome/common/mod.ts`.",
    );

    const issues = await verifyDocsPipeline(sampleInventory(), dir);
    assert(
      issues.some((issue) => issue.message.includes("literal escape")),
      "expected literal escape issue",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("verifyDocsPipeline rejects nonexistent referenced source paths", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeSampleGeneratedPages(dir);
    await Deno.writeTextFile(
      `${dir}/development/directories/bridge.mdx`,
      [
        "Chrome features are discovered from `browser-features/chrome/common/mod.ts`.",
        "The inventory lives at `tools/docs-pipeline/inventory.json`.",
      ].join("\n"),
    );

    const issues = await verifyDocsPipeline(sampleInventory(), dir);
    assert(
      issues.some((issue) =>
        issue.message.includes("referenced source path does not exist")
      ),
      "expected nonexistent source path issue",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("verifyDocsPipeline rejects unusable CI summaries", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeSampleGeneratedPages(dir, sampleInventoryWithCiCommands());
    await Deno.writeTextFile(
      `${dir}/development/reference/ci-test-reference.mdx`,
      [
        "See `.github/workflows/colocated_runner_test.yml`.",
        "| Workflow | Commands |",
        "|---|---|",
        "| Browser Integration | None |",
      ].join("\n"),
    );

    const issues = await verifyDocsPipeline(
      sampleInventoryWithCiCommands(),
      dir,
    );
    assert(
      issues.some((issue) =>
        issue.message.includes("missing workflow run command")
      ),
      "expected missing CI run command issue",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("writeGeneratedDocs stabilizes CI reference from inventory", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeGeneratedDocs(
      dir,
      {
        pages: sampleGeneratedPages().map((page) =>
          page.path === "development/reference/ci-test-reference.mdx"
            ? {
              ...page,
              body: "Bad model output with den o task test:smoke.",
            }
            : page
        ),
      },
      sampleInventoryWithCiCommands(),
    );

    const text = await Deno.readTextFile(
      `${dir}/development/reference/ci-test-reference.mdx`,
    );
    assert(text.includes("deno task test:smoke"));
    assert(text.includes("deno task test --no-autostart"));
    assert(
      text.includes(
        "deno task feles-build test > _dist/ci-feles-build-test.log 2>&1 &",
      ),
    );
    assert(
      text.includes(
        "deno task feles-build test > _dist/ci-downloaded-firefox-feles-build-test.log 2>&1 &",
      ),
    );
    assert(
      text.includes(
        'deno task firefox-tests:collect --runtime-dir _dist/floorp-runtime --out _dist/firefox-tests --scope browser-chrome --source-repo "$FIREFOX_TEST_RUNTIME_REPOSITORY" --source-ref "$FIREFOX_TEST_RUNTIME_REF"',
      ),
    );
    assert(text.includes("deno task firefox-tests:prepare-browser"));
    assert(
      text.includes(
        "deno task test --no-autostart --near browser-features/chrome/test/firefox-downloaded/generated --layer chrome > _dist/ci-downloaded-firefox-test.log 2>&1 &",
      ),
    );
    assert(
      text.includes(
        "deno task docs-pipeline:generate --inventory _dist/docs-pipeline/inventory.json --out docs",
      ),
    );
    assert(!text.includes("den o task"));
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("writeGeneratedDocs sanitizes unsupported generated source citations", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const pages = sampleGeneratedPages().map((page) =>
      page.path === "development/directories/static-gecko.mdx"
        ? {
          ...page,
          body: [
            "Generated files should not cite `static/gecko/config/version.txt`.",
            "Optional branding should not cite [branding](static/gecko/branding/).",
            "Use `static/gecko/pref/override.ini` for tracked Gecko pref sources.",
          ].join("\n"),
        }
        : page
    );

    await writeGeneratedDocs(
      dir,
      { pages },
      sampleInventory(),
    );

    const text = await Deno.readTextFile(
      `${dir}/development/directories/static-gecko.mdx`,
    );
    assert(!text.includes("static/gecko/config/version.txt"));
    assert(!text.includes("static/gecko/branding/"));
    assert(text.includes("`static/gecko/config/README.md`"));
    assert(text.includes("`static/gecko/pref/override.ini`"));
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("writeGeneratedDocs stabilizes command reference from inventory", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeGeneratedDocs(
      dir,
      {
        pages: sampleGeneratedPages().map((page) =>
          page.path === "development/reference/command-reference.mdx"
            ? {
              ...page,
              body: "Bad model output without feles-build subcommands.",
            }
            : page
        ),
      },
      sampleInventory(),
    );

    const text = await Deno.readTextFile(
      `${dir}/development/reference/command-reference.mdx`,
    );
    for (const command of ["dev", "test", "stage", "build", "misc"]) {
      assert(text.includes(`feles-build ${command}`));
    }
    assert(text.includes("tools/feles-build.ts"));
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("writeGeneratedDocs generates nested feature and actor catalogs from inventory", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeGeneratedDocs(
      dir,
      { pages: sampleGeneratedPages() },
      sampleInventory(),
    );

    const commonOverview = await Deno.readTextFile(
      `${dir}/development/features/browser-features/common/overview.mdx`,
    );
    assert(commonOverview.includes("Common Chrome Feature Categories"));
    assert(commonOverview.includes("Tabs & Workspaces"));

    const tabsCatalog = await Deno.readTextFile(
      `${dir}/development/features/browser-features/common/tabs-and-workspaces.mdx`,
    );
    assert(tabsCatalog.includes("workspaces"));
    assert(
      tabsCatalog.includes(
        "browser-features/chrome/common/workspaces/index.ts",
      ),
    );

    const commonDirectoryPage = await Deno.readTextFile(
      `${dir}/development/directories/browser-features/chrome/common.mdx`,
    );
    assert(commonDirectoryPage.includes("Common Chrome Features"));
    assert(
      commonDirectoryPage.includes(
        `${sampleInventory().features.chromeCommon.length} common feature entries`,
      ),
    );
    assertEquals(
      commonDirectoryPage.match(
        /browser-features\/chrome\/common\/statusbar\/index\.ts/g,
      )?.length,
      1,
    );

    const actorCatalog = await Deno.readTextFile(
      `${dir}/development/features/browser-features/modules/settings-and-internal-pages-actors.mdx`,
    );
    assert(actorCatalog.includes("NRSettings"));
    assert(
      actorCatalog.includes(
        "browser-features/modules/modules/BrowserGlue.sys.mts",
      ),
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("writeGeneratedDocs stabilizes architecture overview counts from inventory", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeGeneratedDocs(
      dir,
      {
        pages: sampleGeneratedPages().map((page) =>
          page.path === "development/architecture-overview.mdx"
            ? {
              ...page,
              body:
                "Bad model output says the inventory lists 28 common features.",
            }
            : page
        ),
      },
      sampleInventory(),
    );

    const text = await Deno.readTextFile(
      `${dir}/development/architecture-overview.mdx`,
    );
    assert(
      text.includes(
        `${sampleInventory().features.chromeCommon.length} common features`,
      ),
    );
    assert(!text.includes("28 common features"));
    assert(text.includes("http://localhost:5181/loader/index.ts"));
    assert(text.includes("chrome://noraneko/content/core.js"));
    assert(!text.includes("undefined"));
    assert(text.includes("MCP servers, and other automation clients"));
    assert(
      text.includes(
        "browser-features/modules/modules/os-server/server.sys.mts",
      ),
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("writeGeneratedDocs generates Floorp OS API layer docs from inventory", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeGeneratedDocs(
      dir,
      { pages: sampleGeneratedPages() },
      sampleInventory(),
    );

    const text = await Deno.readTextFile(
      `${dir}/development/directories/floorp-os-api.mdx`,
    );
    assert(text.includes("MCP servers"));
    assert(text.includes("MCP-server usage is a docs requirement"));
    assert(text.includes("/tabs/instances"));
    assert(text.includes("/tabs/instances/:id/element"));
    assert(text.includes("/scraper/instances/:id/text"));
    assert(!text.includes("/scraper/instances/:id/element"));
    assert(!text.includes("token file"));
    assert(!text.includes("Request bodies are size-limited"));
    assert(!text.includes("body limits"));
    assert(
      text.includes(
        "browser-features/modules/modules/os-server/server.sys.mts",
      ),
    );
    assert(text.includes("tools/os-test/verify_os_server_full.ts"));
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("writeGeneratedDocs generates static Gecko docs from tracked sources", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeGeneratedDocs(
      dir,
      { pages: sampleGeneratedPages() },
      sampleInventory(),
    );

    const text = await Deno.readTextFile(
      `${dir}/development/directories/static-gecko.mdx`,
    );
    assert(text.includes("`static/gecko/pref/override.ini`"));
    assert(text.includes("`static/gecko/config/README.md`"));
    assert(text.includes("`.github/workflows/package.yml`"));
    assert(!text.includes("static/gecko/config/version.txt"));
    assert(!text.includes("from under `.github/workflows/package.yml`"));
    assert(!text.includes("contains generated Gecko version files"));
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("verifyDocsPipeline rejects stale deterministic catalog pages", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeSampleGeneratedPages(dir);
    await Deno.writeTextFile(
      `${dir}/development/features/browser-features/common/tabs-and-workspaces.mdx`,
      [
        "Chrome features are discovered from `browser-features/chrome/common/mod.ts`.",
        "This stale page omits the inventory-backed workspaces row.",
      ].join("\n"),
    );

    const issues = await verifyDocsPipeline(sampleInventory(), dir);
    assert(
      issues.some((issue) =>
        issue.message.includes("deterministic generated page is stale")
      ),
      "expected stale deterministic page issue",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("verifyDocsPipeline ignores volatile commit changes in deterministic pages", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const generatedInventory = sampleInventory();
    await writeSampleGeneratedPages(dir, generatedInventory);

    const pullRequestInventory = {
      ...generatedInventory,
      floorpCommit: "synthetic-pr-merge-commit",
    };
    const issues = await verifyDocsPipeline(pullRequestInventory, dir);

    assertEquals(issues, []);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("seedCodexDocs writes required docs and prompt", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const docsDir = `${dir}/generated`;
    const promptPath = `${dir}/codex/generate.md`;
    const written = await seedCodexDocs(docsDir, promptPath, sampleInventory());

    assertEquals(written.length, REQUIRED_GENERATED_PAGE_PATHS.length);
    for (const pagePath of REQUIRED_GENERATED_PAGE_PATHS) {
      const filePath = `${docsDir}/${pagePath}`;
      const text = await Deno.readTextFile(filePath);
      assert(text.includes("floorp_commit"));
      assert(
        text.includes("browser-features") ||
          text.includes("deno.json") ||
          text.includes(".github/workflows") ||
          text.includes("tools/feles-build.ts"),
      );
    }

    const prompt = await Deno.readTextFile(promptPath);
    assert(prompt.includes("_dist/docs-pipeline/inventory.json"));
    assert(prompt.includes("architecture-overview.mdx"));
    assert(prompt.includes("browser-features/chrome/common/mod.ts"));
    assert(!prompt.includes("TOKEN"));
    assert(!prompt.includes("API_KEY"));
    assert(!prompt.includes("secrets."));
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("verifyCodexAuditResult accepts passing audit", () => {
  const audit = verifyCodexAuditResult({
    pass: true,
    blocking_findings: [],
    warnings: ["Architecture page could be more detailed."],
    recommendation: "Use generated docs.",
  });

  assertEquals(audit.pass, true);
});

Deno.test("verifyCodexAuditResult rejects malformed or blocking audits", () => {
  for (
    const value of [
      "{bad json}",
      { pass: "true", blocking_findings: [], warnings: [], recommendation: "" },
      {
        pass: false,
        blocking_findings: ["Missing source citation"],
        warnings: [],
        recommendation: "Do not publish.",
      },
      {
        pass: true,
        blocking_findings: ["Missing source citation"],
        warnings: [],
        recommendation: "Do not publish.",
      },
      {
        pass: true,
        blocking_findings: [1],
        warnings: [],
        recommendation: "Do not publish.",
      },
      {
        pass: true,
        blocking_findings: [],
        warnings: [{}],
        recommendation: "Do not publish.",
      },
    ]
  ) {
    let rejected = false;
    try {
      if (typeof value === "string") {
        JSON.parse(value);
      } else {
        verifyCodexAuditResult(value);
      }
    } catch {
      rejected = true;
    }
    assert(rejected, "invalid Codex audit should be rejected");
  }
});

Deno.test("codexAuditSchema requires pass and finding fields", () => {
  const schema = codexAuditSchema() as {
    required?: string[];
    additionalProperties?: boolean;
  };

  assertEquals(schema.additionalProperties, false);
  assertEquals(schema.required, [
    "pass",
    "blocking_findings",
    "warnings",
    "recommendation",
  ]);
});

Deno.test("verifyDocsPipeline accepts source-backed generated MDX", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeSampleGeneratedPages(dir);
    await Deno.writeTextFile(
      `${dir}/development/directories/bridge.mdx`,
      [
        "Chrome features are discovered from `browser-features/chrome/common/mod.ts`.",
        "See [Command Reference](../reference/command-reference).",
      ].join("\n"),
    );

    const issues = await verifyDocsPipeline(sampleInventory(), dir);
    assertEquals(issues, []);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});
