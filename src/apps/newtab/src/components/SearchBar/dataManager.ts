declare global {
  interface Window {
    NRGetSearchEngines: (callback: (data: string) => void) => void;
    NRGetDefaultEngine: (callback: (data: string) => void) => void;
    NRGetDefaultPrivateEngine: (callback: (data: string) => void) => void;
  }
}

export interface SearchEngine {
  identifier: string;
  name: string;
  searchUrl: string;
  iconURL?: string;
  description?: string;
  isDefault?: boolean;
}

export async function getSearchEngines(): Promise<SearchEngine[]> {
  return await new Promise((resolve) => {
    window.NRGetSearchEngines((data: string) => {
      const engines = JSON.parse(data);
      resolve(engines);
    });
  });
}

export async function getDefaultEngine(): Promise<SearchEngine> {
  return await new Promise((resolve) => {
    window.NRGetDefaultEngine((data: string) => {
      const engine = JSON.parse(data);
      resolve(engine);
    });
  });
}

export async function getDefaultPrivateEngine(): Promise<SearchEngine> {
  return await new Promise((resolve) => {
    window.NRGetDefaultPrivateEngine((data: string) => {
      const engine = JSON.parse(data);
      resolve(engine);
    });
  });
}
