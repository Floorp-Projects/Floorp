/* MPL-2.0 */

export type HttpMethod = "GET" | "POST" | "DELETE" | "OPTIONS";

export interface RouteMatch {
  handler: Handler;
  params: Record<string, string>;
}

export interface Context {
  method: HttpMethod;
  pathname: string;
  searchParams: URLSearchParams;
  headers: Record<string, string>;
  body: Uint8Array;
  // Path parameters captured from the route pattern, e.g. "/items/:id"
  params: Record<string, string>;
  // Convenience helpers injected by server
  json<T = unknown>(): T | null;
}

export interface HttpResult {
  status?: number;
  body?: unknown;
}

export type Handler = (ctx: Context) => Promise<HttpResult> | HttpResult;

interface CompiledRoute {
  method: HttpMethod;
  pattern: string;
  regex: RegExp;
  paramNames: string[];
  handler: Handler;
}

export class Router {
  private routes: CompiledRoute[] = [];

  register(method: HttpMethod, pattern: string, handler: Handler) {
    const { regex, paramNames } = compilePattern(pattern);
    this.routes.push({ method, pattern, regex, paramNames, handler });
  }

  match(method: string, pathname: string): RouteMatch | null {
    for (const r of this.routes) {
      if (r.method !== method) continue;
      const m = r.regex.exec(pathname);
      if (!m) continue;
      const params: Record<string, string> = {};
      r.paramNames.forEach((name, i) => (params[name] = m[i + 1]));
      return { handler: r.handler, params };
    }
    return null;
  }
}

function compilePattern(
  pattern: string,
): { regex: RegExp; paramNames: string[] } {
  const parts = pattern.split("/").filter(Boolean);
  const names: string[] = [];
  const re = parts
    .map((p) => {
      if (p.startsWith(":")) {
        names.push(p.slice(1));
        return "([^/]+)";
      }
      return escapeRegex(p);
    })
    .join("/");
  const regex = new RegExp(`^/${re}$`);
  return { regex, paramNames: names };
}

function escapeRegex(s: string) {
  return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

export class NamespaceBuilder {
  constructor(private router: Router, private base: string) {}

  namespace(seg: string, fn: (ns: NamespaceBuilder) => void) {
    const next = new NamespaceBuilder(this.router, join(this.base, seg));
    fn(next);
    return this;
  }

  get(seg: string, handler: Handler) {
    this.router.register("GET", join(this.base, seg), handler);
    return this;
  }
  post(seg: string, handler: Handler) {
    this.router.register("POST", join(this.base, seg), handler);
    return this;
  }
  delete(seg: string, handler: Handler) {
    this.router.register("DELETE", join(this.base, seg), handler);
    return this;
  }
}

export function createApi(router: Router) {
  return new NamespaceBuilder(router, "");
}

function join(base: string, seg: string) {
  const b = base.endsWith("/") ? base.slice(0, -1) : base;
  const s = seg ? (seg.startsWith("/") ? seg : `/${seg}`) : "";
  return (b || "") + s;
}
