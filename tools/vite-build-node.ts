// SPDX-License-Identifier: MPL-2.0

import process from "node:process";
import { runViteBuildCommand } from "./src/vite_build.ts";

const code = await runViteBuildCommand(process.argv.slice(2), {
  loadBuild: async () => {
    const { build } = await import("vite");
    return (config) => build(config);
  },
  info: (message) => console.log(message),
  error: (error) => console.error("[node-build] Vite build failed:", error),
});

process.exit(code);
