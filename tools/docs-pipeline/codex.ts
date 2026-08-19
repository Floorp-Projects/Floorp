// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import type { DocsInventory, GeneratedDocsPayload } from "./types.ts";
import {
  DETERMINISTIC_GENERATED_PAGE_PATHS,
  REQUIRED_GENERATED_PAGE_PATHS,
} from "./types.ts";
import { writeGeneratedDocs } from "./generator.ts";

export type CodexAuditResult = {
  pass: boolean;
  blocking_findings: string[];
  warnings: string[];
  recommendation: string;
};

const DETERMINISTIC_PAGES: ReadonlySet<string> = new Set(
  DETERMINISTIC_GENERATED_PAGE_PATHS,
);

export async function seedCodexDocs(
  outputDir: string,
  promptPath: string,
  inventory: DocsInventory,
): Promise<string[]> {
  const payload: GeneratedDocsPayload = {
    pages: REQUIRED_GENERATED_PAGE_PATHS.map((pagePath) => ({
      path: pagePath,
      title: titleForPage(pagePath),
      sidebar_label: sidebarLabelForPage(pagePath),
      body: !DETERMINISTIC_PAGES.has(pagePath)
        ? prosePageStub(pagePath, inventory)
        : "This page is generated deterministically from the Floorp inventory.",
    })),
  };

  const written = await writeGeneratedDocs(outputDir, payload, inventory);
  await Deno.mkdir(path.dirname(promptPath), { recursive: true });
  await Deno.writeTextFile(
    promptPath,
    buildCodexGenerationPrompt(outputDir, inventory),
  );
  return written;
}

export function verifyCodexAuditResult(value: unknown): CodexAuditResult {
  if (!value || typeof value !== "object") {
    throw new Error("Codex audit result must be an object");
  }

  const candidate = value as Partial<CodexAuditResult>;
  if (typeof candidate.pass !== "boolean") {
    throw new Error("Codex audit result must include boolean pass");
  }
  if (
    !Array.isArray(candidate.blocking_findings) ||
    !candidate.blocking_findings.every((finding) => typeof finding === "string")
  ) {
    throw new Error(
      "Codex audit result must include blocking_findings strings",
    );
  }
  if (
    !Array.isArray(candidate.warnings) ||
    !candidate.warnings.every((warning) => typeof warning === "string")
  ) {
    throw new Error("Codex audit result must include warnings strings");
  }
  if (typeof candidate.recommendation !== "string") {
    throw new Error("Codex audit result must include recommendation string");
  }
  if (!candidate.pass || candidate.blocking_findings.length > 0) {
    throw new Error(
      `Codex audit failed: ${candidate.blocking_findings.join("; ")}`,
    );
  }

  return candidate as CodexAuditResult;
}

export async function verifyCodexAuditFile(
  auditPath: string,
): Promise<CodexAuditResult> {
  const text = await Deno.readTextFile(auditPath);
  return verifyCodexAuditResult(JSON.parse(text));
}

export function codexAuditSchema(): Record<string, unknown> {
  return {
    type: "object",
    additionalProperties: false,
    properties: {
      pass: { type: "boolean" },
      blocking_findings: {
        type: "array",
        items: { type: "string" },
      },
      warnings: {
        type: "array",
        items: { type: "string" },
      },
      recommendation: { type: "string" },
    },
    required: ["pass", "blocking_findings", "warnings", "recommendation"],
  };
}

