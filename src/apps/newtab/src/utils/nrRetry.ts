type RetryOptions<T> = {
  retries?: number;
  timeoutMs?: number;
  delayMs?: number;
  shouldRetry?: (parsed: T) => boolean;
  functionName?: string; // デバッグ用の関数名
  checkFunctionExists?: boolean; // 関数の存在確認を行うかどうか
  initializationDelayMs?: number; // 初期化待機時間
};

function wait(delayMs: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, delayMs));
}

/**
 * NR関数が存在するかチェック
 */
function checkNRFunctionExists(functionName: string): boolean {
  const windowObj = globalThis as unknown as Window;
  return typeof windowObj[functionName as keyof Window] === "function";
}

/**
 * NR関数の初期化を待機
 */
async function waitForNRFunction(
  functionName: string,
  maxWaitMs = 3000,
  checkIntervalMs = 100,
): Promise<boolean> {
  const startTime = Date.now();

  while (Date.now() - startTime < maxWaitMs) {
    if (checkNRFunctionExists(functionName)) {
      return true;
    }
    await wait(checkIntervalMs);
  }

  return false;
}

export async function callNRWithRetry<T>(
  invoker: (callback: (data: string) => void) => void,
  parse: (data: string) => T,
  options: RetryOptions<T> = {},
): Promise<T> {
  const {
    retries = 3,
    timeoutMs = 1200,
    delayMs = 300,
    shouldRetry,
    functionName,
    checkFunctionExists = true,
    initializationDelayMs = 3000,
  } = options;

  // 関数の存在確認と初期化待機
  if (checkFunctionExists && functionName) {
    const functionExists = await waitForNRFunction(
      functionName,
      initializationDelayMs,
    );
    if (!functionExists) {
      console.warn(
        `NR function ${functionName} not found after waiting ${initializationDelayMs}ms`,
      );
    }
  }

  let attempt = 0;
  // deno-lint-ignore no-explicit-any
  let lastError: any = null;

  while (attempt < retries) {
    try {
      // 関数の存在を再確認
      if (functionName && !checkNRFunctionExists(functionName)) {
        throw new Error(`NR function ${functionName} is not available`);
      }

      const result = await new Promise<T>((resolve, reject) => {
        let settled = false;
        const timer = setTimeout(() => {
          if (!settled) {
            settled = true;
            reject(
              new Error(`NR call timed out (${functionName || "unknown"})`),
            );
          }
        }, timeoutMs);

        try {
          invoker((data: string) => {
            if (settled) return;
            try {
              // 空のレスポンスや無効なJSONをチェック
              if (!data || data.trim() === "") {
                throw new Error("Empty response from NR function");
              }

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

      if (functionName) {
        console.warn(`NR call ${functionName} attempt ${attempt} failed:`, e);
      }

      if (attempt >= retries) break;

      // 指数バックオフを使用（最大2秒まで）
      const backoffDelay = Math.min(delayMs * Math.pow(2, attempt - 1), 2000);
      await wait(backoffDelay);
    }
  }

  const errorMessage = functionName
    ? `NR call ${functionName} failed after ${retries} retries`
    : `NR call failed after ${retries} retries`;

  throw lastError ?? new Error(errorMessage);
}
