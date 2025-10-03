import type { ReactNode } from "react";
import { useMemo } from "react";
import { useTranslation } from "react-i18next";
import { Link, useLocation } from "react-router-dom";
import { ArrowUpRight } from "lucide-react";
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { cn } from "@/lib/utils";
import {
  buildSearchDocuments,
  normalizeSearchText,
  type SettingsSearchDocument,
} from "@/lib/search";

interface SearchResult {
  document: SettingsSearchDocument;
  snippet: string;
  matchSource: "title" | "body" | null;
  score: number;
}

function useSearchQuery() {
  const location = useLocation();
  return useMemo(() => {
    const params = new URLSearchParams(location.search);
    return params.get("q")?.trim() ?? "";
  }, [location.search]);
}

function escapeRegExp(value: string) {
  return value.replace(/[-\\/^$*+?.()|[\]{}]/g, "\\$&");
}

function highlightText(text: string, query: string): ReactNode {
  if (!query) return text;
  const pattern = escapeRegExp(query);
  if (!pattern) {
    return text;
  }
  const regex = new RegExp(pattern, "ig");
  const parts: ReactNode[] = [];
  let lastIndex = 0;
  let match: RegExpExecArray | null;

  while ((match = regex.exec(text)) !== null) {
    const [matched] = match;
    const matchStart = match.index;
    if (matchStart > lastIndex) {
      parts.push(text.slice(lastIndex, matchStart));
    }
    parts.push(
      <mark
        key={`${matchStart}-`}
        className="rounded bg-primary/20 px-1 py-0.5 text-primary-foreground"
      >
        {matched}
      </mark>,
    );
    lastIndex = matchStart + matched.length;
  }

  if (lastIndex < text.length) {
    parts.push(text.slice(lastIndex));
  }

  return parts.length ? parts : text;
}

function extractSnippet(text: string, query: string): string {
  const trimmed = text.trim();
  if (!trimmed) return "";
  if (!query) {
    return trimmed.length > 160 ? `${trimmed.slice(0, 160).trim()}…` : trimmed;
  }
  const regex = new RegExp(escapeRegExp(query), "i");
  const match = regex.exec(trimmed);
  if (!match) {
    return trimmed.length > 160 ? `${trimmed.slice(0, 160).trim()}…` : trimmed;
  }
  const start = Math.max(0, match.index - 80);
  const end = Math.min(trimmed.length, match.index + match[0].length + 100);
  const snippet = trimmed.slice(start, end).trim();
  const prefix = start > 0 ? "…" : "";
  const suffix = end < trimmed.length ? "…" : "";
  return `${prefix}${snippet}${suffix}`;
}

function buildSearchResults(
  documents: SettingsSearchDocument[],
  query: string,
): SearchResult[] {
  const normalizedQuery = normalizeSearchText(query);

  if (!normalizedQuery) {
    return documents
      .slice()
      .sort((a, b) => b.priority - a.priority)
      .slice(0, 8)
      .map((document, index) => ({
        document,
        snippet: document.preview,
        matchSource: null,
        score: index,
      }));
  }

  const results: SearchResult[] = [];

  for (const document of documents) {
    const titleIndex = document.normalizedTitle.indexOf(normalizedQuery);
    const textIndex = document.normalizedText.indexOf(normalizedQuery);

    if (titleIndex === -1 && textIndex === -1) {
      continue;
    }

    const matchSource: "title" | "body" =
      titleIndex !== -1 && (titleIndex <= textIndex || textIndex === -1)
        ? "title"
        : "body";

    const snippet = matchSource === "body"
      ? extractSnippet(document.textContent, query) || document.preview
      : document.preview;

    const baseScore = matchSource === "title" ? 0 : 1000;
    const position = matchSource === "title" ? titleIndex : textIndex;
    const priorityScore = document.priority * -20;
    const score = baseScore + (position === -1 ? 0 : position) + priorityScore;

    results.push({
      document,
      snippet: snippet || document.preview,
      matchSource,
      score,
    });
  }

  return results.sort((a, b) =>
    a.score - b.score ||
    b.document.priority - a.document.priority ||
    a.document.title.localeCompare(b.document.title)
  );
}

export default function SearchPage() {
  const { t, i18n } = useTranslation();
  const query = useSearchQuery();

  const documents = useMemo(
    () => buildSearchDocuments(i18n),
    [i18n, i18n.language],
  );

  const results = useMemo(
    () => buildSearchResults(documents, query),
    [documents, query],
  );

  const hasQuery = normalizeSearchText(query).length > 0;

  return (
    <div className="p-6 space-y-6">
      <div className="flex flex-col items-start pl-6">
        <h1 className="text-3xl font-bold mb-2">{t("pages.search.title")}</h1>
        {hasQuery
          ? (
            <>
              <p className="text-sm text-base-content/80">
                {t("pages.search.resultsFor", { query })}
              </p>
              <p className="text-xs text-base-content/60 mt-1">
                {t("pages.search.searchResultsDescription", { query })}
              </p>
            </>
          )
          : (
            <p className="text-sm text-base-content/70">
              {t("header.searchPlaceholder")}
            </p>
          )}
      </div>

      <div className="space-y-3">
        {results.length === 0 && hasQuery
          ? (
            <div className="pl-6 text-sm text-base-content/70">
              {t("pages.search.noResults")}
            </div>
          )
          : (
            <div className="grid gap-3">
              {results.map((result) => {
                const { document, matchSource, snippet } = result;
                const Icon = document.icon;
                const openLabel = t("pages.search.open", {
                  title: document.title,
                });

                return (
                  <Link
                    key={document.id}
                    to={document.route}
                    aria-label={openLabel}
                    className={cn(
                      "group block focus:outline-none focus-visible:ring-2",
                      "focus-visible:ring-primary/60 focus-visible:ring-offset-2 rounded-lg",
                    )}
                  >
                    <Card className="transition-colors group-hover:border-primary/40 group-hover:bg-base-200/60">
                      <CardHeader className="flex flex-row items-center gap-3">
                        {Icon
                          ? (
                            <Icon
                              className="size-5 text-primary/80"
                              aria-hidden
                            />
                          )
                          : null}
                        <CardTitle className="flex-1 text-base md:text-lg">
                          {highlightText(document.title, hasQuery ? query : "")}
                        </CardTitle>
                        <ArrowUpRight
                          className="size-4 text-base-content/60"
                          aria-hidden
                        />
                      </CardHeader>
                      {(snippet || (!hasQuery && document.preview)) && (
                        <CardContent className="space-y-2 text-sm text-base-content/70">
                          {hasQuery && matchSource === "body"
                            ? (
                              <div className="space-y-1">
                                <p className="text-xs font-medium uppercase tracking-wide text-base-content/50">
                                  {t("pages.search.matchedText")}
                                </p>
                                <p className="text-sm text-base-content/80">
                                  {highlightText(snippet, query)}
                                </p>
                              </div>
                            )
                            : snippet
                            ? (
                              <p>
                                {highlightText(snippet, hasQuery ? query : "")}
                              </p>
                            )
                            : null}
                        </CardContent>
                      )}
                    </Card>
                  </Link>
                );
              })}
            </div>
          )}

        {!hasQuery && results.length > 0 && (
          <p className="pl-6 text-xs text-base-content/50">
            {t("pages.search.results")}
          </p>
        )}
      </div>
    </div>
  );
}
