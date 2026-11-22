// SPDX-License-Identifier: MPL-2.0
import { runViteBuildCommand } from "./src/vite_build.ts";
// Vite's CLI can retain Node-compatible handles after a completed build on Deno.
// Exit only after the build API promise (including close hooks) has settled.
if (import.meta.main) {
  const code = await runViteBuildCommand(Deno.args, {
    loadBuild: async () => {
      const { build } = await import("vite");
      return (config) => build(config);
    },
    info: (message) => console.log(message),
    error: (error) => console.error("[builder] Vite build failed:", error),
  });
  Deno.exit(code);
}