function buildCodexGenerationPrompt(
  outputDir: string,
  inventory: DocsInventory,
): string {
  const architectureSources = [
    ...inventory.architecture.layers.map((layer) => layer.source.path),
    ...inventory.architecture.referenceSources.map((entry) =>
      entry.source.path
    ),
    inventory.architecture.chromeFeatureDiscovery.source.path,
    inventory.architecture.windowActors.source.path,
    inventory.architecture.bridgeLoader.source.path,
    inventory.architecture.loaderDevServer.source.path,
  ];
  const uniqueSources = [...new Set(architectureSources)].sort();
  const normalizedOutputDir = outputDir.replaceAll("\\", "/");
  const prosePages = REQUIRED_GENERATED_PAGE_PATHS.filter((pagePath) =>
    !DETERMINISTIC_PAGES.has(pagePath)
  );

  return [
    "# Floorp Developer Docs Architecture Authoring Task",
    "",
    "You are updating generated English MDX pages under Floorp's docs directory.",
    "This is a narrow CI documentation generation step, not a repository development session.",
    "Do not apply repository four-phase workflows, do not spawn subagents, do not run tests, and do not perform broad repository searches.",
    "Use only the inventory, the source files listed below, and the target MDX page.",
    "",
    "## Inputs",
    "",
    "- Inventory JSON: `_dist/docs-pipeline/inventory.json`",
    "- Output files to edit:",
    ...prosePages.map((pagePath) =>
      `  - \`${normalizedOutputDir}/${pagePath}\``
    ),
    "",
    "Read only the inventory and these source paths for factual grounding:",
    "",
    ...uniqueSources.map((source) => `- \`${source}\``),
    "",
    "## Requirements",
    "",
    "- Edit only the output files listed above.",
    "- Keep the existing frontmatter.",
    "- Write English developer documentation.",
    "- Cite Floorp source paths inline for architectural claims.",
    "- Use only exact file paths from the allowed source list as citations; do not cite directories.",
    "- Do not invent commands, APIs, files, ports, or behavior.",
    "- Do not include LLM service endpoint URLs, keys, prompt payloads, raw model responses, or secret values.",
    "- Do not use MDX imports, exports, JSX, or expression syntax.",
    "- Do not edit feature catalog, source-inventory, command-reference, or ci-test-reference pages; those are deterministic.",
    "- Organize prose around repository directories, not shallow topic summaries.",
    "- Use nested headings inside each directory page for important child directories and files.",
    "- Be comprehensive: prefer concrete implementation details, ownership boundaries, lifecycle ordering, extension points, failure modes, and verification commands over high-level summaries.",
    "- Target roughly 1,500 to 2,500 words per prose page when the available sources support it.",
    "- After writing the target file, stop. Do not run verification; the Deno pipeline runs verification after you exit.",
    "",
    "## Page Requirements",
    "",
    "- `architecture-overview.mdx`: explain the top-level map and how the directory pages fit together; keep it as an index to the deeper directory documentation rather than a long generic overview.",
    "- `directories/bridge.mdx`: deeply explain the `bridge` area, including startup bridge selection, HTTP loader gating, loader dev server, loader entrypoint, module registry, feature enablement prefs, lifecycle hooks, module load hooks, and failure handling.",
    "- `directories/browser-features/overview.mdx`: explain the browser-features directory map and link the chrome, modules, and pages-settings subtrees together without duplicating child pages.",
    "- `directories/browser-features/chrome/overview.mdx`: explain the chrome feature subtree, README ownership conventions, how common/static/nora/utils/example relate, and how this subtree connects to the bridge loader.",
    "- `directories/browser-features/chrome/common.mdx`: deeply explain common chrome feature discovery from `browser-features/chrome/common/mod.ts`, hot reload expectations, feature identity, loader registry integration, lifecycle hooks, and common failure modes.",
    "- `directories/browser-features/chrome/static.mdx`: deeply explain static chrome feature discovery from `browser-features/chrome/static/mod.ts`, when static features are appropriate, how they still participate in the loader registry, and how they differ from static Gecko prefs.",
    "- `directories/browser-features/modules/overview.mdx`: explain the modules subtree at a high level and when privileged modules or Window Actors belong there rather than in chrome features or settings pages.",
    "- `directories/browser-features/modules/browser-glue.mdx`: deeply explain BrowserGlue, Window Actor registration through ActorManagerParent.addJSWindowActors, local actor path conversion, actor table ownership, match/event responsibilities, and failure modes.",
    "- `directories/browser-features/pages-settings/overview.mdx`: explain the settings pages subtree, the React/Tailwind page-bundle boundary, and how it differs from loader-managed chrome features and Window Actors.",
    "- `directories/browser-features/pages-settings/build.mdx`: deeply explain the settings Vite config, React SWC/Tailwind/plugins, chrome packaging, dev CSP behavior, output ownership, and build failure modes.",
    "- `directories/browser-features/pages-settings/routing.mdx`: deeply explain settings bootstrap from `src/main.tsx`, hash-seeded MemoryRouter behavior, `src/App.tsx` route ownership, route extension points, and runtime failure modes.",
    "- `directories/tools-and-ci.mdx`: deeply explain `tools` and `.github/workflows`, including feles-build responsibilities, dev-tool inspection, colocated browser test discovery/filtering/autostart/result collection, docs pipeline workflow boundaries, and CI verification responsibilities.",
    "- `directories/static-gecko.mdx`: deeply explain `static/gecko`, especially pref override ownership, what defaults belong there, what should stay in TypeScript layers, and how Gecko defaults interact with loader/runtime code.",
    "",
    `Output directory: \`${normalizedOutputDir}\``,
    "",
  ].join("\n");
}

