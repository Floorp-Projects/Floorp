import React from "react";
import ReactMarkdown from "react-markdown";

interface AiSearchResultsProps {
  isLoading: boolean;
  result: string | null;
}

const AiSearchResults: React.FC<AiSearchResultsProps> = (
  { isLoading, result },
) => {
  if (!isLoading && !result) {
    return null;
  }

  return (
    <div className="mt-12">
      <h2 className="text-2xl font-semibold mb-4">AI Search 結果</h2>
      <div className="p-4 md:p-6 bg-base-100 rounded-lg shadow-sm border border-base-300 min-h-[100px]">
        {isLoading
          ? (
            <div className="flex items-center justify-center h-full">
              <span className="loading loading-dots loading-md"></span>
              <span className="ml-2 text-base-content/70">AI検索中...</span>
            </div>
          )
          : result
          ? (
            <article className="prose prose-sm md:prose-base max-w-none">
              <ReactMarkdown>{result}</ReactMarkdown>
            </article>
          )
          : (
            <p className="text-base-content/70 text-center">
              AI検索結果はありません。
            </p>
          )}
      </div>
    </div>
  );
};

export default AiSearchResults;
