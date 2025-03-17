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
      <div className="grid grid-cols-4 gap-4">
        {sites.map((site) => (
          <a
            key={site.url}
            href={site.url}
            className="flex flex-col items-center p-3 rounded-lg transition-all duration-200 hover:bg-white/50 dark:hover:bg-gray-700/50"
          >
            <div className="w-8 h-8 mb-2">
              {site.favicon
                ? (
                  <img
                    src={site.favicon}
                    alt={site.title}
                    className="w-full h-full object-contain"
                  />
                )
                : (
                  <div className="w-full h-full bg-gray-200 dark:bg-gray-600 rounded-full" />
                )}
            </div>
            <span className="text-sm text-center truncate text-gray-700 dark:text-gray-300">
              {site.title}
            </span>
          </a>
        ))}
      </div>
    </div>
  );
}
