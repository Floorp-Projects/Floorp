/**
 * Custom error classes for the Noraneko build system
 * Provides structured error handling with context and suggestions
 */

import type { BuildPhase, Platform } from "./types.ts";

/**
 * Base error class for build system errors
 */
export abstract class BuildError extends Error {
  /**
   * Error code for programmatic handling
   */
  public readonly code: string;

  /**
   * Build phase where the error occurred
   */
  public readonly phase?: BuildPhase;

  /**
   * Suggested actions to resolve the error
   */
  public readonly suggestions: string[];

  /**
   * Additional context information
   */
  public readonly context: Record<string, unknown>;

  constructor(
    message: string,
    code: string,
    phase?: BuildPhase,
    suggestions: string[] = [],
    context: Record<string, unknown> = {},
  ) {
    super(message);
    this.name = this.constructor.name;
    this.code = code;
    this.phase = phase;
    this.suggestions = suggestions;
    this.context = context;

    // Ensure proper prototype chain for instanceof checks
    Object.setPrototypeOf(this, new.target.prototype);
  }

  /**
   * Format error for display
   */
  public format(): string {
    let output = `❌ ${this.name}: ${this.message}`;

    if (this.phase) {
      output += `\n   Phase: ${this.phase}`;
    }

    if (this.code) {
      output += `\n   Code: ${this.code}`;
    }

    if (this.suggestions.length > 0) {
      output += `\n   Suggestions:`;
      for (const suggestion of this.suggestions) {
        output += `\n     • ${suggestion}`;
      }
    }

    return output;
  }
}

/**
 * Error for configuration-related issues
 */
export class ConfigurationError extends BuildError {
  constructor(
    message: string,
    code: string = "CONFIG_ERROR",
    suggestions: string[] = [],
    context: Record<string, unknown> = {},
  ) {
    super(message, code, undefined, suggestions, context);
  }
}

/**
 * Error for file system operations
 */
export class FileSystemError extends BuildError {
  public readonly path: string;
  public readonly operation: string;

  constructor(
    message: string,
    path: string,
    operation: string,
    code: string = "FS_ERROR",
    phase?: BuildPhase,
    suggestions: string[] = [],
    context: Record<string, unknown> = {},
  ) {
    super(message, code, phase, suggestions, { ...context, path, operation });
    this.path = path;
    this.operation = operation;
  }

  public format(): string {
    let output = super.format();
    output += `\n   Path: ${this.path}`;
    output += `\n   Operation: ${this.operation}`;
    return output;
  }
}

/**
 * Error for command execution failures
 */
export class CommandError extends BuildError {
  public readonly command: string;
  public readonly args: string[];
  public readonly exitCode: number;
  public readonly stdout: string;
  public readonly stderr: string;

  constructor(
    message: string,
    command: string,
    args: string[],
    exitCode: number,
    stdout: string,
    stderr: string,
    code: string = "COMMAND_ERROR",
    phase?: BuildPhase,
    suggestions: string[] = [],
    context: Record<string, unknown> = {},
  ) {
    super(message, code, phase, suggestions, {
      ...context,
      command,
      args,
      exitCode,
      stdout,
      stderr,
    });
    this.command = command;
    this.args = args;
    this.exitCode = exitCode;
    this.stdout = stdout;
    this.stderr = stderr;
  }

  public format(): string {
    let output = super.format();
    output += `\n   Command: ${this.command} ${this.args.join(" ")}`;
    output += `\n   Exit Code: ${this.exitCode}`;

    if (this.stderr.trim()) {
      output += `\n   Error Output: ${this.stderr.trim()}`;
    }

    return output;
  }
}

/**
 * Error for platform-specific issues
 */
export class PlatformError extends BuildError {
  public readonly platform: Platform;
  public readonly architecture?: string;

  constructor(
    message: string,
    context: { platform: Platform; architecture?: string },
    suggestion?: string,
    code: string = "PLATFORM_ERROR",
    phase?: BuildPhase,
  ) {
    super(message, code, phase, suggestion ? [suggestion] : [], context);
    this.platform = context.platform;
    this.architecture = context.architecture;
  }
}

/**
 * Error for validation failures
 */
export class ValidationError extends BuildError {
  public readonly validationErrors: string[];
  public readonly warnings: string[];

  constructor(
    message: string,
    validationErrors: string[],
    warnings: string[] = [],
    code: string = "VALIDATION_ERROR",
    suggestions: string[] = [],
    context: Record<string, unknown> = {},
  ) {
    super(message, code, undefined, suggestions, {
      ...context,
      validationErrors,
      warnings,
    });
    this.validationErrors = validationErrors;
    this.warnings = warnings;
  }

  public format(): string {
    let output = super.format();

    if (this.validationErrors.length > 0) {
      output += `\n   Validation Errors:`;
      for (const error of this.validationErrors) {
        output += `\n     • ${error}`;
      }
    }

    if (this.warnings.length > 0) {
      output += `\n   Warnings:`;
      for (const warning of this.warnings) {
        output += `\n     • ${warning}`;
      }
    }

    return output;
  }
}

/**
 * Error for dependency-related issues
 */
export class DependencyError extends BuildError {
  public readonly dependency: string;
  public readonly requiredVersion?: string;
  public readonly currentVersion?: string;

  constructor(
    message: string,
    dependency: string,
    requiredVersion?: string,
    currentVersion?: string,
    code: string = "DEPENDENCY_ERROR",
    suggestions: string[] = [],
    context: Record<string, unknown> = {},
  ) {
    super(message, code, undefined, suggestions, {
      ...context,
      dependency,
      requiredVersion,
      currentVersion,
    });
    this.dependency = dependency;
    this.requiredVersion = requiredVersion;
    this.currentVersion = currentVersion;
  }

  public format(): string {
    let output = super.format();
    output += `\n   Dependency: ${this.dependency}`;

    if (this.requiredVersion) {
      output += `\n   Required Version: ${this.requiredVersion}`;
    }

    if (this.currentVersion) {
      output += `\n   Current Version: ${this.currentVersion}`;
    }

    return output;
  }
}

/**
 * Type guard to check if an error is a BuildError
 */
export function isBuildError(error: unknown): error is BuildError {
  return error instanceof BuildError;
}

/**
 * Format any error for display
 */
export function formatError(error: unknown): string {
  if (isBuildError(error)) {
    return error.format();
  }

  if (error instanceof Error) {
    return `❌ ${error.name}: ${error.message}`;
  }

  return `❌ Unknown error: ${String(error)}`;
}

/**
 * Error handler utility for consistent error processing
 */
export class ErrorHandler {
  private logger: { error: (message: string) => void };

  constructor(logger: { error: (message: string) => void }) {
    this.logger = logger;
  }

  /**
   * Handle an error with proper formatting and logging
   */
  handle(error: unknown, exitOnError: boolean = false): void {
    const formattedError = formatError(error);
    this.logger.error(formattedError);

    if (exitOnError) {
      Deno.exit(1);
    }
  }

  /**
   * Wrap an async function with error handling
   */
  wrap<T extends unknown[], R>(
    fn: (...args: T) => Promise<R>,
    exitOnError: boolean = false,
  ): (...args: T) => Promise<R | void> {
    return async (...args: T): Promise<R | void> => {
      try {
        return await fn(...args);
      } catch (error) {
        this.handle(error, exitOnError);
      }
    };
  }
}
