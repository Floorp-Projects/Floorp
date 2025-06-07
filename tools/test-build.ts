#!/usr/bin/env -S deno run -A
/**
 * Quick test script to verify the refactored build system works
 */

import { CLI } from "./build/cli.ts";
import { log } from "./build/logger.ts";

// Test CLI parsing
console.log("🧪 Testing CLI argument parsing...");

const testCases = [
  ["--help"],
  ["--dev"],
  ["--production-before"],
  ["--production-after"],
  ["--dev", "--init-git-for-patch"],
];

testCases.forEach((args, index) => {
  if (args[0] === "--help") {
    console.log(`Test ${index + 1}: Skipping help test`);
    return;
  }

  try {
    const options = CLI.parseArgs(args);
    console.log(
      `✅ Test ${index + 1}: ${args.join(" ")} -> ${JSON.stringify(options)}`,
    );
  } catch (error) {
    console.log(`❌ Test ${index + 1}: ${args.join(" ")} -> Error: ${error}`);
  }
});

// Test logger
console.log("\n🧪 Testing logger...");
log.info("Info message test");
log.warn("Warning message test");
log.success("Success message test");

console.log("\n✅ Basic tests completed!");
console.log(
  "💡 Run 'deno run -A tools/build.ts --help' to see full CLI options",
);
