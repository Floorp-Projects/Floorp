// SPDX-License-Identifier: MPL-2.0

import { parseArgs } from "@std/cli";
import * as path from "@std/path";
import { collectDocsInventory } from "./collector.ts";
import {
  codexAuditSchema,
  seedCodexDocs,
  verifyCodexAuditFile,
} from "./codex.ts";
import {
  generateDocsPayload,
  readLlmConfig,
  writeGeneratedDocs,
} from "./generator.ts";
import {
  readAuditLlmConfig,
  runLlmAudit,
  verifyLlmAuditFile,
} from "./llm_audit.ts";
import { verifyDocsPipeline } from "./verifier.ts";
import type { DocsInventory } from "./types.ts";

const HELP = `
Usage: deno task docs-pipeline <command> [options]

Commands:
  collect    Generate deterministic Floorp docs inventory JSON
  generate   Generate Floorp developer docs MDX from an inventory
  verify     Verify inventory and optional generated MDX
  audit      Audit generated MDX with an OpenAI-compatible LLM
  codex-seed Generate deterministic docs and a Codex architecture prompt
  codex-audit-schema Print the Codex audit JSON schema
  codex-audit-verify Verify Codex audit JSON output

Options:
  --inventory <path>   Inventory JSON path
  --out <path>         Output file or directory
  --docs-dir <path>    Generated docs directory to verify
  --prompt-out <path>  Codex generation prompt path
  --audit <path>       Audit JSON path
  --help, -h           Show this help
`.trim();

function ensureString(value: unknown, name: string): string {
  if (typeof value !== "string" || value.trim() === "") {
    throw new Error(`${name} is required`);
  }
  return value;
}

async function readInventory(inventoryPath: string): Promise<DocsInventory> {
  return JSON.parse(await Deno.readTextFile(inventoryPath)) as DocsInventory;
}

async function writeJsonFile(filePath: string, value: unknown): Promise<void> {
  await Deno.mkdir(path.dirname(filePath), { recursive: true });
  await Deno.writeTextFile(filePath, `${JSON.stringify(value, null, 2)}\n`);
}

async function collectCommand(args: string[]): Promise<void> {
  const parsed = parseArgs(args, {
    string: ["out"],
    boolean: ["help"],
    alias: { h: "help" },
  });
  if (parsed.help) {
    console.log(HELP);
    return;
  }

  const inventory = await collectDocsInventory();
  const out = parsed.out;
  if (typeof out === "string") {
    await writeJsonFile(out, inventory);
  } else {
    console.log(JSON.stringify(inventory, null, 2));
  }
}

async function generateCommand(args: string[]): Promise<void> {
  const parsed = parseArgs(args, {
    string: ["inventory", "out"],
    boolean: ["help"],
    alias: { h: "help" },
  });
  if (parsed.help) {
    console.log(HELP);
    return;
  }

  const inventoryPath = ensureString(parsed.inventory, "--inventory");
  const outDir = ensureString(parsed.out, "--out");
  const inventory = await readInventory(inventoryPath);
  const payload = await generateDocsPayload(inventory, readLlmConfig());
  const written = await writeGeneratedDocs(outDir, payload, inventory);
  console.log(`Generated ${written.length} docs page(s).`);
}

async function verifyCommand(args: string[]): Promise<void> {
  const parsed = parseArgs(args, {
    string: ["inventory", "docs-dir"],
    boolean: ["help"],
    alias: { h: "help" },
  });
  if (parsed.help) {
    console.log(HELP);
    return;
  }

  const inventoryPath = ensureString(parsed.inventory, "--inventory");
  const inventory = await readInventory(inventoryPath);
  const issues = await verifyDocsPipeline(inventory, parsed["docs-dir"]);
  if (issues.length === 0) {
    console.log("Docs pipeline verification passed.");
    return;
  }

  for (const issue of issues) {
    console.error(`[docs-pipeline] ${issue.path}: ${issue.message}`);
  }
  throw new Error(
    `Docs pipeline verification failed with ${issues.length} issue(s)`,
  );
}

async function auditCommand(args: string[]): Promise<void> {
  const parsed = parseArgs(args, {
    string: ["inventory", "docs-dir", "out"],
    boolean: ["help"],
    alias: { h: "help" },
  });
  if (parsed.help) {
    console.log(HELP);
    return;
  }

  const inventoryPath = ensureString(parsed.inventory, "--inventory");
  const docsDir = ensureString(parsed["docs-dir"], "--docs-dir");
  const inventory = await readInventory(inventoryPath);
  const audit = await runLlmAudit(inventory, docsDir, readAuditLlmConfig());
  if (typeof parsed.out === "string") {
    await writeJsonFile(parsed.out, audit);
  } else {
    console.log(JSON.stringify(audit, null, 2));
  }
}

async function auditVerifyCommand(args: string[]): Promise<void> {
  const parsed = parseArgs(args, {
    string: ["audit"],
    boolean: ["help"],
    alias: { h: "help" },
  });
  if (parsed.help) {
    console.log(HELP);
    return;
  }

  const auditPath = ensureString(parsed.audit, "--audit");
  await verifyLlmAuditFile(auditPath);
  console.log("LLM audit verification passed.");
}

async function codexSeedCommand(args: string[]): Promise<void> {
  const parsed = parseArgs(args, {
    string: ["inventory", "out", "prompt-out"],
    boolean: ["help"],
    alias: { h: "help" },
  });
  if (parsed.help) {
    console.log(HELP);
    return;
  }

  const inventoryPath = ensureString(parsed.inventory, "--inventory");
  const outDir = ensureString(parsed.out, "--out");
  const promptOut = ensureString(parsed["prompt-out"], "--prompt-out");
  const inventory = await readInventory(inventoryPath);
  const written = await seedCodexDocs(outDir, promptOut, inventory);
  console.log(
    `Seeded ${written.length} docs page(s) and Codex prompt ${promptOut}.`,
  );
}

function codexAuditSchemaCommand(): void {
  console.log(JSON.stringify(codexAuditSchema(), null, 2));
}

async function codexAuditVerifyCommand(args: string[]): Promise<void> {
  const parsed = parseArgs(args, {
    string: ["audit"],
    boolean: ["help"],
    alias: { h: "help" },
  });
  if (parsed.help) {
    console.log(HELP);
    return;
  }

  const auditPath = ensureString(parsed.audit, "--audit");
  await verifyCodexAuditFile(auditPath);
  console.log("Codex audit verification passed.");
}

export async function main(argv = Deno.args): Promise<void> {
  const [command, ...args] = argv;
  switch (command) {
    case "collect":
      await collectCommand(args);
      return;
    case "generate":
      await generateCommand(args);
      return;
    case "verify":
      await verifyCommand(args);
      return;
    case "audit":
      await auditCommand(args);
      return;
    case "audit-verify":
      await auditVerifyCommand(args);
      return;
    case "codex-seed":
      await codexSeedCommand(args);
      return;
    case "codex-audit-schema":
      codexAuditSchemaCommand();
      return;
    case "codex-audit-verify":
      await codexAuditVerifyCommand(args);
      return;
    case "--help":
    case "-h":
    case undefined:
      console.log(HELP);
      return;
    default:
      throw new Error(`Unknown docs-pipeline command: ${command}`);
  }
}

if (import.meta.main) {
  try {
    await main();
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    console.error(`[docs-pipeline] ${message}`);
    Deno.exit(1);
  }
}
