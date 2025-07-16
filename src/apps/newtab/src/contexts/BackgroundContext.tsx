import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useState,
} from "react";
import { getNewTabSettings, saveNewTabSettings } from "../utils/dataManager.ts";

export type BackgroundType = "none" | "random" | "custom" | "folderPath" | "floorp";

interface BackgroundContextType {
  type: BackgroundType;
  customImage: string | null;
  fileName: string | null;
  folderPath: string | null;
  selectedFloorp: string | null;
  setType: (type: BackgroundType) => Promise<void>;
  setCustomImage: (
    image: string | null,
    fileName: string | null,
  ) => Promise<void>;
  setFolderPath: (path: string | null) => Promise<void>;
  setSelectedFloorp: (imageName: string | null) => Promise<void>;
}

const BackgroundContext = createContext<BackgroundContextType | null>(null);

export function BackgroundProvider(
  { children }: { children: React.ReactNode },
) {
  const [type, setType] = useState<BackgroundType>("none");
  const [customImage, setCustomImage] = useState<string | null>(null);
  const [fileName, setFileName] = useState<string | null>(null);
  const [folderPath, setFolderPath] = useState<string | null>(null);
  const [selectedFloorp, setSelectedFloorp] = useState<string | null>(null);
  const [isInitialized, setIsInitialized] = useState(false);

  useEffect(() => {
    const loadSettings = async () => {
      try {
        const settings = await getNewTabSettings();
        setType(settings.background.type);
        setCustomImage(settings.background.customImage);
        setFileName(settings.background.fileName);
        setFolderPath(settings.background.folderPath || null);
        setSelectedFloorp(settings.background.selectedFloorp || null);
      } catch (e) {
        console.error("Failed to load background settings:", e);
      } finally {
        setIsInitialized(true);
      }
    };
    loadSettings();
  }, []);

  const handleSetType = useCallback(async (newType: BackgroundType) => {
    try {
      const settings = await getNewTabSettings();
      await saveNewTabSettings({
        ...settings,
        background: {
          ...settings.background,
          type: newType,
        },
      });
      setType(newType);
    } catch (e) {
      console.error("Failed to save background type:", e);
      throw e;
    }
  }, []);

  const handleSetCustomImage = useCallback(
    async (image: string | null, newFileName: string | null) => {
      try {
        const settings = await getNewTabSettings();
        await saveNewTabSettings({
          ...settings,
          background: {
            ...settings.background,
            type: "custom",
            customImage: image,
            fileName: newFileName,
          },
        });
        setType("custom");
        setCustomImage(image);
        setFileName(newFileName);
      } catch (e) {
        console.error("Failed to save custom image:", e);
        throw e;
      }
    },
    [],
  );

  const handleSetFolderPath = useCallback(async (path: string | null) => {
    try {
      const settings = await getNewTabSettings();
      await saveNewTabSettings({
        ...settings,
        background: {
          ...settings.background,
          type: "folderPath",
          folderPath: path,
        },
      });
      setType("folderPath");
      setFolderPath(path);
    } catch (e) {
      console.error("Failed to save folder path:", e);
      throw e;
    }
  }, []);

  const handleSetSelectedFloorp = useCallback(async (imageName: string | null) => {
    try {
      const settings = await getNewTabSettings();
      await saveNewTabSettings({
        ...settings,
        background: {
          ...settings.background,
          type: "floorp",
          selectedFloorp: imageName,
        },
      });
      setType("floorp");
      setSelectedFloorp(imageName);
    } catch (e) {
      console.error("Failed to save selected Floorp image:", e);
      throw e;
    }
  }, []);

  if (!isInitialized) {
    return null;
  }

  return (
    <BackgroundContext.Provider
      value={{
        type,
        customImage,
        fileName,
        folderPath,
        selectedFloorp,
        setType: handleSetType,
        setCustomImage: handleSetCustomImage,
        setFolderPath: handleSetFolderPath,
        setSelectedFloorp: handleSetSelectedFloorp,
      }}
    >
      {children}
    </BackgroundContext.Provider>
  );
}

export function useBackground() {
  const context = useContext(BackgroundContext);
  if (!context) {
    throw new Error("useBackground must be used within a BackgroundProvider");
  }
  return context;
}
