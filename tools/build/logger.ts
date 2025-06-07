import chalk from "chalk";

class Logger {
  constructor(private prefix: string) {}

  private format(level: string, message: string): string {
    return `[${this.prefix}] ${level}: ${message}`;
  }

  info(message: string, ...args: unknown[]): void {
    console.log(chalk.blue(this.format("INFO", message)), ...args);
  }

  warn(message: string, ...args: unknown[]): void {
    console.warn(chalk.yellow(this.format("WARN", message)), ...args);
  }

  error(message: string, ...args: unknown[]): void {
    console.error(chalk.red(this.format("ERROR", message)), ...args);
  }

  success(message: string, ...args: unknown[]): void {
    console.log(chalk.green(this.format("SUCCESS", message)), ...args);
  }

  debug(message: string, ...args: unknown[]): void {
    if (Deno.env.get("DEBUG")) {
      console.debug(chalk.gray(this.format("DEBUG", message)), ...args);
    }
  }
}

export const log = new Logger("build");

export function createLogger(prefix: string): Logger {
  return new Logger(prefix);
}
