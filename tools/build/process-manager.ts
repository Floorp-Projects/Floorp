/**
 * Process Manager - Handles browser and dev server processes
 */

import { log } from "./logger.ts";

export class ProcessManager {
  private devProcess: Deno.ChildProcess | null = null;
  private buildProcess: Deno.ChildProcess | null = null;
  private browserProcess: Deno.ChildProcess | null = null;

  setDevProcess(process: Deno.ChildProcess): void {
    this.devProcess = process;
  }

  setBuildProcess(process: Deno.ChildProcess): void {
    this.buildProcess = process;
  }

  setBrowserProcess(process: Deno.ChildProcess): void {
    this.browserProcess = process;
  }

  async launchBrowser(): Promise<void> {
    log.info("🚀 Launching browser...");

    const command = new Deno.Command(Deno.execPath(), {
      args: ["-A", "./tools/dev/launchDev/child-browser.ts"],
      stdin: "piped",
      stdout: "inherit",
      stderr: "inherit",
    });

    this.browserProcess = command.spawn();
    this.setupBrowserMonitoring();
    this.setupSignalHandlers();
  }

  private setupBrowserMonitoring(): void {
    if (!this.browserProcess) return;

    (async () => {
      try {
        const status = await this.browserProcess!.status;
        log.info("🔄 Browser process ended");

        if (!status.success || status.code === 0) {
          log.info("🛑 Browser closed, shutting down development servers...");
          await this.shutdownAll();
          Deno.exit(0);
        }
      } catch (error) {
        log.error("💥 Browser process error:", error);
        await this.shutdownAll();
        Deno.exit(1);
      }
    })();
  }

  private setupSignalHandlers(): void {
    const handleShutdown = async () => {
      log.info("🛑 Shutting down processes...");
      await this.shutdownAll();
      Deno.exit(0);
    };

    try {
      Deno.addSignalListener("SIGINT", handleShutdown);
      if (Deno.build.os !== "windows") {
        Deno.addSignalListener("SIGTERM", handleShutdown);
      }
    } catch (error) {
      log.warn("Could not register signal listeners:", error);
    }
  }

  async shutdownAll(): Promise<void> {
    const promises: Promise<void>[] = [];

    if (this.browserProcess) {
      promises.push(this.shutdownBrowser());
    }

    if (this.devProcess) {
      promises.push(this.shutdownDev());
    }

    if (this.buildProcess) {
      promises.push(this.shutdownBuild());
    }

    if (promises.length > 0) {
      await Promise.allSettled(promises);
    }
  }

  private async shutdownBrowser(): Promise<void> {
    if (!this.browserProcess) return;

    try {
      const writer = this.browserProcess.stdin.getWriter();
      await writer.write(new TextEncoder().encode("q"));
      writer.releaseLock();
    } catch (error) {
      log.warn("Error stopping browser:", error);
    }
    this.browserProcess = null;
  }

  private async shutdownDev(): Promise<void> {
    if (!this.devProcess) return;

    log.info("🔴 Stopping dev server...");
    try {
      const writer = this.devProcess.stdin.getWriter();
      await writer.write(new TextEncoder().encode("q"));
      writer.releaseLock();
      await this.devProcess.status;
    } catch (error) {
      log.warn("Error stopping dev server:", error);
      try {
        this.devProcess.kill("SIGTERM");
      } catch (killError) {
        log.warn("Error killing dev server:", killError);
      }
    }
    this.devProcess = null;
  }

  private async shutdownBuild(): Promise<void> {
    if (!this.buildProcess) return;

    log.info("🔴 Stopping build server...");
    try {
      this.buildProcess.kill("SIGTERM");
      await this.buildProcess.status;
    } catch (error) {
      log.warn("Error stopping build server:", error);
    }
    this.buildProcess = null;
  }
}
