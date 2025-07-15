import { useBackground } from "@/contexts/BackgroundContext.tsx";
import { useEffect, useRef, useState } from "react";
import {
  getAllBackgroundImages,
  getRandomBackgroundImage,
  getSelectedFloorpImage,
} from "../../utils/backgroundImages.ts";
import {
  getAllImagesFromFolder,
  getRandomImageFromFolder,
} from "../../utils/dataManager.ts";

export function Background() {
  const { type, customImage, folderPath, selectedFloorp, slideshow } =
    useBackground();
  const [folderImage, setFolderImage] = useState<string | null>(null);
  const [currentImageIndex, setCurrentImageIndex] = useState<number>(0);
  const [folderImages, setFolderImages] = useState<string[]>([]);
  const slideshowIntervalRef = useRef<number | null>(null);

  useEffect(() => {
    if (type === "folderPath" && folderPath) {
      // Load random image for immediate display
      getRandomImageFromFolder(folderPath)
        .then((result) => {
          if (result.success && result.image) {
            setFolderImage(result.image);
          } else {
            console.error(
              "Failed to load random image from folder or no images found",
            );
            setFolderImage(null);
          }
        })
        .catch((error) => {
          console.error("Failed to load random image from folder:", error);
          setFolderImage(null);
        });

      // Load all images for slideshow
      getAllImagesFromFolder(folderPath)
        .then((result) => {
          if (result.success && result.images.length > 0) {
            setFolderImages(result.images);
          } else {
            setFolderImages([]);
          }
        })
        .catch((error) => {
          console.error("Failed to load all images from folder:", error);
          setFolderImages([]);
        });
    } else {
      setFolderImages([]);
    }
  }, [type, folderPath]);

  // Clear slideshow interval when component unmounts or settings change
  useEffect(() => {
    return () => {
      if (slideshowIntervalRef.current) {
        clearInterval(slideshowIntervalRef.current);
        slideshowIntervalRef.current = null;
      }
    };
  }, []);

  // Handle slideshow for random images
  useEffect(() => {
    if (slideshowIntervalRef.current) {
      clearInterval(slideshowIntervalRef.current);
      slideshowIntervalRef.current = null;
    }

    if (slideshow.enabled && (type === "random" || type === "folderPath")) {
      const allImages = type === "random"
        ? getAllBackgroundImages()
        : folderImages;

      if (allImages.length > 1) {
        slideshowIntervalRef.current = setInterval(() => {
          setCurrentImageIndex((prev) => (prev + 1) % allImages.length);
        }, slideshow.interval * 1000);
      }
    }

    return () => {
      if (slideshowIntervalRef.current) {
        clearInterval(slideshowIntervalRef.current);
        slideshowIntervalRef.current = null;
      }
    };
  }, [slideshow.enabled, slideshow.interval, type, folderImages]);

  useEffect(() => {
    const root = document.documentElement;

    root.style.removeProperty("background-image");
    root.style.removeProperty("background-size");
    root.style.removeProperty("background-position");

    if (type === "random") {
      const allImages = getAllBackgroundImages();
      const imageToShow = slideshow.enabled && allImages.length > 1
        ? allImages[currentImageIndex]
        : getRandomBackgroundImage();

      root.style.backgroundImage = `url(${imageToShow})`;
      root.style.backgroundSize = "cover";
      root.style.backgroundPosition = "center";
    } else if (type === "custom" && customImage) {
      root.style.backgroundImage = `url(${customImage})`;
      root.style.backgroundSize = "cover";
      root.style.backgroundPosition = "center";
    } else if (type === "folderPath" && folderImage) {
      const imageToShow = slideshow.enabled && folderImages.length > 1
        ? folderImages[currentImageIndex]
        : folderImage;

      root.style.backgroundImage = `url(${imageToShow})`;
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
  }, [
    type,
    customImage,
    folderImage,
    selectedFloorp,
    slideshow.enabled,
    currentImageIndex,
    folderImages,
  ]);

  return null;
}
