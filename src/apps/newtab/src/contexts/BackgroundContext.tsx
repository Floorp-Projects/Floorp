import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useState,
} from "react";
import { getNewTabSettings, saveNewTabSettings } from "../utils/dataManager.ts";

export type BackgroundType = "none" | "random" | "custom";

interface BackgroundContextType {
  type: BackgroundType;
  customImage: string | null;
  fileName: string | null;
  setType: (type: BackgroundType) => Promise<void>;
  setCustomImage: (
    image: string | null,
    fileName: string | null,
  ) => Promise<void>;
}

const BackgroundContext = createContext<BackgroundContextType | null>(null);

export function BackgroundProvider(
  { children }: { children: React.ReactNode },
) {
  const [type, setType] = useState<BackgroundType>("none");
  const [customImage, setCustomImage] = useState<string | null>(null);
  const [fileName, setFileName] = useState<string | null>(null);
  const [isInitialized, setIsInitialized] = useState(false);

  useEffect(() => {
    const loadSettings = async () => {
      try {
        const settings = await getNewTabSettings();
        setType(settings.background.type);
        setCustomImage(settings.background.customImage);
        setFileName(settings.background.fileName);
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

  if (!isInitialized) {
    return null;
  }

  return (
    <BackgroundContext.Provider
      value={{
        type,
        customImage,
        fileName,
        setType: handleSetType,
        setCustomImage: handleSetCustomImage,
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
