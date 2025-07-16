import { useBackground } from "@/contexts/BackgroundContext.tsx";
import { useEffect, useRef, useState } from "react";
import {
  getRandomBackgroundImage,
  getSelectedFloorpImage,
} from "../../utils/backgroundImages.ts";
import { getRandomImageFromFolder } from "../../utils/dataManager.ts";

export function Background() {
  const {
    type,
    customImage,
    folderPath,
    selectedFloorp,
    slideshowEnabled,
    slideshowInterval,
  } = useBackground();
  const [currentImage, setCurrentImage] = useState<string | null>(null);
  const [nextImage, setNextImage] = useState<string | null>(null);
  const [isFading, setIsFading] = useState(false);
  const isTransitioningRef = useRef(false);

  const getNextImage = async (): Promise<string | null> => {
    if (type === "random") {
      return getRandomBackgroundImage();
    }
    if (type === "folderPath" && folderPath) {
      const result = await getRandomImageFromFolder(folderPath);
      return result.success ? result.image : null;
    }
    return null;
  };

  useEffect(() => {
    const initialize = async () => {
      //
      if (slideshowEnabled && (type === "random" || type === "folderPath")) {
        // In slideshow mode, let the slideshow effect handle the first image.
        if (!currentImage) {
          const newImage = await getNextImage();
          setCurrentImage(newImage);
        }
        return;
      }

      if (type === "random") {
        setCurrentImage(getRandomBackgroundImage());
      } else if (type === "folderPath" && folderPath) {
        const result = await getRandomImageFromFolder(folderPath);
        if (result.success && result.image) {
          setCurrentImage(result.image);
        } else {
          console.error(
            "Failed to load initial image from folder or no images found",
          );
          setCurrentImage(null);
        }
      } else if (type === "custom" && customImage) {
        setCurrentImage(customImage);
      } else if (type === "floorp" && selectedFloorp) {
        setCurrentImage(getSelectedFloorpImage(selectedFloorp));
      } else {
        setCurrentImage(null);
      }
    };
    initialize();
  }, [type, folderPath, customImage, selectedFloorp, slideshowEnabled]);

  useEffect(() => {
    let timeoutId: number | undefined;

    if (
      slideshowEnabled &&
      (type === "random" || type === "folderPath") &&
      slideshowInterval > 0
    ) {
      const intervalId = setInterval(async () => {
        if (isTransitioningRef.current) {
          return;
        }
        isTransitioningRef.current = true;

        const newImage = await getNextImage();
        if (newImage && newImage !== currentImage) {
          setIsFading(true);
          setNextImage(newImage);

          timeoutId = setTimeout(() => {
            setIsFading(false);
            setCurrentImage(newImage);
            setNextImage(null);
            isTransitioningRef.current = false;
          }, 1000); // Must match transition duration
        } else {
          isTransitioningRef.current = false;
        }
      }, slideshowInterval * 1000);

      return () => {
        clearInterval(intervalId);
        if (timeoutId) {
          clearTimeout(timeoutId);
        }
      };
    }
  }, [slideshowEnabled, type, slideshowInterval, folderPath, currentImage]);

  const transitionClass = isFading ? "transition-opacity duration-1000" : "";

  return (
    <>
      <div
        className={`fixed inset-0 bg-cover bg-center ${transitionClass}`}
        style={{
          backgroundImage: currentImage ? `url(${currentImage})` : "none",
          opacity: nextImage ? 0 : 1,
        }}
      />
      <div
        className={`fixed inset-0 bg-cover bg-center ${transitionClass}`}
        style={{
          backgroundImage: nextImage ? `url(${nextImage})` : "none",
          opacity: nextImage ? 1 : 0,
        }}
      />
    </>
  );
}
