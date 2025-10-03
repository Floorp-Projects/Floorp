import { FormEvent, type KeyboardEvent, useEffect, useMemo, useState } from "react";
import { useLocation, useNavigate } from "react-router-dom";
import { useTranslation } from "react-i18next";
import { Search } from "lucide-react";
import { Input } from "@/components/common/input.tsx";
import { cn } from "@/lib/utils";

function useSearchQuery() {
  const location = useLocation();
  return useMemo(() => {
    const params = new URLSearchParams(location.search);
    return params.get("q")?.trim() ?? "";
  }, [location.search]);
}

export function SettingsSearchInput() {
  const { t } = useTranslation();
  const navigate = useNavigate();
  const queryFromLocation = useSearchQuery();
  const [value, setValue] = useState(queryFromLocation);

  useEffect(() => {
    setValue(queryFromLocation);
  }, [queryFromLocation]);

  const handleSubmit = (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    const trimmed = value.trim();
    if (!trimmed) {
      navigate({ pathname: "/search" });
      return;
    }

    navigate({
      pathname: "/search",
      search: `?q=${encodeURIComponent(trimmed)}`,
    });
  };

  const handleKeyDown = (event: KeyboardEvent<HTMLInputElement>) => {
    if (event.key === "Escape" && value) {
      event.preventDefault();
      setValue("");
      navigate({ pathname: "/search" });
    }
  };

  const placeholder = t("header.searchPlaceholder");

  return (
    <form
      role="search"
      aria-label={t("pages.search.title")}
      onSubmit={handleSubmit}
      className="relative"
    >
      <Search className="pointer-events-none absolute left-3 top-1/2 size-4 -translate-y-1/2 text-base-content/40" />
      <Input
        id="settings-search-input"
        type="search"
        value={value}
        onChange={(event) => setValue(event.target.value)}
        onKeyDown={handleKeyDown}
        placeholder={placeholder}
        aria-label={placeholder}
        autoComplete="off"
        className={cn("pl-9 pr-3", "bg-base-100/90 dark:bg-base-800/70")}
      />
    </form>
  );
}
