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
    const interval = setInterval(updateStats, 5000);
    return () => clearInterval(interval);
  }, []);

  const stats_items = [
    { label: "トラッカー", value: stats.trackerBlockCount, unit: "件" },
    { label: "タブ", value: stats.tabCount, unit: "個" },
    { label: "メモリ", value: stats.memoryUsage, unit: "MB" },
    { label: "プライベート", value: stats.isPrivateMode ? "オン" : "オフ" },
  ];

  return (
    <div className="bg-white/50 dark:bg-gray-800/50 backdrop-blur-sm rounded-lg shadow-sm">
      <div className="grid grid-cols-2 gap-px bg-gray-200/20 dark:bg-gray-700/20 rounded-lg overflow-hidden">
        {stats_items.map((item, index) => (
          <div
            key={index}
            className="flex flex-col items-center bg-white/50 dark:bg-gray-800/50 p-3 hover:bg-white/60 dark:hover:bg-gray-700/60 transition-colors"
          >
            <div className="text-sm font-medium text-gray-900 dark:text-white">
              {item.value}
              {item.unit}
            </div>
            <div className="text-xs text-gray-500 dark:text-gray-400">
              {item.label}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

async function getTrackerCount(): Promise<number> {
  return 0;
}