function prosePageStub(pagePath: string, inventory: DocsInventory): string {
  return [
    `# ${titleForPage(pagePath)}`,
    "",
    "This page is intentionally seeded before Codex rewrites it from the source inventory and allowed source files.",
    "",
    "## Source Anchors",
    "",
    ...inventory.architecture.layers.map((layer) =>
      `- ${layer.name}: \`${layer.source.path}\``
    ),
    ...inventory.architecture.referenceSources.map((entry) =>
      `- ${entry.area}: \`${entry.source.path}\``
    ),
    `- Chrome feature discovery: \`${inventory.architecture.chromeFeatureDiscovery.source.path}\``,
    `- Window Actors: \`${inventory.architecture.windowActors.source.path}\``,
    `- Bridge loader: \`${inventory.architecture.bridgeLoader.source.path}\``,
    `- Loader dev server: \`${inventory.architecture.loaderDevServer.source.path}\``,
  ].join("\n");
}

function titleForPage(pagePath: string): string {
  switch (pagePath) {
    case "development/architecture-overview.mdx":
      return "Architecture Overview";
    case "development/directories/bridge.mdx":
      return "Bridge Directory";
    case "development/directories/browser-features/overview.mdx":
      return "Browser Features Overview";
    case "development/directories/browser-features/chrome/overview.mdx":
      return "Chrome Features Overview";
    case "development/directories/browser-features/chrome/common.mdx":
      return "Common Chrome Features";
    case "development/directories/browser-features/chrome/static.mdx":
      return "Static Chrome Features";
    case "development/directories/browser-features/modules/overview.mdx":
      return "Modules Overview";
    case "development/directories/browser-features/modules/browser-glue.mdx":
      return "BrowserGlue & Window Actors";
    case "development/directories/browser-features/pages-settings/overview.mdx":
      return "Settings Pages Overview";
    case "development/directories/browser-features/pages-settings/build.mdx":
      return "Settings Pages Build";
    case "development/directories/browser-features/pages-settings/routing.mdx":
      return "Settings Pages Routing";
    case "development/directories/tools-and-ci.mdx":
      return "Tools & CI Directories";
    case "development/directories/floorp-os-api.mdx":
      return "Floorp OS API Layer";
    case "development/directories/static-gecko.mdx":
      return "Static Gecko Directory";
    case "development/features/browser-features/overview.mdx":
      return "Browser Features Catalog";
    case "development/features/browser-features/chrome-common.mdx":
      return "Common Chrome Feature Catalog";
    case "development/features/browser-features/chrome-static.mdx":
      return "Static Chrome Feature Catalog";
    case "development/features/browser-features/settings-pages.mdx":
      return "Settings Page Feature Catalog";
    case "development/features/browser-features/window-actors.mdx":
      return "Window Actor Catalog";
    case "development/features/browser-features/common/overview.mdx":
      return "Common Chrome Feature Categories";
    case "development/features/browser-features/common/tabs-and-workspaces.mdx":
      return "Tabs & Workspaces";
    case "development/features/browser-features/common/sidebar-and-panels.mdx":
      return "Sidebar & Panels";
    case "development/features/browser-features/common/browser-ui-customization.mdx":
      return "Browser UI Customization";
    case "development/features/browser-features/common/input-and-shortcuts.mdx":
      return "Input & Shortcuts";
    case "development/features/browser-features/common/webapps-and-integration.mdx":
      return "Web Apps & Integration";
    case "development/features/browser-features/common/utilities-and-actions.mdx":
      return "Utilities & Actions";
    case "development/features/browser-features/modules/overview.mdx":
      return "Window Actor Categories";
    case "development/features/browser-features/modules/settings-and-internal-pages-actors.mdx":
      return "Settings & Internal Page Actors";
    case "development/features/browser-features/modules/web-content-and-store-actors.mdx":
      return "Web Content & Store Actors";
    case "development/features/browser-features/modules/pwa-workspaces-profile-actors.mdx":
      return "PWA, Workspaces & Profile Actors";
    case "development/reference/source-inventory.mdx":
      return "Source Inventory";
    case "development/reference/command-reference.mdx":
      return "Command Reference";
    case "development/reference/ci-test-reference.mdx":
      return "CI & Test Reference";
    default:
      return pagePath;
  }
}

