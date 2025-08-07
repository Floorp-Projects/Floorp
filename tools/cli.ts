/**
 * CLI - Command line interface parsing and help
 */

import { BuildOptions } from "./utils/types.ts";
import {
  mapCLIArgsToBuildOptions,
  parseCLIArgs,
  RawCLIArgs,
  validateCLIArgs,
} from "./utils/cli-args.ts";

export class CLI {
  static showHelp(): void {
    console.log(`
🏗️  feles-build : Noraneko Build System

Development:
  deno run -A tools/feles-build.ts --mode=dev              # Run dev environment with browser
  deno run -A tools/feles-build.ts --mode=stage            # Run production environment with browser
  deno run -A tools/feles-build.ts --mode=test             # Run production environment for test with browser

Production (CI recommended):
  deno run -A tools/feles-build.ts --production-mozbuild=before    # Phase 1: Prepare scripts
  [Run: ./mach build]
  deno run -A tools/feles-build.ts --production-mozbuild=after     # Phase 2: Apply injections

Patch Development:
  deno run -A tools/feles-build.ts --init-git-for-patch     # Dev with Git init
    `);
  }

  /**
   * Parse CLI arguments, validate, and map to BuildOptions.
   */
  static parseArgs(args: string[]): BuildOptions {
    const parsedArgs: RawCLIArgs = parseCLIArgs(args);

    if (parsedArgs.help || parsedArgs.h) {
      this.showHelp();
      Deno.exit(0);
    }

    try {
      validateCLIArgs(parsedArgs);
    } catch (err) {
      console.error(err.message);
      this.showHelp();
      Deno.exit(1);
    }

    return mapCLIArgsToBuildOptions(parsedArgs);
  }
}
