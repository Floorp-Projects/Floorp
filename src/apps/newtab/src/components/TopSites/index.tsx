import { useEffect, useState } from "react";
import { TopSitesManager } from "./dataManager.ts";

interface TopSite {
  url: string;
  title: string;
  favicon?: string;
}

export function TopSites() {
  const [sites, setSites] = useState<TopSite[]>([]);

  useEffect(() => {
    const loadTopSites = async () => {
      const manager = TopSitesManager.getInstance();
      const topSites = await manager.getTopSites();
      setSites(topSites);
    };

    loadTopSites();
  }, []);

  return (
    <div className="bg-white/70 dark:bg-gray-800/70 backdrop-blur-md rounded-lg shadow-lg p-4">
      <h2 className="text-lg font-medium text-gray-900 dark:text-white mb-4">
        よく訪れるサイト
      </h2>
      <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-6 gap-4">
        {sites.map((site) => (
          <a
            key={site.url}
            href={site.url}
            className="group flex flex-col items-center p-3 rounded-lg transition-all duration-200 hover:bg-white/50 dark:hover:bg-gray-700/50"
          >
            <div className="w-12 h-12 mb-2 rounded-lg overflow-hidden bg-white dark:bg-gray-700 flex items-center justify-center transform transition-transform group-hover:scale-110">
              {site.favicon
                ? (
                  <img
                    src={site.favicon}
                    alt={site.title}
                    className="w-8 h-8 object-contain"
                  />
                )
                : (
                  <div className="w-8 h-8 bg-gray-200 dark:bg-gray-600 rounded-full" />
                )}
            </div>
            <span className="text-sm text-center truncate text-gray-700 dark:text-gray-300 w-full">
              {site.title}
            </span>
          </a>
        ))}
      </div>
    </div>
  );
}
