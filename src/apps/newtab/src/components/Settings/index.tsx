import { useEffect, useState } from "react";
import { Modal } from "../Modal/index.tsx";
import { useBackground } from "@/contexts/BackgroundContext.tsx";
import { useComponents } from "@/contexts/ComponentsContext.tsx";
import { getBackgroundImageCount } from "@/utils/backgroundImages.ts";

export function Settings(
  { isOpen, onClose }: { isOpen: boolean; onClose: () => void },
) {
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
    <Modal isOpen={isOpen} onClose={onClose} title="新しいタブの設定">
      <div className="space-y-6">
        <section>
          <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-4">
            表示コンポーネント
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
                トップサイト
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
              <span className="text-gray-700 dark:text-gray-200">時計</span>
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
                検索バー
              </span>
            </label>
          </div>
        </section>

        <section>
          <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-4">
            背景設定
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
              <span className="text-gray-700 dark:text-gray-200">背景なし</span>
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
                ランダム画像 ({imageCount}枚)
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
              <span className="text-gray-700 dark:text-gray-200">指定画像</span>
            </label>

            {backgroundType === "custom" && (
              <div className="mt-4 pl-8">
                {currentFileName && (
                  <div className="mb-2 text-sm text-gray-600 dark:text-gray-400">
                    現在の画像: {currentFileName}
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
                  推奨: 1920x1080以上の画像
                </p>
              </div>
            )}
          </div>
        </section>

        <div className="flex justify-end space-x-3 mt-6 pt-6 border-t border-gray-200 dark:border-gray-700">
          <button
            type="button"
            onClick={onClose}
            disabled={isSubmitting}
            className="px-4 py-2 text-sm font-medium text-gray-700 dark:text-gray-200 bg-white dark:bg-gray-700 border border-gray-300 dark:border-gray-600 rounded-md hover:bg-gray-50 dark:hover:bg-gray-600 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-primary disabled:opacity-50"
          >
            閉じる
          </button>
        </div>
      </div>
    </Modal>
  );
}
