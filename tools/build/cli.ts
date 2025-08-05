/**
 * CLI - Command line interface parsing and help
 */

import type { BuildMode, BuildOptions, BuildPhase } from "./types.ts";

export class CLI {
  static showHelp(): void {
    console.log(`
🏗️  Noraneko Build System

Development:
  deno run -A tools/build.ts --dev              # Dev build with browser

Production (CI recommended):
  deno run -A tools/build.ts --production-before    # Phase 1: Prepare scripts
  [Run: ./mach build]
  deno run -A tools/build.ts --production-after     # Phase 2: Apply injections

Patch Development:
  deno run -A tools/build.ts --dev --init-git-for-patch     # Dev with Git init
  deno run -A tools/build.ts --production --init-git-for-patch # Prod with Git init

Legacy:
  deno run -A tools/build.ts --production       # Full production build

Aliases:
  --release-build-before    # Same as --production-before
  --release-build-after     # Same as --production-after
    `);
  }

  static parseArgs(args: string[]): BuildOptions {
    if (args.includes("--help") || args.includes("-h")) {
      this.showHelp();
      Deno.exit(0);
    }

    const initGitForPatch = args.includes("--init-git-for-patch");
    let mode: BuildMode = "dev";
    let phase: BuildPhase = "full";

    if (args.includes("--dev")) {
      mode = "dev";
    } else if (
      args.includes("--production") ||
      args.includes("--production-before") ||
      args.includes("--production-after") ||
      args.includes("--release-build-before") ||
      args.includes("--release-build-after")
    ) {
      mode = "production";
    } else if (args.includes("--test")) {
      mode = "test";
    }

    if (
      args.includes("--production-before") ||
      args.includes("--release-build-before")
    ) {
      phase = "before";
    } else if (
      args.includes("--production-after") ||
      args.includes("--release-build-after")
    ) {
      phase = "after";
    }

    const launchBrowser = (mode === "dev" || mode == "test") &&
      phase === "full";

    return {
      mode,
      phase,
      initGitForPatch,
      launchBrowser,
    };
  }
}
