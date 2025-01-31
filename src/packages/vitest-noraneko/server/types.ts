import { SerializedConfig } from "vitest";

/**
 * Server
 */
export interface WSPoolFunctions {

}

/**
 * Client
 */
export interface WSRunnerFunctions {
  registerConfig(config:SerializedConfig): void;
}