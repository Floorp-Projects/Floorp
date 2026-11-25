// SPDX-License-Identifier: MPL-2.0

import { loadOpenApiChunks } from "./api_context.ts";
import { runExperiment, summarizeExperiments } from "./experiment.ts";
import { fixtureContextChunks, fixtureTasks } from "./fixtures.ts";
import { scriptedSmallModelGenerator } from "./generator.ts";
import { allSelectorNames, selectorByName } from "./selectors.ts";
import type { ExperimentLog } from "./types.ts";

const DEFAULT_OPENAPI_PATH =
  "browser-features/modules/modules/os-server/_os-plugin/api-spec/openapi.yaml";

interface CliOptions {
  out: string;
  selectors: string[];
  tasks: string[];
  includeOpenapi: boolean;
}

function valueAfter(args: string[], index: number, name: string): string {
  const value = args[index + 1];
  if (!value || value.startsWith("--")) {
    throw new Error(`${name} requires a value`);
  }
  return value;
}

export function parseArgs(args: string[]): CliOptions {
  const options: CliOptions = {
    out: "_dist/workflow-research/results.jsonl",
    selectors: allSelectorNames(),
    tasks: [],
    includeOpenapi: false,
  };
  for (let index = 0; index < args.length; index++) {
    const arg = args[index];
    if (arg === "run") continue;
    if (arg === "--out") {
      options.out = valueAfter(args, index, arg);
      index++;
    } else if (arg === "--selectors") {
      options.selectors = valueAfter(args, index, arg).split(",").map((entry) =>
        entry.trim()
      ).filter(Boolean);
      index++;
    } else if (arg === "--tasks") {
      options.tasks = valueAfter(args, index, arg).split(",").map((entry) =>
        entry.trim()
      ).filter(Boolean);
      index++;
    } else if (arg === "--include-openapi") {
      options.includeOpenapi = true;
    } else {
      throw new Error(`unknown argument: ${arg}`);
    }
  }
  return options;
}

export async function runCli(args: string[]): Promise<string> {
  const options = parseArgs(args);
  const tasks = fixtureTasks().filter((task) =>
    options.tasks.length === 0 || options.tasks.includes(task.id)
  );
  if (tasks.length === 0) throw new Error("no fixture tasks selected");

  const chunks = fixtureContextChunks();
  if (options.includeOpenapi) {
    chunks.push(...await loadOpenApiChunks(DEFAULT_OPENAPI_PATH));
  }

  const logs: ExperimentLog[] = [];
  for (const task of tasks) {
    for (const selectorName of options.selectors) {
      logs.push(
        await runExperiment(
          task,
          chunks,
          selectorByName(selectorName),
          scriptedSmallModelGenerator,
        ),
      );
    }
  }

  const directory = options.out.split("/").slice(0, -1).join("/");
  if (directory) await Deno.mkdir(directory, { recursive: true });
  await Deno.writeTextFile(
    options.out,
    logs.map((log) => JSON.stringify(log)).join("\n") + "\n",
  );
  const summary = summarizeExperiments(logs);
  console.log(summary);
  console.log(`\nwrote ${logs.length} experiment logs to ${options.out}`);
  return summary;
}

if (import.meta.main) {
  await runCli(Deno.args);
}
