interface TopSite {
  url: string;
  label: string;
  favicon?: string;
  smallFavicon?: string | null;
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
      globalThis.GetCurrentTopSites((sites: string) => {
        resolve(
          JSON.parse(sites).topsites.map((site: TopSite) => ({
            ...site,
            smallFavicon: site.smallFavicon || "",
          })) as TopSite[],
        );
      });
    });
  }

  saveUserAddedSites(sites: TopSite[]): void {
    try {
      const sanitizedSites = sites.map((site) => ({
        ...site,
        label: site.label || "",
        smallFavicon: site.smallFavicon || "",
      }));
      localStorage.setItem("userAddedTopSites", JSON.stringify(sanitizedSites));
    } catch (error) {
      console.error("Failed to save user-added top sites:", error);
    }
  }

  getUserAddedSites(): TopSite[] {
    try {
      const sites = localStorage.getItem("userAddedTopSites");
      return sites
        ? JSON.parse(sites).map((site: TopSite) => ({
          ...site,
          label: site.label || "",
          smallFavicon: site.smallFavicon || "",
        }))
        : [];
    } catch (error) {
      console.error("Failed to retrieve user-added top sites:", error);
      return [];
    }
  }
}
