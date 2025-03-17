import { useBackground } from "@/contexts/BackgroundContext.tsx";
import { useEffect } from "react";
import { getRandomBackgroundImage } from "../../utils/backgroundImages.ts";

export function Background() {
  const { type, customImage } = useBackground();

  useEffect(() => {
    const root = document.documentElement;

    root.style.removeProperty("background-image");
    root.style.removeProperty("background-size");
    root.style.removeProperty("background-position");

    if (type === "random") {
      const randomImage = getRandomBackgroundImage();
      root.style.backgroundImage = `url(${randomImage})`;
      root.style.backgroundSize = "cover";
      root.style.backgroundPosition = "center";
    } else if (type === "custom" && customImage) {
      root.style.backgroundImage = `url(${customImage})`;
      root.style.backgroundSize = "cover";
      root.style.backgroundPosition = "center";
    }
  }, [type, customImage]);

  return null;
}
