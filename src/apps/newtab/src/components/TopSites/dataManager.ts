interface TopSite {
  url: string;
  label: string;
  favicon?: string;
}

export class TopSitesManager {
  private static instance: TopSitesManager;

  private constructor() {}

  static getInstance(): TopSitesManager {
    if (!TopSitesManager.instance) {
      TopSitesManager.instance = new TopSitesManager();
    }
    return TopSitesManager.instance;
  }

  // deno-lint-ignore require-await
  async getTopSites(): Promise<TopSite[]> {
    return new Promise<TopSite[]>((resolve) => {
      window.GetCurrentTopSites((sites: string) => {
        resolve(JSON.parse(sites).topsites as TopSite[]);
      });
    });
  }
}
