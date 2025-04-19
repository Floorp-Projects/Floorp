import React from "react";
import { ExternalLink } from "lucide-react";

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

interface SearchResultsProps {
  websites: Website[];
  files: LocalFile[];
}

const SearchResults: React.FC<SearchResultsProps> = ({ websites, files }) => {
  return (
    <>
      {websites.length > 0 && (
        <div className="mb-12">
          <h2 className="text-2xl font-semibold mb-4">ウェブサイト検索結果</h2>
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4 md:gap-6">
            {websites.map((item, idx) => (
              <div
                key={idx}
                className="card card-compact bg-base-100 shadow-md hover:shadow-lg transition-shadow duration-200 ease-in-out overflow-hidden"
              >
                {item.image_url && (
                  <figure className="h-40 overflow-hidden">
                    <img
                      src={item.image_url}
                      alt={`${item.title} screenshot`}
                      className="w-full h-full object-cover"
                    />
                  </figure>
                )}
                <div className="card-body">
                  <h3 className="card-title text-base md:text-lg">
                    <a
                      href={item.url}
                      target="_blank"
                      rel="noopener noreferrer"
                      className="link link-hover link-primary inline-flex items-center gap-1"
                    >
                      {item.title}
                      <ExternalLink className="h-4 w-4 text-primary/70" />
                      {" "}
                    </a>
                  </h3>
                  {item.description && (
                    <p className="text-sm text-base-content/80 line-clamp-3">
                      {item.description}
                    </p>
                  )}
                  {item.sitename && (
                    <p className="text-xs text-info mt-1">
                      {item.sitename}
                    </p>
                  )}
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {files.length > 0 && (
        <div className="mb-12">
          <h2 className="text-2xl font-semibold mb-4">
            ローカルファイル検索結果
          </h2>
          <div className="space-y-3">
            {files.map((file, idx) => (
              <div
                key={idx}
                className="p-4 bg-base-100 rounded-lg shadow-sm border border-base-300"
              >
                <h3
                  className="font-semibold text-base mb-1 truncate"
                  title={file.file_name}
                >
                  {file.file_name}
                </h3>
                <p
                  className="text-sm text-base-content/70 truncate"
                  title={file.file_path}
                >
                  {file.file_path}
                </p>
              </div>
            ))}
          </div>
        </div>
      )}
    </>
  );
};

export default SearchResults;
