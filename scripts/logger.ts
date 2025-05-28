import chalk from "chalk";

enum LogLevel {
  INFO = "INFO",
  WARN = "WARN",
  ERROR = "ERROR",
}

class Logger {
  private prefix: string;

  constructor(prefix: string) {
    this.prefix = `[${prefix}]`;
  }

  private log(level: LogLevel, message: string, ...args: unknown[]) {
    const timestamp = new Date().toISOString();
    let formattedMessage = `${timestamp} ${this.prefix} ${level}: ${message}`;

    switch (level) {
      case LogLevel.INFO:
        console.log(chalk.blue(formattedMessage), ...args);
        break;
      case LogLevel.WARN:
        console.warn(chalk.yellow(formattedMessage), ...args);
        break;
      case LogLevel.ERROR:
        console.error(chalk.red(formattedMessage), ...args);
        break;
      default:
        console.log(formattedMessage, ...args);
    }
  }

  info(message: string, ...args: unknown[]) {
    this.log(LogLevel.INFO, message, ...args);
  }

  warn(message: string, ...args: unknown[]) {
    this.log(LogLevel.WARN, message, ...args);
  }

  error(message: string, ...args: unknown[]) {
    this.log(LogLevel.ERROR, message, ...args);
  }
}

export const log = new Logger("build");
