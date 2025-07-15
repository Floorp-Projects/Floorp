import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { Modal } from "../Modal/index.tsx";
import { useBackground } from "@/contexts/BackgroundContext.tsx";
import { useComponents } from "@/contexts/ComponentsContext.tsx";
import { getFloorpImages } from "@/utils/backgroundImages.ts";
import { getFolderPathFromDialog } from "@/utils/dataManager.ts";

export function Settings(
  { isOpen, onClose }: { isOpen: boolean; onClose: () => void },
) {
  const { t } = useTranslation();
  const {
    type: backgroundType,
    fileName,
    folderPath,
    selectedFloorp,
    slideshow,
    setType: setBackgroundType,
    setCustomImage,
    setFolderPath,
    setSelectedFloorp,
    setSlideshow,
  } = useBackground();
  const { components, toggleComponent } = useComponents();
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [currentFileName, setCurrentFileName] = useState<string>("");
  const [currentFolderPath, setCurrentFolderPath] = useState<string>("");
  const [floorpImages, setFloorpImages] = useState<
    { name: string; url: string }[]
  >([]);

  useEffect(() => {
    if (backgroundType === "custom" && fileName) {
      setCurrentFileName(fileName);
    } else {
      setCurrentFileName("");
    }

    if (backgroundType === "folderPath" && folderPath) {
      setCurrentFolderPath(folderPath);
    } else {
      setCurrentFolderPath("");
    }

    setFloorpImages(getFloorpImages());
  }, [backgroundType, fileName, folderPath]);

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

  const handleFolderSelect = async () => {
    setIsSubmitting(true);
    try {
      const folderPath = await getFolderPathFromDialog();
      if (folderPath) {
        await setFolderPath(folderPath.path);
      }
    } finally {
      setIsSubmitting(false);
    }
  };

  const handleFloorpImageSelect = async (imageName: string) => {
    setIsSubmitting(true);
    try {
      await setSelectedFloorp(imageName);
    } catch (error) {
      console.error("Failed to select Floorp image:", error);
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

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title={t("settings.newTabSettings")}
    >
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
              <span className="text-gray-700 dark:text-gray-200">
                {t("settings.clock")}
              </span>
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
              <span className="text-gray-700 dark:text-gray-200">
                {t("settings.noBackground")}
              </span>
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
              <span className="text-gray-700 dark:text-gray-200">
                {t("settings.customImage")}
              </span>
            </label>

            <label className="flex items-center space-x-3">
              <input
                type="radio"
                name="background"
                value="folderPath"
                checked={backgroundType === "folderPath"}
                onChange={() => handleBackgroundTypeChange("folderPath")}
                disabled={isSubmitting}
                className="form-radio h-5 w-5 text-primary border-gray-300 dark:border-gray-600 focus:ring-primary"
              />
              <span className="text-gray-700 dark:text-gray-200">
                {t("settings.folderImages")}
              </span>
            </label>

            <label className="flex items-center space-x-3">
              <input
                type="radio"
                name="background"
                value="floorp"
                checked={backgroundType === "floorp"}
                onChange={() => handleBackgroundTypeChange("floorp")}
                disabled={isSubmitting}
                className="form-radio h-5 w-5 text-primary border-gray-300 dark:border-gray-600 focus:ring-primary"
              />
              <span className="text-gray-700 dark:text-gray-200">
                {t("settings.floorpImages")}
              </span>
            </label>

            {(backgroundType === "random" || backgroundType === "folderPath") &&
              (
                <div className="mt-4 pl-8 space-y-4">
                  <div className="flex items-center space-x-3">
                    <input
                      type="checkbox"
                      checked={slideshow.enabled}
                      onChange={(e) => setSlideshow(e.target.checked)}
                      disabled={isSubmitting}
                      className="form-checkbox h-5 w-5 text-primary rounded border-gray-300 dark:border-gray-600 focus:ring-primary"
                    />
                    <span className="text-gray-700 dark:text-gray-200">
                      {t("settings.enableSlideshow")}
                    </span>
                  </div>

                  {slideshow.enabled && (
                    <div className="flex items-center space-x-3">
                      <label className="text-sm text-gray-700 dark:text-gray-200">
                        {t("settings.slideshowInterval")}:
                      </label>
                      <select
                        value={slideshow.interval}
                        onChange={(e) =>
                          setSlideshow(true, parseInt(e.target.value))}
                        disabled={isSubmitting}
                        className="form-select text-sm border-gray-300 dark:border-gray-600 rounded focus:ring-primary focus:border-primary"
                      >
                        <option value={10}>10 {t("settings.seconds")}</option>
                        <option value={15}>15 {t("settings.seconds")}</option>
                        <option value={30}>30 {t("settings.seconds")}</option>
                        <option value={60}>1 {t("settings.minute")}</option>
                        <option value={120}>2 {t("settings.minutes")}</option>
                        <option value={300}>5 {t("settings.minutes")}</option>
                      </select>
                    </div>
                  )}
                </div>
              )}

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

            {backgroundType === "folderPath" && (
              <div className="mt-4 pl-8">
                {currentFolderPath && (
                  <div className="mb-2 text-sm text-gray-600 dark:text-gray-400">
                    {t("settings.currentFolder")} {currentFolderPath}
                  </div>
                )}
                <button
                  onClick={handleFolderSelect}
                  disabled={isSubmitting}
                  className="px-4 py-2 bg-primary/10 text-primary rounded-full text-sm font-semibold hover:bg-primary/20 transition-colors"
                >
                  {t("settings.selectFolder")}
                </button>
              </div>
            )}

            {backgroundType === "floorp" && (
              <div className="mt-4 pl-8">
                <div className="grid grid-cols-3 gap-4">
                  {floorpImages.map((image) => (
                    <div
                      key={image.name}
                      className={`
                        relative cursor-pointer rounded-lg overflow-hidden border-2
                        ${
                        selectedFloorp === image.name
                          ? "border-primary"
                          : "border-transparent"
                      }
                      `}
                      onClick={() => handleFloorpImageSelect(image.name)}
                    >
                      <img
                        src={image.url}
                        alt={image.name}
                        className="w-full h-auto aspect-video object-cover"
                      />
                      {selectedFloorp === image.name && (
                        <div className="absolute inset-0 bg-primary/20 flex items-center justify-center">
                          <span className="bg-primary text-white px-2 py-1 rounded text-xs">
                            {t("settings.selected")}
                          </span>
                        </div>
                      )}
                    </div>
                  ))}
                </div>
              </div>
            )}
          </div>
        </section>
      </div>
    </Modal>
  );
}
