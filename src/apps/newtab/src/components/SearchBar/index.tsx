import { useTranslation } from "react-i18next";
import { Search } from "lucide-react";

declare global {
  var NRFocusUrlBar: (() => void) | undefined;
}

export function SearchBar() {
  const { t } = useTranslation();

  const handleClick = () => {
    if (globalThis.NRFocusUrlBar) {
      globalThis.NRFocusUrlBar();
    }
  };

  return (
    <div className="relative">
      <div
        className="bg-gray-700 backdrop-blur-sm rounded-lg shadow-sm flex items-center p-2 cursor-pointer hover:bg-gray-600 transition-colors"
        onClick={handleClick}
      >
        <div className="relative flex-1">
          <input
            type="text"
            placeholder={t("searchBar.searchOrEnterUrl")}
            className="w-full bg-transparent border-none outline-none px-2 py-1 text-gray-100 cursor-pointer"
            readOnly
          />
        </div>

        <div className="p-2 text-gray-400">
          <Search size={18} />
        </div>
      </div>
    </div>
  );
}
