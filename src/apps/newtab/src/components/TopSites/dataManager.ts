interface TopSite {
  url: string;
  title: string;
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
    return [
      {
        url: "https://www.google.com",
        title: "Google",
        favicon: "https://www.google.com/favicon.ico",
      },
      {
        url: "https://www.youtube.com",
        title: "YouTube",
        favicon: "https://www.youtube.com/favicon.ico",
      },
      {
        url: "https://github.com",
        title: "GitHub",
        favicon: "https://github.com/favicon.ico",
      },
      {
        url: "https://www.wikipedia.org",
        title: "Wikipedia",
        favicon: "https://www.wikipedia.org/favicon.ico",
      },
      {
        url: "https://twitter.com",
        title: "Twitter",
        favicon: "https://twitter.com/favicon.ico",
      },
      {
        url: "https://www.amazon.com",
        title: "Amazon",
        favicon: "https://www.amazon.com/favicon.ico",
      },
      {
        url: "https://www.reddit.com",
        title: "Reddit",
        favicon: "https://www.reddit.com/favicon.ico",
      },
      {
        url: "https://www.netflix.com",
        title: "Netflix",
        favicon: "https://www.netflix.com/favicon.ico",
      },
    ];
  }
}
