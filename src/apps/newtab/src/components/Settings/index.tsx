import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { Modal } from "../Modal/index.tsx";
import { useBackground } from "@/contexts/BackgroundContext.tsx";
import { useComponents } from "@/contexts/ComponentsContext.tsx";
import { getBackgroundImageCount } from "@/utils/backgroundImages.ts";

export function Settings(
  { isOpen, onClose }: { isOpen: boolean; onClose: () => void },
) {
  const { t } = useTranslation();
  const {
    type: backgroundType,
    fileName,
    setType: setBackgroundType,
    setCustomImage,
  } = useBackground();
  const { components, toggleComponent } = useComponents();
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [currentFileName, setCurrentFileName] = useState<string>("");

  useEffect(() => {
    if (backgroundType === "custom" && fileName) {
      setCurrentFileName(fileName);
    } else {
      setCurrentFileName("");
    }
  }, [backgroundType, fileName]);

  const handleFileChange = async (
    event: React.ChangeEvent<HTMLInputElement>,
  ) => {
    const file = event.target.files?.[0];
    if (!file) return;

    setIsSubmitting(true);
    try {
      const reader = new FileReader();
      const imageData = await new Promise<string>((resolve, reject) => {
        reader.onload = () => resolve(reader.result as string);
        reader.onerror = reject;
        reader.readAsDataURL(file);
      });
      await setCustomImage(imageData, file.name);
      setCurrentFileName(file.name);
    } catch (error) {
      console.error("Failed to load image:", error);
    } finally {
      setIsSubmitting(false);
    }
  };

  const handleBackgroundTypeChange = async (type: typeof backgroundType) => {
    setIsSubmitting(true);
    try {
      if (type === "none") {
        await setCustomImage(null, null);
      }
      await setBackgroundType(type);
    } catch (error) {
      console.error("Failed to change background type:", error);
    } finally {
      setIsSubmitting(false);
    }
  };

  const imageCount = getBackgroundImageCount();

  return (
    <Modal isOpen={isOpen} onClose={onClose} title={t("settings.newTabSettings")}>
      <div className="space-y-6">
        <section>
          <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-4">
            {t("settings.displayComponents")}
          </h3>
          <div className="space-y-4">
            <label className="flex items-center space-x-3">
              <input
                type="checkbox"
                checked={components.topSites}
                onChange={() => toggleComponent("topSites")}
                disabled={isSubmitting}
                className="form-checkbox h-5 w-5 text-primary rounded border-gray-300 dark:border-gray-600 focus:ring-primary"
              />
              <span className="text-gray-700 dark:text-gray-200">
                {t("settings.topSites")}
              </span>
            </label>
            <label className="flex items-center space-x-3">
              <input
                type="checkbox"
                checked={components.clock}
                onChange={() => toggleComponent("clock")}
                disabled={isSubmitting}
                className="form-checkbox h-5 w-5 text-primary rounded border-gray-300 dark:border-gray-600 focus:ring-primary"
              />
              <span className="text-gray-700 dark:text-gray-200">{t("settings.clock")}</span>
            </label>
            <label className="flex items-center space-x-3">
              <input
                type="checkbox"
                checked={components.searchBar}
                onChange={() => toggleComponent("searchBar")}
                disabled={isSubmitting}
                className="form-checkbox h-5 w-5 text-primary rounded border-gray-300 dark:border-gray-600 focus:ring-primary"
              />
              <span className="text-gray-700 dark:text-gray-200">
                {t("settings.searchBar")}
              </span>
            </label>
          </div>
        </section>

        <section>
          <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-4">
            {t("settings.backgroundSettings")}
          </h3>
          <div className="space-y-4">
            <label className="flex items-center space-x-3">
              <input
                type="radio"
                name="background"
                value="none"
                checked={backgroundType === "none"}
                onChange={() => handleBackgroundTypeChange("none")}
                disabled={isSubmitting}
                className="form-radio h-5 w-5 text-primary border-gray-300 dark:border-gray-600 focus:ring-primary"
              />
              <span className="text-gray-700 dark:text-gray-200">{t("settings.noBackground")}</span>
            </label>
            <label className="flex items-center space-x-3">
              <input
                type="radio"
                name="background"
                value="random"
                checked={backgroundType === "random"}
                onChange={() => handleBackgroundTypeChange("random")}
                disabled={isSubmitting}
                className="form-radio h-5 w-5 text-primary border-gray-300 dark:border-gray-600 focus:ring-primary"
              />
              <span className="text-gray-700 dark:text-gray-200">
                {t("settings.randomImage")}
              </span>
            </label>
            <label className="flex items-center space-x-3">
              <input
                type="radio"
                name="background"
                value="custom"
                checked={backgroundType === "custom"}
                onChange={() => handleBackgroundTypeChange("custom")}
                disabled={isSubmitting}
                className="form-radio h-5 w-5 text-primary border-gray-300 dark:border-gray-600 focus:ring-primary"
              />
              <span className="text-gray-700 dark:text-gray-200">{t("settings.customImage")}</span>
            </label>

            {backgroundType === "custom" && (
              <div className="mt-4 pl-8">
                {currentFileName && (
                  <div className="mb-2 text-sm text-gray-600 dark:text-gray-400">
                    {t("settings.currentImage")} {currentFileName}
                  </div>
                )}
                <input
                  type="file"
                  accept="image/*"
                  onChange={handleFileChange}
                  disabled={isSubmitting}
                  className="file-input block w-full text-sm text-gray-500 dark:text-gray-400
                    file:mr-4 file:py-2 file:px-4
                    file:rounded-full file:border-0
                    file:text-sm file:font-semibold
                    file:bg-primary/10 file:text-primary
                    hover:file:bg-primary/20
                    file:cursor-pointer"
                />
                <p className="mt-2 text-sm text-gray-500 dark:text-gray-400">
                  {t("settings.imageRecommendation")}
                </p>
              </div>
            )}
          </div>
        </section>
      </div>
    </Modal>
  );
}
