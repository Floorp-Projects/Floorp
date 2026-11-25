// SPDX-License-Identifier: MPL-2.0

import type { Workflow } from "./types.ts";

export function workflowToTypeScriptArtifact(workflow: Workflow): string {
  const literal = JSON.stringify(workflow, null, 2);
  return `import type { Workflow } from "./types.ts";\n\nexport default ${literal} satisfies Workflow;\n`;
}
