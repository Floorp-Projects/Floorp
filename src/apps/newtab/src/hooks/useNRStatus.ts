import { useEffect, useState } from "react";
import { nrStatusManager } from "@/utils/nrStatus.ts";

interface NRStatusHookResult {
  isInitialized: boolean;
  availableFunctions: string[];
  unavailableFunctions: string[];
  timeSinceInit: number;
  checkStatus: () => void;
}

/**
 * NR関数の状態を監視するReactフック
 */
export function useNRStatus(): NRStatusHookResult {
  const [isInitialized, setIsInitialized] = useState(false);
  const [availableFunctions, setAvailableFunctions] = useState<string[]>([]);
  const [unavailableFunctions, setUnavailableFunctions] = useState<string[]>(
    [],
  );
  const [timeSinceInit, setTimeSinceInit] = useState(0);

  const checkStatus = () => {
    const status = nrStatusManager.getAllNRFunctionStatus();
    const available = Object.entries(status)
      .filter(([, isAvailable]) => isAvailable)
      .map(([funcName]) => funcName);
    const unavailable = Object.entries(status)
      .filter(([, isAvailable]) => !isAvailable)
      .map(([funcName]) => funcName);

    setAvailableFunctions(available);
    setUnavailableFunctions(unavailable);
    setTimeSinceInit(nrStatusManager.getTimeSinceInitialization());

    // 重要な関数が利用可能になったら初期化完了とする
    const criticalFunctions = [
      "NRGetCurrentTopSites",
      "NRGetSearchEngines",
      "NRGetDefaultEngine",
    ];

    const criticalAvailable = criticalFunctions.every(
      (funcName) => status[funcName as keyof typeof status],
    );

    setIsInitialized(criticalAvailable);
  };

  useEffect(() => {
    // 初回チェック
    checkStatus();

    // 定期的にチェック（初期化が完了するまで）
    const interval = setInterval(() => {
      checkStatus();

      // 初期化が完了したら、または10秒経過したらチェックを停止
      if (
        isInitialized ||
        nrStatusManager.getTimeSinceInitialization() > 10000
      ) {
        clearInterval(interval);
      }
    }, 500);

    return () => clearInterval(interval);
  }, [isInitialized]);

  return {
    isInitialized,
    availableFunctions,
    unavailableFunctions,
    timeSinceInit,
    checkStatus,
  };
}

/**
 * デバッグ用：NR関数の状態をコンソールに出力するフック
 */
export function useNRStatusDebug(enabled = false): void {
  const {
    isInitialized,
    availableFunctions,
    unavailableFunctions,
    timeSinceInit,
  } = useNRStatus();

  useEffect(() => {
    if (!enabled) return;

    console.group("🔍 NR Functions Debug Info");
    console.log("Initialized:", isInitialized);
    console.log("Time since init:", `${timeSinceInit}ms`);
    console.log("Available functions:", availableFunctions);
    console.log("Unavailable functions:", unavailableFunctions);
    console.groupEnd();
  }, [
    enabled,
    isInitialized,
    availableFunctions,
    unavailableFunctions,
    timeSinceInit,
  ]);
}
