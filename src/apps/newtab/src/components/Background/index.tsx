import { useBackground } from "@/contexts/BackgroundContext.tsx";
import { useEffect, useState } from "react";
import { getRandomBackgroundImage, getSelectedFloorpImage } from "../../utils/backgroundImages.ts";
import { getRandomImageFromFolder } from "../../utils/dataManager.ts";

export function Background() {
  const { type, customImage, folderPath, selectedFloorp } = useBackground();
  const [folderImage, setFolderImage] = useState<string | null>(null);

  useEffect(() => {
    if (type === "folderPath" && folderPath) {
      getRandomImageFromFolder(folderPath)
        .then(result => {
          if (result.success && result.image) {
            setFolderImage(result.image);
          } else {
            console.error("Failed to load random image from folder or no images found");
            setFolderImage(null);
          }
        })
        .catch(error => {
          console.error("Failed to load random image from folder:", error);
          setFolderImage(null);
        });
    }
  }, [type, folderPath]);

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
    } else if (type === "folderPath" && folderImage) {
      root.style.backgroundImage = `url(${folderImage})`;
      root.style.backgroundSize = "cover";
      root.style.backgroundPosition = "center";
    } else if (type === "floorp" && selectedFloorp) {
      const floorpImage = getSelectedFloorpImage(selectedFloorp);
      if (floorpImage) {
        root.style.backgroundImage = `url(${floorpImage})`;
        root.style.backgroundSize = "cover";
        root.style.backgroundPosition = "center";
      }
    }
  }, [type, customImage, folderImage, selectedFloorp]);

  return null;
}
