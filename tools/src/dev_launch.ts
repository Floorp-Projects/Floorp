// SPDX-License-Identifier: MPL-2.0

export function nativeDevLogTarget(platform: string): string {
  return platform === "windows" ? "NUL" : "/dev/null";
}

export async function startDevBrowser(
  startServers: () => Promise<void>,
  launch: (onStarted: () => void) => Promise<void>,
  shutdown: () => void,
  onExit: () => void,
  onFailure: (error: unknown) => void,
): Promise<void> {
  let started = false;
  const ready = Promise.withResolvers<void>();
  try {
    await startServers();
    void launch(() => {
      started = true;
      ready.resolve();
    }).then(() => {
      if (started) onExit();
      else {ready.reject(
          new Error("Browser exited before startup registration"),
        );}
    }, (error: unknown) => {
      if (started) {
        shutdown();
        onFailure(error);
      } else ready.reject(error);
    });
    await ready.promise;
  } catch (error) {
    shutdown();
    throw error;
  }
}
