type RetryOptions<T> = {
  retries?: number;
  timeoutMs?: number;
  delayMs?: number;
  shouldRetry?: (parsed: T) => boolean;
};

function wait(delayMs: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, delayMs));
}

export async function callNRWithRetry<T>(
  invoker: (callback: (data: string) => void) => void,
  parse: (data: string) => T,
  options: RetryOptions<T> = {},
): Promise<T> {
  const {
    retries = 3,
    timeoutMs = 800,
    delayMs = 300,
    shouldRetry,
  } = options;

  let attempt = 0;
  // deno-lint-ignore no-explicit-any
  let lastError: any = null;

  while (attempt < retries) {
    try {
      const result = await new Promise<T>((resolve, reject) => {
        let settled = false;
        const timer = setTimeout(() => {
          if (!settled) {
            settled = true;
            reject(new Error("NR call timed out"));
          }
        }, timeoutMs);

        try {
          invoker((data: string) => {
            if (settled) return;
            try {
              const parsed = parse(data);
              if (shouldRetry && shouldRetry(parsed)) {
                throw new Error("Parsed result indicates retry condition");
              }
              settled = true;
              clearTimeout(timer);
              resolve(parsed);
            } catch (e) {
              settled = true;
              clearTimeout(timer);
              reject(e);
            }
          });
        } catch (e) {
          if (!settled) {
            settled = true;
            clearTimeout(timer);
            reject(e);
          }
        }
      });
      return result;
    } catch (e) {
      lastError = e;
      attempt += 1;
      if (attempt >= retries) break;
      await wait(delayMs * attempt);
    }
  }

  throw lastError ?? new Error("NR call failed");
}
