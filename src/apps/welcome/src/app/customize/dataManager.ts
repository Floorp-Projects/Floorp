import type { SearchEngine } from "./types.ts";
import { rpc } from "../../lib/rpc/rpc";

export async function getSearchEngines(): Promise<SearchEngine[]> {
  return await new Promise((resolve) => {
    // deno-lint-ignore no-window
    window.NRGetSearchEngines((data: string) => {
      const engines = JSON.parse(data);
      resolve(engines);
    });
  });
}

export async function getDefaultEngine(): Promise<SearchEngine> {
  return await new Promise((resolve) => {
    // deno-lint-ignore no-window
    window.NRGetDefaultEngine((data: string) => {
      const engine = JSON.parse(data);
      resolve(engine);
    });
  });
}
export async function setDefaultEngine(
  engineId: string,
): Promise<{ success: boolean; engineId: string }> {
  console.log("setDefaultEngine", engineId);
  return await new Promise((resolve) => {
    // deno-lint-ignore no-window
    window.NRSetDefaultEngine(engineId, (data: string) => {
      resolve(JSON.parse(data));
    });
  });
}

export async function getDefaultPrivateEngine(): Promise<SearchEngine> {
  return await new Promise((resolve) => {
    // deno-lint-ignore no-window
    window.NRGetDefaultPrivateEngine((data: string) => {
      const engine = JSON.parse(data);
      resolve(engine);
    });
  });
}

export async function setDefaultPrivateEngine(
  engineId: string,
): Promise<{ success: boolean; engineId: string }> {
  return await new Promise((resolve) => {
    // deno-lint-ignore no-window
    window.NRSetDefaultPrivateEngine(engineId, (data: string) => {
      resolve(JSON.parse(data));
    });
  });
}

export async function getThemeSetting(): Promise<number | null> {
  return await rpc.getIntPref(
    "layout.css.prefers-color-scheme.content-override",
  );
}

export async function setThemeSetting(
  theme: "system" | "light" | "dark",
): Promise<void> {
  const themeValue = theme === "light" ? 1 : theme === "dark" ? 2 : 0;
  await rpc.setIntPref(
    "layout.css.prefers-color-scheme.content-override",
    themeValue,
  );
}
