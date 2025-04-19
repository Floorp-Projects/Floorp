import React, { useState } from "react";
import SearchForm from "./SearchForm.tsx";
import SearchResults from "./SearchResults.tsx";
import AiSearchResults from "./AiSearchResults.tsx";

interface Website {
  url: string;
  title: string;
  description?: string;
  image_url?: string;
  sitename?: string;
}

interface LocalFile {
  file_name: string;
  file_path: string;
}

type AiResultData = string | null;

const SearchPage: React.FC = () => {
  const [searchQuery, setSearchQuery] = useState("");
  const [hasSearched, setHasSearched] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [websites, setWebsites] = useState<Website[]>([]);
  const [files, setFiles] = useState<LocalFile[]>([]);

  const [aiResult, setAiResult] = useState<AiResultData>(null);
  const [aiLoading, setAiLoading] = useState(false);

  // --- API Endpoint ---
  const API_BASE_URL = "http://localhost:8080/v1";

  const pollAiResult = async (id: string) => {
    try {
      // TODO: Replace fetch with your project's specific API client if you have one.
      const res = await fetch(`${API_BASE_URL}/ai_search/${id}`, {
        method: "GET",
        headers: {
          "accept": "application/json",
        },
      });
      if (!res.ok) {
        throw new Error(`API Error: ${res.status}`);
      }
      const data = await res.json();

      console.log("AI検索結果ポーリング:", data);

      if (!data.completed) {
        setTimeout(() => pollAiResult(id), 2000);
      } else {
        // Check if data.data is an object and extract the text string
        let resultString: AiResultData = null;
        if (typeof data.data === "string") {
          resultString = data.data;
        } else if (
          data.data && typeof data.data === "object" && "text" in data.data &&
          typeof data.data.text === "string"
        ) {
          // Extract markdown from the 'text' property
          resultString = data.data.text;
        } else {
          // Handle unexpected data structure
          console.error("AI検索結果の形式が不正です:", data.data);
          resultString =
            "AI検索結果の表示中にエラーが発生しました (不正な形式)";
        }
        setAiResult(resultString);
        setAiLoading(false);
      }
    } catch (error) {
      console.error("AI検索ポーリングエラー:", error);
      setAiResult("AI検索結果の取得中にエラーが発生しました。");
      setAiLoading(false);
    }
  };

  const handleAiSearch = async () => {
    setAiLoading(true);
    setAiResult(null);
    try {
      const res = await fetch(`${API_BASE_URL}/ai_search`, {
        method: "POST",
        headers: {
          "accept": "application/json",
          "Content-Type": "application/json",
        },
        body: JSON.stringify({ query: searchQuery }),
      });
      if (!res.ok) {
        throw new Error(`API Error: ${res.status}`);
      }
      const data = await res.json();
      if (data.id) {
        pollAiResult(data.id);
      } else {
        console.error("AI検索開始エラー: IDが返されませんでした", data);
        setAiResult("AI検索を開始できませんでした。");
        setAiLoading(false);
      }
    } catch (error) {
      console.error("AI検索開始エラー:", error);
      setAiResult("AI検索の開始中にエラーが発生しました。");
      setAiLoading(false);
    }
  };

  const handleSearch = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!searchQuery.trim()) return;

    console.log(`検索開始: ${searchQuery}`);
    setIsLoading(true);
    setAiResult(null);
    setHasSearched(false);
    setWebsites([]);
    setFiles([]);
    setAiLoading(false);

    try {
      const res = await fetch(
        `${API_BASE_URL}/fast_search/${encodeURIComponent(searchQuery)}`,
      );
      if (!res.ok) {
        throw new Error(`API Error: ${res.status}`);
      }
      const data = await res.json();
      console.log("高速検索結果:", data);
      setWebsites(data.websites || []);
      setFiles(data.local_files || []);
    } catch (error) {
      console.error("高速検索エラー:", error);
      setWebsites([]);
      setFiles([]);
    }

    setHasSearched(true);
    setIsLoading(false);
    handleAiSearch();
  };

  return (
    <div className="min-h-screen bg-base-100 text-base-content flex flex-col relative overflow-hidden">
      <div className="absolute inset-0 pointer-events-none overflow-hidden">
        <div
          className="absolute inset-0 opacity-[0.03]"
          style={{
            backgroundImage:
              "linear-gradient(to right, currentColor 1px, transparent 1px), linear-gradient(to bottom, currentColor 1px, transparent 1px)",
            backgroundSize: "40px 40px",
          }}
        >
        </div>
        <div className="absolute -right-20 -top-20 w-96 h-96 opacity-[0.07]">
          <div className="relative w-full h-full">
            {[...Array(24)].map((_, i) => (
              <div
                key={i}
                className="absolute"
                style={{
                  width: "40px",
                  height: "35px",
                  transform: `rotate(${i * 15}deg) translateY(-120px)`,
                  transformOrigin: "center center",
                }}
              >
                <div className="w-full h-full border border-primary rounded-md">
                </div>
              </div>
            ))}
          </div>
        </div>
        <div className="absolute -left-10 bottom-1/4 opacity-[0.06]">
          <div className="grid grid-cols-5 gap-6">
            {[...Array(25)].map((_, i) => (
              <div
                key={i}
                className={`w-10 h-10 border border-secondary ${
                  i % 2 === 0 ? "rotate-0" : "rotate-12"
                }`}
              >
              </div>
            ))}
          </div>
        </div>
        <div className="absolute right-1/4 -bottom-20 opacity-[0.06]">
          <div className="grid grid-cols-4 grid-rows-4 gap-4">
            {[...Array(16)].map((_, i) => (
              <div
                key={i}
                className={`w-8 h-8 rounded-full border-2 border-accent ${
                  i % 3 === 0 ? "bg-accent/20" : ""
                }`}
              >
              </div>
            ))}
          </div>
        </div>
        <div className="absolute left-0 top-1/3 opacity-[0.05]">
          <div className="grid grid-cols-3 gap-2">
            {[...Array(9)].map((_, i) => (
              <div
                key={i}
                className={`w-12 h-12 border border-primary/40 ${
                  i % 2 === 0 ? "rotate-45" : ""
                }`}
              >
              </div>
            ))}
          </div>
        </div>
        <div
          className="absolute right-20 top-1/3 w-48 h-64 opacity-[0.04]"
          style={{
            backgroundImage:
              "repeating-linear-gradient(45deg, currentColor, currentColor 1px, transparent 1px, transparent 10px)",
          }}
        >
        </div>
        <div className="absolute left-1/4 top-1/2 transform -translate-y-1/2 opacity-[0.08]">
          <div className="grid grid-cols-10 gap-3">
            {[...Array(100)].map((_, i) => (
              <div
                key={i}
                className={`w-1 h-1 rounded-full ${
                  i % 5 === 0
                    ? "bg-primary"
                    : i % 3 === 0
                    ? "bg-secondary"
                    : "bg-accent"
                }`}
              >
              </div>
            ))}
          </div>
        </div>
      </div>

      <main className="flex-1 container mx-auto max-w-4xl px-4 py-8 relative z-10">
        <div className="text-center mb-8 md:mb-12">
          <h1 className="text-3xl md:text-4xl lg:text-5xl font-bold mb-2">
            Floorp Search
          </h1>
          <p className="text-lg text-base-content/70">
            ここに検索キーワードを入力してください
          </p>
        </div>

        <div className="max-w-3xl mx-auto">
          <SearchForm
            searchQuery={searchQuery}
            setSearchQuery={setSearchQuery}
            handleSearch={handleSearch}
            isLoading={isLoading}
          />
        </div>

        <div className="mt-8 md:mt-12">
          {isLoading && (
            <div className="flex justify-center items-center py-10">
              <span className="loading loading-lg loading-spinner text-primary">
              </span>
            </div>
          )}

          {!isLoading && hasSearched && websites.length === 0 &&
            files.length === 0 && !aiLoading && !aiResult && (
            <div className="text-center text-base-content/70 py-10">
              検索結果がありません。
            </div>
          )}

          {!isLoading && hasSearched &&
            (websites.length > 0 || files.length > 0) && (
            <SearchResults websites={websites} files={files} />
          )}

          {hasSearched && (
            <AiSearchResults isLoading={aiLoading} result={aiResult} />
          )}
        </div>
      </main>
    </div>
  );
};

export default SearchPage;
