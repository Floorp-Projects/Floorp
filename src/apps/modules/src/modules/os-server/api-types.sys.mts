/* MPL-2.0 */

// Shared API contract types between server and clients.

// Domain models
export interface Tab {
  id: number;
  url: string;
  title: string;
  isActive: boolean;
  isPinned: boolean;
}

export interface HistoryItem {
  url: string;
  title: string;
  lastVisitDate: number;
  visitCount: number;
}

export interface Download {
  id: number;
  url: string;
  filename: string;
  status: string;
  startTime: number;
}

// Common response helpers
export interface OkResponse {
  ok: boolean;
}
export interface ErrorResponse {
  error: string;
}
export interface HealthResponse {
  status: "ok";
}

// Route contract map (subset shown; extend as needed)
export type RouteMap = {
  // Health
  "GET /health": { req?: undefined; res: HealthResponse };

  // Browser info
  "GET /browser/tabs": { req?: undefined; res: Tab[] };
  "GET /browser/history": {
    query?: { limit?: number };
    req?: undefined;
    res: HistoryItem[];
  };
  "GET /browser/downloads": {
    query?: { limit?: number };
    req?: undefined;
    res: Download[];
  };
  "GET /browser/context": {
    query?: { historyLimit?: number; downloadLimit?: number };
    req?: undefined;
    res: { history: HistoryItem[]; tabs: Tab[]; downloads: Download[] };
  };

  // WebScraper (subset)
  "GET /scraper/instances/:id/exists": { req?: undefined; res: { exists: boolean } };
  "POST /scraper/instances": { req?: undefined; res: { instanceId: string } };
  "DELETE /scraper/instances/:id": { req?: undefined; res: OkResponse };
  "POST /scraper/instances/:id/navigate": { req: { url: string }; res: OkResponse };
  "GET /scraper/instances/:id/uri": { req?: undefined; res: { uri: string | null } };
  "GET /scraper/instances/:id/html": { req?: undefined; res: { html: string | null } };
  "GET /scraper/instances/:id/element": { query?: { selector?: string }; req?: undefined; res: { element: string | null } };
  "GET /scraper/instances/:id/elementText": { query?: { selector?: string }; req?: undefined; res: { text: string | null } };
  "POST /scraper/instances/:id/click": { req: { selector: string }; res: { clicked: boolean } };
  "POST /scraper/instances/:id/waitForElement": { req: { selector: string; timeout?: number }; res: { found: boolean } };
  "POST /scraper/instances/:id/executeScript": { req: { script: string }; res: OkResponse };
  "GET /scraper/instances/:id/screenshot": { req?: undefined; res: { image: string | null } };
  "GET /scraper/instances/:id/elementScreenshot": { query?: { selector?: string }; req?: undefined; res: { image: string | null } };
  "GET /scraper/instances/:id/fullPageScreenshot": { req?: undefined; res: { image: string | null } };
  "POST /scraper/instances/:id/regionScreenshot": { req: { rect?: { x?: number; y?: number; width?: number; height?: number } }; res: { image: string | null } };
  "POST /scraper/instances/:id/fillForm": { req: { formData: { [selector: string]: string } }; res: { ok: boolean } };
  "GET /scraper/instances/:id/value": { query?: { selector?: string }; req?: undefined; res: { value: string | null } };
  "POST /scraper/instances/:id/submit": { req: { selector: string }; res: OkResponse };

  // TabManager (subset)
  "POST /tabs/instances": { req: { url: string; options?: { inBackground?: boolean } }; res: { instanceId: string } };
  "POST /tabs/instances/attach": { req: { browserId: string } ; res: { instanceId: string | null } };
  "GET /tabs/instances": { req?: undefined; res: Array<{ browserId: string; title: string; url: string; selected: boolean; pinned: boolean }>; };
  "DELETE /tabs/instances/:id": { req?: undefined; res: OkResponse };
  "POST /tabs/instances/:id/navigate": { req: { url: string }; res: OkResponse };
  "GET /tabs/instances/:id/uri": { req?: undefined; res: { uri: string } };
  "GET /tabs/instances/:id/html": { req?: undefined; res: { html: string | null } };
  "GET /tabs/instances/:id/element": { query?: { selector?: string }; req?: undefined; res: { element: string | null } };
  "GET /tabs/instances/:id/elementText": { query?: { selector?: string }; req?: undefined; res: { text: string | null } };
  "POST /tabs/instances/:id/click": { req: { selector: string }; res: { clicked: boolean | null } };
  "POST /tabs/instances/:id/waitForElement": { req: { selector: string; timeout?: number }; res: { found: boolean | null } };
  "POST /tabs/instances/:id/executeScript": { req: { script: string }; res: OkResponse };
  "GET /tabs/instances/:id/screenshot": { req?: undefined; res: { image: string | null } };
  "GET /tabs/instances/:id/elementScreenshot": { query?: { selector?: string }; req?: undefined; res: { image: string | null } };
  "GET /tabs/instances/:id/fullPageScreenshot": { req?: undefined; res: { image: string | null } };
  "POST /tabs/instances/:id/regionScreenshot": { req: { rect?: { x?: number; y?: number; width?: number; height?: number } }; res: { image: string | null } };
  "POST /tabs/instances/:id/fillForm": { req: { formData: { [selector: string]: string } }; res: { ok: boolean | null } };
  "GET /tabs/instances/:id/value": { query?: { selector?: string }; req?: undefined; res: { value: string | null } };
  "POST /tabs/instances/:id/submit": { req: { selector: string }; res: { ok: boolean | null } };
};

