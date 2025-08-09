declare global {
  interface Window {
    NRGetSearchEngines: (callback: (data: string) => void) => void;
    NRGetDefaultEngine: (callback: (data: string) => void) => void;
    NRGetDefaultPrivateEngine: (callback: (data: string) => void) => void;
    NRGetSuggestions: (
      query: string,
      engineId: string,
      callback: (data: string) => void,
    ) => void;
  }
}

import { callNRWithRetry } from "@/utils/nrRetry.ts";

export interface SearchEngine {
  identifier: string;
  name: string;
  searchUrl: string;
  iconURL?: string;
  description?: string;
  isDefault?: boolean;
}

export interface SearchSuggestion {
  success: boolean;
  suggestions: string[];
}

export async function getSearchEngines(): Promise<SearchEngine[]> {
  try {
    return await callNRWithRetry<SearchEngine[]>(
      (cb) => (globalThis as unknown as Window).NRGetSearchEngines(cb),
      (data) => JSON.parse(data),
      {
        retries: 3,
        timeoutMs: 1200,
        delayMs: 300,
        shouldRetry: (v) => !Array.isArray(v),
      },
    );
  } catch (e) {
    console.error("Failed to get search engines:", e);
    return [];
  }
}

export async function getDefaultEngine(): Promise<SearchEngine> {
  try {
    return await callNRWithRetry<SearchEngine>(
      (cb) => (globalThis as unknown as Window).NRGetDefaultEngine(cb),
      (data) => JSON.parse(data),
      {
        retries: 3,
        timeoutMs: 1200,
        delayMs: 300,
        shouldRetry: (v) => !v || !v.identifier,
      },
    );
  } catch (e) {
    console.error("Failed to get default engine:", e);
    return {
      identifier: "",
      name: "",
      searchUrl: "",
      description: "",
      iconURL: "",
    };
  }
}

export async function getDefaultPrivateEngine(): Promise<SearchEngine> {
  try {
    return await callNRWithRetry<SearchEngine>(
      (cb) => (globalThis as unknown as Window).NRGetDefaultPrivateEngine(cb),
      (data) => JSON.parse(data),
      {
        retries: 3,
        timeoutMs: 1200,
        delayMs: 300,
        shouldRetry: (v) => !v || !v.identifier,
      },
    );
  } catch (e) {
    console.error("Failed to get default private engine:", e);
    return {
      identifier: "",
      name: "",
      searchUrl: "",
      description: "",
      iconURL: "",
    };
  }
}

export async function getSuggestions(
  query: string,
  engine: SearchEngine,
): Promise<SearchSuggestion> {
  try {
    return await callNRWithRetry<SearchSuggestion>(
      (cb) =>
        (globalThis as unknown as Window).NRGetSuggestions(
          query,
          engine.identifier,
          cb,
        ),
      (data) => JSON.parse(data),
      {
        retries: 3,
        timeoutMs: 1500,
        delayMs: 300,
        shouldRetry: (v) => !v || v.success === false,
      },
    );
  } catch (e) {
    console.error("Failed to get suggestions:", e);
    return { success: false, suggestions: [] };
  }
}
