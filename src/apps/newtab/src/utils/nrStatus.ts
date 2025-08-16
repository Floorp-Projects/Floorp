/**
 * NR関数の状態を監視し、利用可能性をチェックするユーティリティ
 */

type NRFunctionName =
  | "NRGetCurrentTopSites"
  | "NRGetSearchEngines"
  | "NRGetDefaultEngine"
  | "NRGetDefaultPrivateEngine"
  | "NRGetSuggestions"
  | "NRGetFolderPathFromDialog"
  | "NRGetRandomImageFromFolder"
  | "NRSettingsSend"
  | "NRSettingsRegisterReceiveCallback";

interface NRStatusInfo {
  isAvailable: boolean;
  lastChecked: number;
  initializationStartTime: number;
}

class NRStatusManager {
  private statusMap = new Map<NRFunctionName, NRStatusInfo>();
  private readonly CACHE_DURATION = 5000; // 5秒間キャッシュ
  private initializationStartTime = Date.now();

  /**
   * NR関数が利用可能かチェック（キャッシュあり）
   */
  public isNRFunctionAvailable(functionName: NRFunctionName): boolean {
    const now = Date.now();
    const status = this.statusMap.get(functionName);

    // キャッシュが有効な場合はそれを使用
    if (status && now - status.lastChecked < this.CACHE_DURATION) {
      return status.isAvailable;
    }

    // 実際にチェック
    const windowObj = globalThis as unknown as Window;
    const isAvailable =
      typeof windowObj[functionName as keyof Window] === "function";

    // キャッシュを更新
    this.statusMap.set(functionName, {
      isAvailable,
      lastChecked: now,
      initializationStartTime: status?.initializationStartTime ||
        this.initializationStartTime,
    });

    return isAvailable;
  }

  /**
   * 全てのNR関数の状態を取得
   */
  public getAllNRFunctionStatus(): Record<NRFunctionName, boolean> {
    const allFunctions: NRFunctionName[] = [
      "NRGetCurrentTopSites",
      "NRGetSearchEngines",
      "NRGetDefaultEngine",
      "NRGetDefaultPrivateEngine",
      "NRGetSuggestions",
      "NRGetFolderPathFromDialog",
      "NRGetRandomImageFromFolder",
      "NRSettingsSend",
      "NRSettingsRegisterReceiveCallback",
    ];

    const status: Partial<Record<NRFunctionName, boolean>> = {};
    for (const funcName of allFunctions) {
      status[funcName] = this.isNRFunctionAvailable(funcName);
    }

    return status as Record<NRFunctionName, boolean>;
  }

  /**
   * NR関数の初期化を待機
   */
  public async waitForNRFunction(
    functionName: NRFunctionName,
    maxWaitMs = 5000,
    checkIntervalMs = 100,
  ): Promise<boolean> {
    const startTime = Date.now();

    while (Date.now() - startTime < maxWaitMs) {
      if (this.isNRFunctionAvailable(functionName)) {
        return true;
      }
      await new Promise((resolve) => setTimeout(resolve, checkIntervalMs));
    }

    return false;
  }

  /**
   * 初期化からの経過時間を取得
   */
  public getTimeSinceInitialization(): number {
    return Date.now() - this.initializationStartTime;
  }

  /**
   * デバッグ情報を出力
   */
  public logDebugInfo(): void {
    const status = this.getAllNRFunctionStatus();
    const timeSinceInit = this.getTimeSinceInitialization();

    console.group("NR Functions Status");
    console.log(`Time since initialization: ${timeSinceInit}ms`);
    console.table(status);
    console.groupEnd();
  }

  /**
   * キャッシュをクリア
   */
  public clearCache(): void {
    this.statusMap.clear();
  }
}

// シングルトンインスタンス
export const nrStatusManager = new NRStatusManager();

/**
 * NR関数が利用可能かチェック（便利関数）
 */
export function isNRFunctionAvailable(functionName: NRFunctionName): boolean {
  return nrStatusManager.isNRFunctionAvailable(functionName);
}

/**
 * NR関数の初期化を待機（便利関数）
 */
export async function waitForNRFunction(
  functionName: NRFunctionName,
  maxWaitMs?: number,
  checkIntervalMs?: number,
): Promise<boolean> {
  return await nrStatusManager.waitForNRFunction(
    functionName,
    maxWaitMs,
    checkIntervalMs,
  );
}

/**
 * 全てのNR関数の状態を取得（便利関数）
 */
export function getAllNRFunctionStatus(): Record<NRFunctionName, boolean> {
  return nrStatusManager.getAllNRFunctionStatus();
}
