import { useEffect, useRef, useState } from "react";
import { TopSitesManager } from "./dataManager.ts";
import { useTranslation } from "react-i18next";

interface CustomTopSite {
  url: string;
  label: string;
  title?: string;
  favicon?: string;
  tippyTopIcon?: string;
  smallFavicon: string;
}

export function TopSites() {
  const { t } = useTranslation();
  const [sites, setSites] = useState<CustomTopSite[]>([]);
  const [userAddedSites, setUserAddedSites] = useState<CustomTopSite[]>([]);
  const [isModalOpen, setIsModalOpen] = useState(false);
  const [newSite, setNewSite] = useState({
    url: "",
    label: "",
    smallFavicon: "",
  });
  const overlayRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const manager = TopSitesManager.getInstance();

    const savedUserAddedSites = manager.getUserAddedSites().map((site) => ({
      ...site,
      label: site.label || "",
      smallFavicon: site.smallFavicon || "",
    }));
    setUserAddedSites(savedUserAddedSites);

    const loadTopSites = async () => {
      const topSites = await manager.getTopSites();
      const remainingSlots = 8 - savedUserAddedSites.length;
      setSites(
        topSites.slice(0, remainingSlots).map((site) => ({
          ...site,
          smallFavicon: site.smallFavicon || "",
        })),
      );
    };

    loadTopSites();
  }, []);

  useEffect(() => {
    const handleEscape = (e: KeyboardEvent) => {
      if (e.key === "Escape" && isModalOpen) {
        setIsModalOpen(false);
      }
    };

    document.addEventListener("keydown", handleEscape);
    return () => document.removeEventListener("keydown", handleEscape);
  }, [isModalOpen]);

  const handleAddSite = () => {
    if (newSite.url && newSite.label) {
      const updatedUserAddedSites = [{
        ...newSite,
        label: newSite.label || "",
        smallFavicon: newSite.smallFavicon || "",
      }, ...userAddedSites] as CustomTopSite[];
      setUserAddedSites(updatedUserAddedSites);

      const manager = TopSitesManager.getInstance();
      manager.saveUserAddedSites(updatedUserAddedSites);

      setNewSite({ url: "", label: "", smallFavicon: "" });
      setIsModalOpen(false);
    }
  };

  const handleOverlayClick = (e: React.MouseEvent) => {
    if (e.target === overlayRef.current) {
      setIsModalOpen(false);
    }
  };

  return (
    <>
      {isModalOpen && (
        <div
          className="modal modal-open"
          ref={overlayRef}
          onClick={handleOverlayClick}
        >
          <div className="modal-box">
            <div className="flex justify-between items-center">
              <h3 className="font-bold text-lg">{t("topSites.addNewSite")}</h3>
              <button
                type="button"
                onClick={() => setIsModalOpen(false)}
                className="btn btn-sm btn-circle btn-ghost"
              >
                <span className="sr-only">{t("topSites.close")}</span>
                <svg
                  className="h-5 w-5"
                  fill="none"
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  strokeWidth="2"
                  viewBox="0 0 24 24"
                  stroke="currentColor"
                >
                  <path d="M6 18L18 6M6 6l12 12" />
                </svg>
              </button>
            </div>
            <div className="py-4">
              <div className="form-control w-full">
                <label className="label">
                  <span className="label-text">{t("topSites.url")}</span>
                </label>
                <input
                  type="text"
                  placeholder="https://floorp.app"
                  value={newSite.url}
                  onChange={(e) =>
                    setNewSite({ ...newSite, url: e.target.value })}
                  className="input input-bordered w-full"
                />
              </div>
              <div className="form-control w-full mt-2">
                <label className="label">
                  <span className="label-text">{t("topSites.label")}</span>
                </label>
                <input
                  type="text"
                  placeholder={t("topSites.labelPlaceholder")}
                  value={newSite.label}
                  onChange={(e) =>
                    setNewSite({ ...newSite, label: e.target.value })}
                  className="input input-bordered w-full"
                />
              </div>
            </div>
            <div className="modal-action">
              <button
                type="button"
                onClick={() => setIsModalOpen(false)}
                className="btn btn-ghost"
              >
                {t("topSites.cancel")}
              </button>
              <button
                type="button"
                onClick={handleAddSite}
                className="btn btn-primary"
              >
                {t("topSites.add")}
              </button>
            </div>
          </div>
        </div>
      )}
      <div className="fixed top-4 left-4 bg-white/50 dark:bg-gray-800/50 backdrop-blur-sm rounded-lg shadow-sm p-2">
        <div className="flex gap-3">
          {[...userAddedSites, ...sites].map((site, index) => (
            <a
              key={index}
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
          <button
            type="button"
            onClick={() => setIsModalOpen(true)}
            className="group flex flex-col items-center w-16 p-2 rounded-lg transition-all duration-200 hover:bg-white/50 dark:hover:bg-gray-700/50"
          >
            <div className="w-8 h-8 rounded-lg overflow-hidden bg-gray-300 dark:bg-gray-600 flex items-center justify-center transform transition-transform group-hover:scale-110 mb-1">
              <span className="text-lg font-semibold text-gray-700 dark:text-gray-300">
                +
              </span>
            </div>
            <span className="text-xs text-center text-gray-700 dark:text-gray-300 line-clamp-2 leading-tight">
              {t("topSites.addSite")}
            </span>
          </button>
        </div>
      </div>
    </>
  );
}