function sidebarLabelForPage(pagePath: string): string {
  switch (pagePath) {
    case "development/architecture-overview.mdx":
      return "Architecture";
    case "development/directories/bridge.mdx":
      return "Bridge";
    case "development/directories/browser-features/overview.mdx":
      return "Overview";
    case "development/directories/browser-features/chrome/overview.mdx":
      return "Overview";
    case "development/directories/browser-features/chrome/common.mdx":
      return "Common";
    case "development/directories/browser-features/chrome/static.mdx":
      return "Static";
    case "development/directories/browser-features/modules/overview.mdx":
      return "Overview";
    case "development/directories/browser-features/modules/browser-glue.mdx":
      return "BrowserGlue";
    case "development/directories/browser-features/pages-settings/overview.mdx":
      return "Overview";
    case "development/directories/browser-features/pages-settings/build.mdx":
      return "Build";
    case "development/directories/browser-features/pages-settings/routing.mdx":
      return "Routing";
    case "development/directories/tools-and-ci.mdx":
      return "Tools & CI";
    case "development/directories/floorp-os-api.mdx":
      return "Floorp OS API";
    case "development/directories/static-gecko.mdx":
      return "Static Gecko";
    case "development/features/browser-features/overview.mdx":
      return "Feature Catalog";
    case "development/features/browser-features/chrome-common.mdx":
      return "Common Chrome";
    case "development/features/browser-features/chrome-static.mdx":
      return "Static Chrome";
    case "development/features/browser-features/settings-pages.mdx":
      return "Settings Pages";
    case "development/features/browser-features/window-actors.mdx":
      return "Window Actors";
    case "development/features/browser-features/common/overview.mdx":
      return "Common Categories";
    case "development/features/browser-features/common/tabs-and-workspaces.mdx":
      return "Tabs & Workspaces";
    case "development/features/browser-features/common/sidebar-and-panels.mdx":
      return "Sidebar & Panels";
    case "development/features/browser-features/common/browser-ui-customization.mdx":
      return "UI Customization";
    case "development/features/browser-features/common/input-and-shortcuts.mdx":
      return "Input & Shortcuts";
    case "development/features/browser-features/common/webapps-and-integration.mdx":
      return "Web Apps";
    case "development/features/browser-features/common/utilities-and-actions.mdx":
      return "Utilities";
    case "development/features/browser-features/modules/overview.mdx":
      return "Actor Categories";
    case "development/features/browser-features/modules/settings-and-internal-pages-actors.mdx":
      return "Settings Actors";
    case "development/features/browser-features/modules/web-content-and-store-actors.mdx":
      return "Web Content Actors";
    case "development/features/browser-features/modules/pwa-workspaces-profile-actors.mdx":
      return "PWA & Workspaces";
    case "development/reference/source-inventory.mdx":
      return "Source Inventory";
    case "development/reference/command-reference.mdx":
      return "Commands";
    case "development/reference/ci-test-reference.mdx":
      return "CI & Tests";
    default:
      return pagePath;
  }
}
