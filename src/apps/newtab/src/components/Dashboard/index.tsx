import { useEffect, useState } from "react";

interface BrowserStats {
  trackerBlockCount: number;
  tabCount: number;
  memoryUsage: number;
  isPrivateMode: boolean;
}

export function Dashboard() {
  const [stats, setStats] = useState<BrowserStats>({
    trackerBlockCount: 0,
    tabCount: 0,
    memoryUsage: 0,
    isPrivateMode: false,
  });

  useEffect(() => {
    // ブラウザーの統計情報を取得・更新する
    const updateStats = async () => {
      try {
        const tabs = 123; // 仮の実装
        const memoryInfo = 123; // 仮の実装
        const isPrivate = true; // 仮の実装

        setStats({
          trackerBlockCount: await getTrackerCount(),
          tabCount: tabs, // Use the mock value directly
          memoryUsage: Math.round(memoryInfo / (1024 * 1024)), // Convert to MB
          isPrivateMode: isPrivate,
        });
      } catch (error) {
        console.error("Failed to update browser stats:", error);
      }
    };

    updateStats();
    const interval = setInterval(updateStats, 5000); // 5秒ごとに更新

    return () => clearInterval(interval);
  }, []);

  return (
    <div className="bg-white/70 dark:bg-gray-800/70 backdrop-blur-md rounded-lg shadow-lg p-4">
      <h2 className="text-lg font-medium text-gray-900 dark:text-white mb-3">
        ブラウザーの状態
      </h2>
      <div className="grid grid-cols-2 gap-4">
        <div className="space-y-2">
          <div className="flex justify-between items-center">
            <span className="text-sm text-gray-600 dark:text-gray-400">
              ブロックされたトラッカー
            </span>
            <span className="text-sm font-medium text-gray-900 dark:text-white">
              {stats.trackerBlockCount}
            </span>
          </div>
          <div className="flex justify-between items-center">
            <span className="text-sm text-gray-600 dark:text-gray-400">
              開いているタブ
            </span>
            <span className="text-sm font-medium text-gray-900 dark:text-white">
              {stats.tabCount}
            </span>
          </div>
        </div>
        <div className="space-y-2">
          <div className="flex justify-between items-center">
            <span className="text-sm text-gray-600 dark:text-gray-400">
              メモリ使用量
            </span>
            <span className="text-sm font-medium text-gray-900 dark:text-white">
              {stats.memoryUsage} MB
            </span>
          </div>
          <div className="flex justify-between items-center">
            <span className="text-sm text-gray-600 dark:text-gray-400">
              プライベートモード
            </span>
            <span className="text-sm font-medium text-gray-900 dark:text-white">
              {stats.isPrivateMode ? "オン" : "オフ"}
            </span>
          </div>
        </div>
      </div>
    </div>
  );
}

// トラッカーのブロック数を取得する関数
// この実装はダミーで、実際にはブラウザーAPIを使用して取得する必要があります
async function getTrackerCount(): Promise<number> {
  return 0; // 仮の実装
}
