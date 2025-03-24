import { FormEvent, useEffect, useRef, useState } from "react";
import { ChevronDown, Search } from "lucide-react";
import {
  getNewTabSettings,
  saveNewTabSettings,
} from "../../utils/dataManager.ts";
import {
  getSearchEngines,
  getDefaultEngine,
  type SearchEngine
} from "./dataManager.ts";

export function SearchBar() {
  const [query,
    setQuery] = useState("");
  const [isDropdownOpen, setIsDropdownOpen] = useState(false);
  const [searchEngines, setSearchEngines] = useState<SearchEngine[]>([]);
  const [selectedEngine, setSelectedEngine] = useState<SearchEngine | null>(null);
  const [loading, setLoading] = useState(true);

  const dropdownRef = useRef<HTMLDivElement>(null);
  const dropdownButtonRef = useRef<HTMLButtonElement>(null);

  useEffect(() => {
    const loadEngines = async () => {
      try {
        setLoading(true);
        const engines = await getSearchEngines();
        setSearchEngines(engines);

        const defaultEngine = await getDefaultEngine();

        const settings = await getNewTabSettings();
        if (settings.searchBar && settings.searchBar.searchEngine !== "default") {
          const savedEngine = engines.find(
            (e) => e.identifier === settings.searchBar.searchEngine
          );
          if (savedEngine) {
            setSelectedEngine(savedEngine);
          } else {
            setSelectedEngine(defaultEngine);
          }
        } else {
          setSelectedEngine(defaultEngine);
        }
      } catch (error) {
        console.error("Failed to load search engines:", error);
        setSelectedEngine({
          identifier: "google",
          name: "Google",
          searchUrl: "https://www.google.com/search?q={searchTerms}",
          iconURL: "https://www.google.com/favicon.ico"
        });
      } finally {
        setLoading(false);
      }
    };

    loadEngines();
  }, []);

  useEffect(() => {
    const handleClickOutside = (event: MouseEvent) => {
      if (
        isDropdownOpen &&
        dropdownRef.current &&
        dropdownButtonRef.current &&
        !dropdownRef.current.contains(event.target as Node) &&
        !dropdownButtonRef.current.contains(event.target as Node)
      ) {
        setIsDropdownOpen(false);
      }
    };

    document.addEventListener("mousedown", handleClickOutside);

    return () => {
      document.removeEventListener("mousedown", handleClickOutside);
    };
  }, [isDropdownOpen]);

  const handleSearch = (e: FormEvent) => {
    console.log("handleSearch");
    e.preventDefault();

    if (!query.trim() || !selectedEngine) return;

    let url = "";

    if (
      query.match(/^https?:\/\//i) ||
      query.match(/^[a-z0-9][-a-z0-9.]+\.[a-z]{2,}(\/.*)?$/i)
    ) {

      if (!query.match(/^https?:\/\//i)) {
        url = `https://${query}`;
      } else {
        url = query;
      }
    } else {
      console.log("selectedEngine", selectedEngine);
      url = selectedEngine.searchUrl + query;
    }
    globalThis.location.href = url;
  };

  const selectSearchEngine = async (engine: SearchEngine) => {
    setSelectedEngine(engine);
    setIsDropdownOpen(false);

    const settings = await getNewTabSettings();
    await saveNewTabSettings({
      ...settings,
      searchBar: {
        ...settings.searchBar,
        searchEngine: engine.identifier,
      },
    });
  };

  if (loading || !selectedEngine) {
    return (
      <div className="fixed top-4 left-1/2 transform -translate-x-1/2 w-full max-w-xl">
        <div className="bg-white/50 dark:bg-gray-800/50 backdrop-blur-sm rounded-lg shadow-sm flex items-center p-2 justify-center">
          <div className="animate-pulse">検索エンジンを読み込み中...</div>
        </div>
      </div>
    );
  }

  return (
    <div className="fixed top-4 left-1/2 transform -translate-x-1/2 w-full max-w-xl">
      <form
        onSubmit={handleSearch}
        className="bg-white/50 dark:bg-gray-800/50 backdrop-blur-sm rounded-lg shadow-sm flex items-center p-2"
      >
        <div className="relative">
          <button
            type="button"
            ref={dropdownButtonRef}
            onClick={() => setIsDropdownOpen(!isDropdownOpen)}
            className="flex items-center gap-1 p-2 hover:bg-gray-100 dark:hover:bg-gray-700 rounded-lg"
          >
            {selectedEngine.iconURL && (
              <img
                src={selectedEngine.iconURL}
                alt={selectedEngine.name}
                className="w-4 h-4"
              />
            )}
            <ChevronDown size={16} className="text-gray-500" />
          </button>

          {isDropdownOpen && (
            <div
              ref={dropdownRef}
              className="absolute top-full left-0 mt-1 bg-white dark:bg-gray-800 rounded-lg shadow-md overflow-hidden z-10 w-48"
            >
              {searchEngines.map((engine) => (
                <button
                  key={engine.identifier}
                  type="button"
                  onClick={() => selectSearchEngine(engine)}
                  className={`flex items-center gap-2 w-full px-3 py-2 text-left hover:bg-gray-100 dark:hover:bg-gray-700 transition-colors ${selectedEngine.identifier === engine.identifier
                    ? "bg-gray-100 dark:bg-gray-700"
                    : ""
                    }`}
                >
                  {engine.iconURL && (
                    <img
                      src={engine.iconURL}
                      alt={engine.name}
                      className="w-4 h-4"
                    />
                  )}
                  <span>{engine.name}</span>
                </button>
              ))}
            </div>
          )}
        </div>

        <input
          type="text"
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          placeholder="検索またはURLを入力"
          className="flex-1 bg-transparent border-none outline-none px-2 py-1 text-gray-900 dark:text-gray-100"
          autoFocus
        />

        <button
          type="submit"
          className="p-2 text-gray-500 hover:text-gray-700 dark:text-gray-400 dark:hover:text-gray-200"
        >
          <Search size={18} />
        </button>
      </form>
    </div>
  );
}
