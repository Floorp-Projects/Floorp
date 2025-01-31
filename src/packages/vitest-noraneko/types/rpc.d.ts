import type { SerializedConfig } from "npm:vitest@^3.0.4";

export type RunnerUID = string;

export interface ClientFunctions {
  registerNoraRunner(): RunnerUID;
}

export interface ServerFunctions {
}

export interface MiddlewareFunctions {
  registerNoraRunner(): Promise<RunnerUID>;
}

export interface NoraRunner {
  runTest(filepath: string): Promise<void>;
}
