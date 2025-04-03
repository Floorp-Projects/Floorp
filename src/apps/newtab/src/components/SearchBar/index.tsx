import { FormEvent, useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import { ChevronDown, Search } from "lucide-react";
import {
  getSearchEngines,
  getDefaultEngine,
  type SearchEngine,
  getSuggestions,
} from "./dataManager.ts";

export function SearchBar() {
  const { t } = useTranslation();
  const [query, setQuery] = useState("");
  const [isDropdownOpen, setIsDropdownOpen] = useState(false);
  const [searchEngines, setSearchEngines] = useState<SearchEngine[]>([]);
  const [selectedEngine, setSelectedEngine] = useState<SearchEngine | null>(null);
  const [loading, setLoading] = useState(true);
  const [suggestions, setSuggestions] = useState<string[]>([]);
  const [showSuggestions, setShowSuggestions] = useState(false);

  const dropdownRef = useRef<HTMLDivElement>(null);
  const dropdownButtonRef = useRef<HTMLButtonElement>(null);
  const suggestionsRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    const loadEngines = async () => {
      try {
        setLoading(true);
        const engines = await getSearchEngines();
        const defaultEngine = await getDefaultEngine();

        setSearchEngines(engines);
        setSelectedEngine(defaultEngine);
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

      if (
        showSuggestions &&
        suggestionsRef.current &&
        inputRef.current &&
        !suggestionsRef.current.contains(event.target as Node) &&
        !inputRef.current.contains(event.target as Node)
      ) {
        setShowSuggestions(false);
      }
    };

    document.addEventListener("mousedown", handleClickOutside);

    return () => {
      document.removeEventListener("mousedown", handleClickOutside);
    };
  }, [isDropdownOpen, showSuggestions]);

  const handleSearch = (e: FormEvent) => {
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
      console.log("selectedEngine", selectedEngine.searchUrl);
      url = selectedEngine.searchUrl + query;
    }
    globalThis.location.href = url;
  };

  const selectSearchEngine = (engine: SearchEngine) => {
    setSelectedEngine(engine);
    setIsDropdownOpen(false);
  };

  const handleInputChange = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const value = e.target.value;
    setQuery(value);

    if (value.trim() && selectedEngine) {
      try {
        const result = await getSuggestions(value, selectedEngine);
        if (result.success && result.suggestions.length > 0) {
          setSuggestions(result.suggestions);
          setShowSuggestions(true);
        } else {
          setSuggestions([]);
          setShowSuggestions(false);
        }
      } catch (error) {
        console.error("Failed to get suggestions:", error);
        setSuggestions([]);
        setShowSuggestions(false);
      }
    } else {
      setSuggestions([]);
      setShowSuggestions(false);
    }
  };

  const handleSuggestionClick = (suggestion: string) => {
    setQuery(suggestion);
    setShowSuggestions(false);

    if (selectedEngine) {
      const url = selectedEngine.searchUrl + encodeURIComponent(suggestion);
      globalThis.location.href = url;
    }
  };

  if (loading || !selectedEngine) {
    return (
      <div className="fixed top-4 left-1/2 transform -translate-x-1/2 w-full max-w-xl">
        <div className="bg-white/50 dark:bg-gray-700/50 backdrop-blur-sm rounded-lg shadow-sm flex items-center p-2 justify-center">
          <div className="animate-pulse">{t("searchBar.loadingSearchEngines")}</div>
        </div>
      </div>
    );
  }

  return (
    <div className="fixed top-4 left-1/2 transform -translate-x-1/2 w-full max-w-xl">
      <div className="relative">
        <form
          onSubmit={handleSearch}
          className={`bg-gray-700 backdrop-blur-sm rounded-lg shadow-sm flex items-center p-2 ${showSuggestions && suggestions.length > 0 ? 'rounded-b-none' : ''}`}
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
                className="absolute top-full left-0 mt-2 bg-gray-800 rounded-lg shadow-md overflow-hidden z-10 w-48"
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

          <div className="relative flex-1">
            <input
              type="text"
              value={query}
              onChange={handleInputChange}
              placeholder={t("searchBar.searchOrEnterUrl")}
              className="w-full bg-transparent border-none outline-none px-2 py-1 text-gray-900 dark:text-gray-100"
              autoFocus
              ref={inputRef}
            />
          </div>

          <button
            type="submit"
            className="p-2 text-gray-500 hover:text-gray-700 dark:text-gray-400 dark:hover:text-gray-200"
          >
            <Search size={18} />
          </button>
        </form>

        {showSuggestions && suggestions.length > 0 && (
          <div
            ref={suggestionsRef}
            className="absolute top-full left-0 w-full bg-gray-700 rounded-b-lg z-10 overflow-hidden"
            style={{ width: '100%' }}
          >
            <ul>
              {suggestions.map((suggestion, index) => (
                <li key={index} className="">
                  <button
                    type="button"
                    onClick={() => handleSuggestionClick(suggestion)}
                    className="w-full px-3 py-2 text-left text-gray-900 dark:text-gray-100 hover:bg-gray-100 dark:hover:bg-gray-700 flex items-center"
                  >
                    <Search size={16} className="text-gray-400 mr-2 flex-shrink-0" />
                    {suggestion}
                  </button>
                </li>
              ))}
            </ul>
          </div>
        )}
      </div>
    </div>
  );
}
