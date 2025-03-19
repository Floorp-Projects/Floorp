import { FormEvent, useEffect, useRef, useState } from "react";
import { ChevronDown, Search } from "lucide-react";
import {
  getNewTabSettings,
  saveNewTabSettings,
} from "../../utils/dataManager.ts";

interface SearchEngine {
  id: string;
  name: string;
  searchUrl: string;
  iconUrl?: string;
}

const DEFAULT_SEARCH_ENGINES: SearchEngine[] = [
  {
    id: "google",
    name: "Google",
    searchUrl: "https://www.google.com/search?q={searchTerms}",
    iconUrl: "https://www.google.com/favicon.ico",
  },
  {
    id: "bing",
    name: "Bing",
    searchUrl: "https://www.bing.com/search?q={searchTerms}",
    iconUrl: "https://www.bing.com/favicon.ico",
  },
  {
    id: "duckduckgo",
    name: "DuckDuckGo",
    searchUrl: "https://duckduckgo.com/?q={searchTerms}",
    iconUrl: "https://duckduckgo.com/favicon.ico",
  },
];

export function SearchBar() {
  const [query, setQuery] = useState("");
  const [isDropdownOpen, setIsDropdownOpen] = useState(false);
  const searchEngines: SearchEngine[] = DEFAULT_SEARCH_ENGINES;

  const [selectedEngine, setSelectedEngine] = useState<SearchEngine>(
    DEFAULT_SEARCH_ENGINES[0],
  );

  const dropdownRef = useRef<HTMLDivElement>(null);
  const dropdownButtonRef = useRef<HTMLButtonElement>(null);

  useEffect(() => {
    const loadSettings = async () => {
      const settings = await getNewTabSettings();
      if (settings.searchBar) {
        if (settings.searchBar.searchEngine !== "default") {
          const engine = DEFAULT_SEARCH_ENGINES.find(
            (e) => e.id === settings.searchBar.searchEngine,
          );
          if (engine) {
            setSelectedEngine(engine);
          }
        }
      }
    };

    loadSettings();
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
    e.preventDefault();

    if (!query.trim()) return;

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
      if (selectedEngine.id === "default") {
        url = `search:${query}`;
      } else {
        url = selectedEngine.searchUrl.replace(
          "{searchTerms}",
          encodeURIComponent(query),
        );
      }
    }

    window.location.href = url;
  };

  const selectSearchEngine = async (engine: SearchEngine) => {
    setSelectedEngine(engine);
    setIsDropdownOpen(false);

    const settings = await getNewTabSettings();
    await saveNewTabSettings({
      ...settings,
      searchBar: {
        ...settings.searchBar,
        searchEngine: engine.id,
      },
    });
  };

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
            {selectedEngine.iconUrl && (
              <img
                src={selectedEngine.iconUrl}
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
                  key={engine.id}
                  type="button"
                  onClick={() => selectSearchEngine(engine)}
                  className={`flex items-center gap-2 w-full px-3 py-2 text-left hover:bg-gray-100 dark:hover:bg-gray-700 transition-colors ${
                    selectedEngine.id === engine.id
                      ? "bg-gray-100 dark:bg-gray-700"
                      : ""
                  }`}
                >
                  {engine.iconUrl && (
                    <img
                      src={engine.iconUrl}
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
          placeholder="Search or enter URL"
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
