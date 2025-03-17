import { useEffect } from "react";

export function useBackground(backgroundUrl: string | null) {
  useEffect(() => {
    const root = document.documentElement;
    if (backgroundUrl) {
      root.style.setProperty("--custom-background", `url(${backgroundUrl})`);
    } else {
      root.style.removeProperty("--custom-background");
    }
  }, [backgroundUrl]);
}
