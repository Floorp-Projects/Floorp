import { createContext, useContext, useEffect, useState } from "react";
import { getThemeSetting, setThemeSetting } from "../app/customize/dataManager";

type Theme = "light" | "dark" | "system";

type ThemeProviderProps = {
  children: React.ReactNode;
  defaultTheme?: Theme;
  storageKey?: string;
};

type ThemeProviderState = {
  theme: Theme;
  setTheme: (theme: Theme) => void;
};

const initialState: ThemeProviderState = {
  theme: "system",
  setTheme: () => null,
};

const ThemeProviderContext = createContext<ThemeProviderState>(initialState);

export function ThemeProvider({
  children,
  defaultTheme = "system",
  storageKey = "floorp-ui-theme",
  ...props
}: ThemeProviderProps) {
  const [theme, setTheme] = useState<Theme>(defaultTheme);

  useEffect(() => {
    const fetchTheme = async () => {
      try {
        const themeValue = await getThemeSetting();
        if (themeValue === 1) {
          setTheme("light");
        } else if (themeValue === 2) {
          setTheme("dark");
        } else {
          setTheme("system");
        }
      } catch (error) {
        console.error("テーマ設定の取得に失敗しました:", error);
      }
    };

    fetchTheme();
  }, []);

  useEffect(() => {
    const root = window.document.documentElement;

    root.classList.remove("light", "dark");

    if (theme === "system") {
      const systemTheme = window.matchMedia("(prefers-color-scheme: dark)")
        .matches
        ? "dark"
        : "light";

      root.classList.add(systemTheme);
      root.setAttribute("data-theme", systemTheme);
      return;
    }

    root.classList.add(theme);
    root.setAttribute("data-theme", theme);
  }, [theme]);

  const value = {
    theme,
    setTheme: (newTheme: Theme) => {
      setThemeSetting(newTheme).catch(error => {
        console.error("テーマ設定の保存に失敗しました:", error);
      });
      setTheme(newTheme);
    },
  };

  return (
    <ThemeProviderContext.Provider {...props} value={value}>
      {children}
    </ThemeProviderContext.Provider>
  );
}

export const useTheme = () => {
  const context = useContext(ThemeProviderContext);

  if (context === undefined)
    throw new Error("useTheme must be used within a ThemeProvider");

  return context;
};
