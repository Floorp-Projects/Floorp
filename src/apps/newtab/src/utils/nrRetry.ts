type RetryOptions<T> = {
  retries?: number;
  timeoutMs?: number;
  delayMs?: number;
  shouldRetry?: (parsed: T) => boolean;
};

function wait(delayMs: number): Promise<void> {
  console.log(`🔍 [NRRetry Debug] Starting wait for ${delayMs}ms`);
  return new Promise((resolve) => {
    setTimeout(() => {
      console.log(`🔍 [NRRetry Debug] Wait for ${delayMs}ms completed`);
      resolve();
    }, delayMs);
  });
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

  console.log("🔍 [NRRetry Debug] Starting callNRWithRetry");
  console.log("🔍 [NRRetry Debug] Options:", {
    retries,
    timeoutMs,
    delayMs,
    hasShouldRetry: !!shouldRetry,
  });

  let attempt = 0;
  // deno-lint-ignore no-explicit-any
  let lastError: any = null;

  while (attempt < retries) {
    console.log(`🔍 [NRRetry Debug] Attempt ${attempt + 1}/${retries}`);
    try {
      const result = await new Promise<T>((resolve, reject) => {
        let settled = false;
        console.log(`🔍 [NRRetry Debug] Setting timeout for ${timeoutMs}ms`);
        const timer = setTimeout(() => {
          if (!settled) {
            console.log("🔍 [NRRetry Debug] Timeout occurred");
            settled = true;
            reject(new Error("NR call timed out"));
          }
        }, timeoutMs);

        try {
          console.log("🔍 [NRRetry Debug] Calling invoker function");
          invoker((data: string) => {
            if (settled) {
              console.log(
                "🔍 [NRRetry Debug] Callback called but already settled",
              );
              return;
            }
            console.log("🔍 [NRRetry Debug] Callback received data:", data);
            try {
              const parsed = parse(data);
              console.log(
                "🔍 [NRRetry Debug] Data parsed successfully:",
                parsed,
              );
              if (shouldRetry && shouldRetry(parsed)) {
                console.log(
                  "🔍 [NRRetry Debug] shouldRetry returned true, triggering retry",
                );
                throw new Error("Parsed result indicates retry condition");
              }
              console.log(
                "🔍 [NRRetry Debug] Success! Resolving with parsed data",
              );
              settled = true;
              clearTimeout(timer);
              resolve(parsed);
            } catch (e) {
              console.log(
                "🔍 [NRRetry Debug] Parse error or retry condition:",
                e,
              );
              settled = true;
              clearTimeout(timer);
              reject(e);
            }
          });
        } catch (e) {
          console.log("🔍 [NRRetry Debug] Invoker function threw error:", e);
          if (!settled) {
            settled = true;
            clearTimeout(timer);
            reject(e);
          }
        }
      });
      console.log(`🔍 [NRRetry Debug] Attempt ${attempt + 1} succeeded`);
      return result;
    } catch (e) {
      console.log(`🔍 [NRRetry Debug] Attempt ${attempt + 1} failed:`, e);
      lastError = e;
      attempt += 1;
      if (attempt >= retries) {
        console.log(`🔍 [NRRetry Debug] Max retries (${retries}) reached`);
        break;
      }
      const waitTime = delayMs * attempt;
      console.log(`🔍 [NRRetry Debug] Waiting ${waitTime}ms before retry`);
      await wait(waitTime);
    }
  }

  console.log(
    "🔍 [NRRetry Debug] All attempts failed, throwing error:",
    lastError,
  );
  throw lastError ?? new Error("NR call failed");
}
