import { useState, useEffect } from "react";
import Navigation from "../../components/Navigation.tsx";
import { getLocaleData, installLangPack, setAppLocale, getNativeNames } from "./dataManager.ts";
import type { LangPack, LocaleData } from "./type.ts";
import { useTranslation } from "react-i18next";
import { Globe, ChevronDown, AlertCircle, Languages } from "lucide-react";

export default function LocalizationPage() {
    const { t, i18n } = useTranslation();
    const [localeData, setlocaleData] = useState<LocaleData | null>(null);
    const [loading, setLoading] = useState(true);
    const [selectedLocale, setSelectedLocale] = useState<string>("");
    const [installingLanguage, setInstallingLanguage] = useState(false);
    const [nativeLanguageNames, setNativeLanguageNames] = useState<Record<string, string>>({});

    useEffect(() => {
        fetchLocaleInfo();
    }, []);

    useEffect(() => {
        if (localeData && localeData.availableLocales.length > 0) {
            fetchNativeNames(localeData.availableLocales);
        }
    }, [localeData]);

    const fetchNativeNames = async (availableLocales: LangPack[]) => {
        try {
            const locales = availableLocales.map(langPack => getLocaleFromLangPack(langPack));
            const nativeNames = await getNativeNames(locales);
            const namesMap: Record<string, string> = {};
            locales.forEach((locale, index) => {
                namesMap[locale] = nativeNames[index] || locale;
            });
            setNativeLanguageNames(namesMap);
        } catch (error) {
            console.error("Failed to get native language name:", error);
        }
    };

    const fetchLocaleInfo = async () => {
        try {
            setLoading(true);
            const data = await getLocaleData();
            setlocaleData(data);
            setSelectedLocale(data.localeInfo.appLocaleRaw);
        } catch (error) {
            console.error("Failed to get language information:", error);
        } finally {
            setLoading(false);
        }
    };

    const getLocaleFromLangPack = (langPack: LangPack): string => {
        return langPack.target_locale;
    };


    const handleLocaleChange = async (locale: string) => {
        if (installingLanguage || locale === selectedLocale) {
            return
        };

        try {
            setInstallingLanguage(true);

            if (!localeData?.installedLocales.includes(locale)) {
                const targetLangPack = localeData?.availableLocales.find(langPack => getLocaleFromLangPack(langPack) === locale);
                await installLangPack(targetLangPack!);
            }

            await setAppLocale(locale);
            await i18n.changeLanguage(locale);

            setInstallingLanguage(false);
            setSelectedLocale(locale);

        } catch (error) {
            console.error("Failed to change locale:", error);
            setInstallingLanguage(false);
        }
    };

    const getLanguageName = (langCode: string) => {
        if (nativeLanguageNames[langCode]) {
            return nativeLanguageNames[langCode];
        }
        return langCode;
    };

    const getSortedLanguages = () => {
        if (!localeData) return [];
        return [...localeData.availableLocales].sort((a, b) => {
            const localeA = getLocaleFromLangPack(a);
            const localeB = getLocaleFromLangPack(b);
            const nameA = getLanguageName(localeA);
            const nameB = getLanguageName(localeB);
            return nameA.localeCompare(nameB);
        });
    };

    return (
        <div className="flex flex-col gap-8">
            <div className="text-center">
                <h1 className="text-4xl font-bold mb-4">{t('localizationPage.title')}</h1>
                <p className="text-lg">{t('localizationPage.subtitle')}</p>
            </div>

            {loading ? (
                <div className="flex justify-center">
                    <div className="loading loading-spinner loading-lg"></div>
                </div>
            ) : !localeData ? (
                <div className="alert alert-error">
                    <svg xmlns="http://www.w3.org/2000/svg" className="stroke-current shrink-0 h-6 w-6" fill="none" viewBox="0 0 24 24"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M10 14l2-2m0 0l2-2m-2 2l-2-2m2 2l2 2m7-2a9 9 0 11-18 0 9 9 0 0118 0z" /></svg>
                    <span>Failed to retrieve language information. Please try reloading.</span>
                </div>
            ) : (
                <div className="card bg-base-200 shadow-xl">
                    <div className="card-body">
                        <h2 className="card-title">
                            <Globe className="text-primary mr-2" size={24} />
                            {t('localizationPage.languageSettings')}
                        </h2>

                        <div className="flex flex-row gap-4 mt-2">
                            <div className="flex justify-center items-center w-2/5">
                                <div className="text-center">
                                    <Languages size={160} className="text-primary mx-auto mb-4" />
                                    <div className="font-semibold text-lg">{t('localizationPage.multilingual')}</div>
                                    <p className="text-sm mt-2">{t('localizationPage.chooseLanguage')}</p>
                                </div>
                            </div>

                            <div className="flex flex-col gap-4 w-3/4">
                                <div className="bg-base-300 p-3 rounded-lg">
                                    <h3 className="font-bold mb-2">{t('localizationPage.currentLanguageInfo')}</h3>
                                    <div className="flex flex-col gap-2">
                                        <div className="flex items-center">
                                            <div>
                                                <div className="text-xs opacity-70">{t('localizationPage.systemLanguage')}</div>
                                                <div className="font-bold text-sm">{localeData.localeInfo.displayNames?.systemLanguage || localeData.localeInfo.systemLocaleRaw}</div>
                                            </div>
                                        </div>
                                        <div className="flex items-center">
                                            <div>
                                                <div className="text-xs opacity-70">{t('localizationPage.floorpLanguage')}</div>
                                                <div className="font-bold text-sm">{getLanguageName(selectedLocale)}</div>
                                            </div>
                                        </div>
                                    </div>
                                </div>
                                <div className="bg-base-300 p-4 rounded-lg">
                                    <h3 className="font-bold mb-2">{t('localizationPage.selectLanguage')}</h3>
                                    <div className="mb-3">
                                        <button
                                            className="btn btn-sm btn-primary w-full justify-start"
                                            onClick={() => handleLocaleChange(localeData.localeInfo.systemLocale.language)}
                                            disabled={installingLanguage || localeData.localeInfo.systemLocaleRaw === selectedLocale}
                                        >
                                            <span>{t('localizationPage.useSystemLanguage')} ({localeData.localeInfo.systemLocale.language})</span>
                                        </button>
                                    </div>

                                    <div className="dropdown dropdown-bottom w-full">
                                        <label tabIndex={0} className="btn btn-sm btn-outline w-full flex justify-between items-center">
                                            <span className="flex items-center">
                                                <span className="truncate">{getLanguageName(selectedLocale)}</span>
                                            </span>
                                            <ChevronDown size={16} />
                                        </label>
                                        <div tabIndex={0} className="dropdown-content menu p-2 shadow bg-base-100 rounded-box max-h-70 w-full overflow-y-auto flex flex-col flex-nowrap">
                                            {getSortedLanguages().map((langPack) => {
                                                const locale = getLocaleFromLangPack(langPack);
                                                if (locale === selectedLocale) {
                                                    return null
                                                };
                                                return (
                                                    <li key={locale} onClick={() => {
                                                        handleLocaleChange(locale);
                                                        document.activeElement instanceof HTMLElement && document.activeElement.blur();
                                                    }} className="w-full">
                                                        <a className={selectedLocale === locale ? 'active' : ''}>
                                                            <span className="truncate">{getLanguageName(locale)}</span>
                                                        </a>
                                                    </li>
                                                );
                                            })}
                                        </div>
                                    </div>

                                    {installingLanguage && (
                                        <div className="mt-3 flex justify-center items-center">
                                            <div className="loading loading-spinner loading-xs mr-2"></div>
                                            <span className="text-sm">Changing language...</span>
                                        </div>
                                    )}
                                </div>

                                <div className="alert bg-base-100">
                                    <AlertCircle className="stroke-info shrink-0 w-6 h-6" />
                                    <div className="text-sm">
                                        {t('localizationPage.languageChangeNotice')}
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            )}

            <Navigation />
        </div>
    );
}
