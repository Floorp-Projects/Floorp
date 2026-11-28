// SPDX-License-Identifier: MPL-2.0

import * as path from "node:path";
import type {
  NodeBuildCommand,
  NodeBuildMode,
  NodeBuildPlan,
  NodeBuildPlanOptions,
} from "./node_build_types.ts";

export const PAGE_BUILDS = [
  [
    "pages-newtab",
    "browser-features/pages-newtab",
    "chrome://noraneko-newtab/content",
  ],
  [
    "pages-settings",
    "browser-features/pages-settings",
    "chrome://noraneko-settings/content",
  ],
  [
    "pages-welcome",
    "browser-features/pages-welcome",
    "chrome://noraneko-welcome/content",
  ],
  [
    "pages-notes",
    "browser-features/pages-notes",
    "chrome://noraneko-notes/content",
  ],
  [
    "pages-modal-child",
    "browser-features/pages-modal-child",
    "chrome://noraneko-modal-child/content",
  ],
  [
    "pages-profile-manager",
    "browser-features/pages-profile-manager",
    "chrome://noraneko-profile-manager/content",
  ],
  [
    "pages-aboutDialog",
    "browser-features/pages-aboutDialog",
    "chrome://noraneko-pages-aboutdialog/content",
  ],
] as const;

export const PACKAGED_ASSET_MOUNTS = [
  ["content", "bridge/loader-features/_dist"],
  ["startup", "bridge/startup/_dist"],
  ["skin", "browser-features/skin"],
  ["resource", "bridge/loader-modules/_dist"],
  ...PAGE_BUILDS.map(([name, directory]) => [name, `${directory}/_dist`]),
] as const;

export function normalizeNodeBuildMode(mode: string): NodeBuildMode {
  const normalized = mode.trim().toLowerCase();
  if (
    normalized === "dev" || normalized === "test" ||
    normalized === "stage" || normalized === "production"
  ) {
    return normalized;
  }
  throw new Error(
    `Unsupported build mode: ${mode}. Allowed: dev, test, stage, production`,
  );
}

function command(
  name: string,
  command: string,
  args: readonly string[],
  cwd: string,
): NodeBuildCommand {
  return { name, command, args, cwd };
}

export function createNodeBuildPlan(
  options: NodeBuildPlanOptions,
): NodeBuildPlan {
  const mode = normalizeNodeBuildMode(options.mode);
  const packagedQa = options.packagedQa ?? false;
  const root = options.projectRoot;
  const node = options.nodeExecutable;
  const tsdown = options.tsdownExecutable;
  const viteBuild = path.join(root, "tools/vite-build-node.ts");

  const startupMode = packagedQa ? "test-packaged" : mode;
  const startup = command(
    "bridge/startup",
    node,
    [tsdown, "--config-loader", "unrun", `--env.MODE=${startupMode}`],
    path.join(root, "bridge/startup"),
  );
  const loaderModules = command(
    "bridge/loader-modules",
    node,
    [
      tsdown,
      "--config-loader",
      "unrun",
      `--env.__BUILDID2__=${options.buildid2}`,
      `--env.__VERSION2__=${options.version}`,
    ],
    path.join(root, "bridge/loader-modules"),
  );
  const loaderFeatures = command(
    "bridge/loader-features",
    node,
    [viteBuild, "--base", "chrome://noraneko/content"],
    path.join(root, "bridge/loader-features"),
  );

  if (mode === "dev" || mode === "test") {
    return {
      mode,
      commands: [startup, loaderModules, loaderFeatures],
      usesPackagedAssets: false,
      packagedQa: false,
    };
  }

  const pageCommands = PAGE_BUILDS.map(([name, directory, base]) =>
    command(
      name,
      node,
      [viteBuild, "--config", "vite.config.ts", "--base", base],
      path.join(root, directory),
    )
  );

  const commands: NodeBuildCommand[] = [
    startup,
    loaderFeatures,
    ...pageCommands,
    loaderModules,
  ];

  if (packagedQa) {
    commands.push(command(
      "bridge/loader-features packaged QA",
      node,
      [viteBuild, "--config", "vite.packaged-test.config.ts"],
      path.join(root, "bridge/loader-features"),
    ));
  }

  return {
    mode,
    commands,
    usesPackagedAssets: true,
    packagedQa,
  };
}
