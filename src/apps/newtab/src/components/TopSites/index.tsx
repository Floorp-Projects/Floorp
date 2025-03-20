import { useEffect, useState } from "react";
import { TopSitesManager } from "./dataManager.ts";

interface TopSite {
  url: string;
  label?: string;
  title?: string;
  favicon?: string;
  tippyTopIcon?: string;
  smallFavicon: string;
}

export function TopSites() {
  const [sites, setSites] = useState<TopSite[]>([]);

  useEffect(() => {
    const loadTopSites = async () => {
      const manager = TopSitesManager.getInstance();
      const topSites = await manager.getTopSites();
      setSites(topSites.slice(0, 8));
    };

    loadTopSites();
  }, []);

  return (
    <div className="fixed top-4 left-4 bg-white/50 dark:bg-gray-800/50 backdrop-blur-sm rounded-lg shadow-sm p-2">
      <div className="flex gap-3">
        {sites.map((site) => (
          <a
            key={site.url}
            href={site.url}
            className="group flex flex-col items-center w-16 p-2 rounded-lg transition-all duration-200 hover:bg-white/50 dark:hover:bg-gray-700/50"
          >
            <div className="w-8 h-8 rounded-lg overflow-hidden bg-white/80 dark:bg-gray-700/80 flex items-center justify-center transform transition-transform group-hover:scale-110 mb-1">
              {site.favicon
                ? (
                  <img
                    src={site.favicon}
                    alt={site.label}
                    className="w-6 h-6 object-contain"
                  />
                )
                : site.smallFavicon
                ? (
                  <img
                    src={site.smallFavicon}
                    alt={site.label}
                    className="w-6 h-6 object-contain"
                  />
                )
                : (
                  <span className="text-lg font-semibold text-gray-700 dark:text-gray-300">
                    {(site.label || site.title || "?")[0]}
                  </span>
                )}
            </div>
            <span className="text-xs text-center text-gray-700 dark:text-gray-300 line-clamp-2 leading-tight">
              {site.label || site.title}
            </span>
          </a>
        ))}
      </div>
    </div>
  );
}
