import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useState,
} from "react";
import { getNewTabSettings, saveNewTabSettings } from "../utils/dataManager.ts";

interface ComponentsState {
  topSites: boolean;
  clock: boolean;
  weather: boolean;
}

interface ComponentsContextType {
  components: ComponentsState;
  toggleComponent: (key: keyof ComponentsState) => Promise<void>;
}

const ComponentsContext = createContext<ComponentsContextType | null>(null);

export function ComponentsProvider(
  { children }: { children: React.ReactNode },
) {
  const [components, setComponents] = useState<ComponentsState>({
    topSites: true,
    clock: true,
    weather: true,
  });
  const [isInitialized, setIsInitialized] = useState(false);

  useEffect(() => {
    const loadSettings = async () => {
      try {
        const settings = await getNewTabSettings();
        setComponents(settings.components);
      } catch (e) {
        console.error("Failed to load component settings:", e);
      } finally {
        setIsInitialized(true);
      }
    };
    loadSettings();
  }, []);

  const toggleComponent = useCallback(async (key: keyof ComponentsState) => {
    try {
      const settings = await getNewTabSettings();
      const newComponents = {
        ...settings.components,
        [key]: !settings.components[key],
      };

      await saveNewTabSettings({
        ...settings,
        components: newComponents,
      });

      setComponents(newComponents);
    } catch (e) {
      console.error("Failed to save component settings:", e);
      throw e;
    }
  }, []);

  if (!isInitialized) {
    return null;
  }

  return (
    <ComponentsContext.Provider
      value={{
        components,
        toggleComponent,
      }}
    >
      {children}
    </ComponentsContext.Provider>
  );
}

export function useComponents() {
  const context = useContext(ComponentsContext);
  if (!context) {
    throw new Error("useComponents must be used within a ComponentsProvider");
  }
  return context;
}
