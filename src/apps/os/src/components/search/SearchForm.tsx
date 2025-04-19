import React from "react";
import { Search } from "lucide-react";

interface SearchFormProps {
  searchQuery: string;
  setSearchQuery: (query: string) => void;
  handleSearch: (e: React.FormEvent) => void;
  isLoading: boolean;
}

const SearchForm: React.FC<SearchFormProps> = ({
  searchQuery,
  setSearchQuery,
  handleSearch,
  isLoading,
}) => {
  return (
    <form onSubmit={handleSearch} className="mb-8 md:mb-12 max-w-3xl mx-auto">
      <div className="join w-full shadow-sm">
        <input
          type="text"
          placeholder="ウェブ検索、ローカルファイル検索、AI検索..."
          className="input input-bordered join-item w-full input-lg focus:outline-none focus:border-primary-focus focus:ring-2 focus:ring-primary/30"
          value={searchQuery}
          onChange={(e) =>
            setSearchQuery(e.target.value)}
          disabled={isLoading}
        />
        <button
          type="submit"
          className="btn btn-primary join-item btn-lg"
          disabled={isLoading || !searchQuery.trim()}
        >
          {isLoading
            ? <span className="loading loading-spinner loading-sm"></span>
            : <Search className="h-6 w-6" />}
        </button>
      </div>
    </form>
  );
};

export default SearchForm;
