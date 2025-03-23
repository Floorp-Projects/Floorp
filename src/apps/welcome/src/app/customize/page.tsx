import { useState, useEffect } from "react";
import { useTranslation } from "react-i18next";
import Navigation from "../../components/Navigation.tsx";
import { getSearchEngines, getDefaultEngine, setDefaultEngine, getThemeSetting, setThemeSetting } from "./dataManager.ts";
import type { SearchEngine } from "./types.ts";

export default function CustomizePage() {
    const { t } = useTranslation();

    const themes = [
        { id: "light", name: t("customize.themes.light"), icon: "☀️" },
        { id: "dark", name: t("customize.themes.dark"), icon: "🌙" },
        { id: "system", name: t("customize.themes.system"), icon: "🖥️" }
    ];

    const [selectedTheme, setSelectedTheme] = useState<"system" | "light" | "dark">("system");
    const [selectedEngine, setSelectedEngine] = useState("");
    const [searchEngines, setSearchEngines] = useState<SearchEngine[]>([]);
    const [loading, setLoading] = useState(true);

    const applyThemeToPage = (theme: "system" | "light" | "dark") => {
        let dataTheme: string;

        if (theme === "light") {
            dataTheme = "light";
        } else if (theme === "dark") {
            dataTheme = "dark";
        } else {
            const prefersDark = window.matchMedia('(prefers-color-scheme: dark)').matches;
            dataTheme = prefersDark ? "dark" : "light";
        }

        document.documentElement.setAttribute('data-theme', dataTheme);
    };

    useEffect(() => {
        const fetchData = async () => {
            setLoading(true);
            try {
                const engines = await getSearchEngines();
                setSearchEngines(engines);

                const defaultEngine = await getDefaultEngine();
                setSelectedEngine(defaultEngine.identifier);

                const themePref = await getThemeSetting();
                let currentTheme: "system" | "light" | "dark" = "system";
                if (themePref === 1) {
                    currentTheme = "light";
                } else if (themePref === 2) {
                    currentTheme = "dark";
                } else {
                    currentTheme = "system";
                }
                setSelectedTheme(currentTheme);

                applyThemeToPage(currentTheme);
            } catch (error) {
                console.error("Failed to retrieve settings:", error);
            } finally {
                setLoading(false);
            }
        };

        fetchData();

        const mediaQuery = window.matchMedia('(prefers-color-scheme: dark)');
        const handleSystemThemeChange = () => {
            if (selectedTheme === "system") {
                applyThemeToPage("system");
            }
        };

        mediaQuery.addEventListener('change', handleSystemThemeChange);

        return () => {
            mediaQuery.removeEventListener('change', handleSystemThemeChange);
        };
    }, [selectedTheme]);

    const handleEngineChange = async (engineId: string) => {
        setSelectedEngine(engineId);
        try {
            const result = await setDefaultEngine(engineId);
            if (!result.success) {
                console.error("Failed to set search engine");
            }
        } catch (error) {
            console.error("Error occurred while setting search engine:", error);
        }
    };

    const handleThemeChange = async (themeId: "system" | "light" | "dark") => {
        setSelectedTheme(themeId);

        applyThemeToPage(themeId);

        try {
            await setThemeSetting(themeId);
        } catch (error) {
            console.error("Error occurred while setting theme:", error);
        }
    };

    const getEngineIcon = (engine: SearchEngine) => {
        if (engine.iconURL) {
            return <img src={engine.iconURL} alt={engine.name} className="w-5 h-5 mr-2 object-contain" />;
        }
        return (
            <div className="w-5 h-5 mr-2 flex items-center justify-center bg-base-300 rounded">
                <span className="text-xs">🔍</span>
            </div>
        );
    };

    const getEngineName = (engine: SearchEngine) => {
        const maxLength = 15;
        if (engine.name.length > maxLength) {
            return `${engine.name.substring(0, maxLength)}...`;
        }
        return engine.name;
    };

    const ThemePreview = () => {
        const currentEngine = searchEngines.find(e => e.identifier === selectedEngine);

        return (
            <div className="w-full h-full flex items-center justify-center">
                <div className="relative w-full max-w-md bg-base-300 rounded-lg p-4 shadow-lg">
                    <div className={`mockup-browser border ${selectedTheme === 'dark' ? 'bg-neutral text-neutral-content' : 'bg-base-200'}`}>
                        <div className="mockup-browser-toolbar">
                            <div className="input border border-base-content/20 flex items-center">
                                {currentEngine && getEngineIcon(currentEngine)}
                                <span className="truncate">
                                    {currentEngine?.name || t("customize.searchEngine.default")} {t("customize.searchEngine.search")}
                                </span>
                            </div>
                        </div>
                        <div className={`flex flex-col px-4 py-8 ${selectedTheme === 'dark' ? 'bg-neutral-focus' : 'bg-base-100'}`}>
                            <div className="flex items-center mb-2">
                                <div className="w-4 h-4 rounded-full bg-primary mr-2"></div>
                                <div className="h-2 bg-base-content/20 rounded w-24"></div>
                            </div>
                            <div className="h-2 bg-base-content/20 rounded w-full mb-1.5"></div>
                            <div className="h-2 bg-base-content/20 rounded w-5/6 mb-1.5"></div>
                            <div className="h-2 bg-base-content/20 rounded w-4/6"></div>
                        </div>
                    </div>
                    <div className="text-center mt-2">
                        <span className="text-sm opacity-70">
                            {selectedTheme === 'light'
                                ? t("customize.themeInfo.light")
                                : selectedTheme === 'dark'
                                    ? t("customize.themeInfo.dark")
                                    : t("customize.themeInfo.system")
                            }
                        </span>
                    </div>
                </div>
            </div>
        );
    };

    const renderEngines = () => {
        if (loading) {
            return (
                <div className="flex justify-center my-4">
                    <div className="loading loading-spinner loading-md"></div>
                </div>
            );
        }

        if (searchEngines.length === 0) {
            return (
                <div className="alert alert-info">
                    <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" className="stroke-current shrink-0 w-6 h-6"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M13 16h-1v-4h-1m1-4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"></path></svg>
                    <span>{t("customize.searchEngine.noEnginesFound")}</span>
                </div>
            );
        }

        return (
            <div className="grid grid-cols-2 gap-3 my-2">
                {searchEngines.map((engine) => (
                    <div
                        key={engine.identifier}
                        className={`btn ${selectedEngine === engine.identifier ? 'btn-primary' : 'btn-outline'} justify-start overflow-hidden`}
                        onClick={() => handleEngineChange(engine.identifier)}
                        title={engine.description || engine.name}
                    >
                        {getEngineIcon(engine)}
                        <span className="truncate">{getEngineName(engine)}</span>
                        {engine.isDefault && (
                            <span className="badge badge-sm badge-accent ml-auto">{t("customize.searchEngine.default")}</span>
                        )}
                    </div>
                ))}
            </div>
        );
    };

    return (
        <div className="flex flex-col gap-8">
            <div className="text-center">
                <h1 className="text-4xl font-bold mb-4">{t("customize.title")}</h1>
                <p className="text-lg">{t("customize.description")}</p>
            </div>

            <div className="card bg-base-200 shadow-xl">
                <div className="card-body p-0 overflow-hidden">
                    <div className="grid grid-cols-1 md:grid-cols-2">
                        <div className="p-6 flex flex-col justify-center">
                            <div className="flex items-centermb-6 gap-2">
                                <div className="mb-4 text-primary">
                                    <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><circle cx="12" cy="12" r="4" /><path d="M12 2v2" /><path d="M12 20v2" /><path d="m4.93 4.93 1.41 1.41" /><path d="m17.66 17.66 1.41 1.41" /><path d="M2 12h2" /><path d="M20 12h2" /><path d="m6.34 17.66-1.41 1.41" /><path d="m19.07 4.93-1.41 1.41" /></svg>
                                </div>
                                <h2 className="text-2xl font-bold mb-4">{t("customize.appearance.title")}</h2>
                            </div>
                            <p className="mb-6">
                                {t("customize.appearance.description")}
                            </p>

                            <div className="flex flex-wrap gap-3 my-2">
                                {themes.map((theme) => (
                                    <div
                                        key={theme.id}
                                        className={`btn ${selectedTheme === theme.id ? 'btn-primary' : 'btn-outline'}`}
                                        onClick={() => handleThemeChange(theme.id as "system" | "light" | "dark")}
                                    >
                                        <span className="mr-2">{theme.icon}</span>
                                        {theme.name}
                                    </div>
                                ))}
                            </div>

                            <h2 className="text-2xl font-bold mt-6 mb-3">{t("customize.searchEngine.title")}</h2>
                            {renderEngines()}
                        </div>
                        <div className="bg-base-300 p-6 flex items-center justify-center">
                            <ThemePreview />
                        </div>
                    </div>
                </div>
            </div>
            <Navigation />
        </div>
    );
} 