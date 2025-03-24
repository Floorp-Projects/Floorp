import Navigation from "../../components/Navigation";
import { useTranslation } from "react-i18next";
import { setDefaultBrowser } from "@/app/finish/dataManager.ts";
import { useState } from "react";

export default function FinishPage() {
    const { t } = useTranslation();
    const [isDefaultBrowser, setIsDefaultBrowser] = useState({ message: "", success: false });

    const setAsDefaultBrowser = () => {
        setDefaultBrowser().then((success) => {
            if (success) {
                setIsDefaultBrowser({ message: t('finishPage.defaultBrowserSuccess'), success: true });
            } else {
                setIsDefaultBrowser({ message: t('finishPage.defaultBrowserError'), success: false });
            }
        });
    };

    const closeWelcomePage = () => {
        window.open("about:newtab", "_blank");
        setTimeout(() => {
            window.close();
        }, 50);
    };

    return (
        <main className="flex flex-col justify-between overflow-hidden py-6">
            <div className="container mx-auto px-4 z-10 flex flex-col items-center flex-grow">
                <div className="flex flex-col items-center justify-center mb-6 mt-2">
                    <div className="relative mb-5">
                        <div className="w-20 h-20 md:w-24 md:h-24 rounded-full bg-gradient-to-br from-success to-success/70 flex items-center justify-center shadow-lg shadow-success/30 animate-pulse">
                            <svg xmlns="http://www.w3.org/2000/svg" className="h-12 w-12 md:h-14 md:w-14 text-white" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
                                <path d="M20 6L9 17l-5-5"></path>
                            </svg>
                        </div>
                        <div className="absolute inset-0 rounded-full border-3 border-dashed border-success/40 animate-[spin_10s_linear_infinite]"></div>
                    </div>

                    <h1 id="completion-title" className="text-3xl md:text-4xl lg:text-5xl font-bold text-center text-success mb-4">
                        {t('finishPage.title')}
                    </h1>

                    <p className="text-base md:text-lg text-center max-w-xl mb-5">
                        {t('finishPage.subtitle')}
                    </p>
                </div>

                <div className="card bg-base-100 shadow-xl w-full max-w-2xl backdrop-blur-sm bg-base-100/80 border border-base-300 hover:shadow-2xl transition-all duration-300 mb-6">
                    <div className="card-body p-4 md:p-6">
                        <h2 className="flex items-center gap-2 text-lg md:text-xl font-semibold text-primary mb-3">
                            <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                                <path d="M12 2v4M12 18v4M4.93 4.93l2.83 2.83M16.24 16.24l2.83 2.83M2 12h4M18 12h4M4.93 19.07l2.83-2.83M16.24 7.76l2.83-2.83" />
                            </svg>
                            {t('finishPage.nextSteps')}
                        </h2>

                        {isDefaultBrowser.message && (
                            <div className={`alert ${isDefaultBrowser.success ? 'alert-success' : 'alert-error'} mb-3 py-2`}>
                                <div className="flex-1">
                                    <p className="font-semibold text-sm">{isDefaultBrowser.message}</p>
                                </div>
                            </div>
                        )}

                        <div className="flex flex-col gap-3">
                            <button
                                className="btn btn-primary btn-md md:btn-lg shadow-lg hover:shadow-xl transform hover:-translate-y-1 transition-all duration-300 w-full"
                                onClick={setAsDefaultBrowser}
                            >
                                <svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="mr-2">
                                    <path d="M22 11.08V12a10 10 0 1 1-5.93-9.14" />
                                    <path d="m9 11 3 3L22 4" />
                                </svg>
                                {t('finishPage.defaultBrowser')}
                            </button>

                            <div className="divider text-xs opacity-70 my-1">{t('finishPage.resourcesLabel') || 'リソース'}</div>

                            <div className="flex flex-wrap gap-2 justify-center">
                                <a href="https://github.com/Floorp-Projects/Floorp" target="_blank" rel="noopener noreferrer" className="btn btn-outline btn-sm">
                                    <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="mr-1">
                                        <path d="M9 19c-5 1.5-5-2.5-7-3m14 6v-3.87a3.37 3.37 0 0 0-.94-2.61c3.14-.35 6.44-1.54 6.44-7A5.44 5.44 0 0 0 20 4.77 5.07 5.07 0 0 0 19.91 1S18.73.65 16 2.48a13.38 13.38 0 0 0-7 0C6.27.65 5.09 1 5.09 1A5.07 5.07 0 0 0 5 4.77a5.44 5.44 0 0 0-1.5 3.78c0 5.42 3.3 6.61 6.44 7A3.37 3.37 0 0 0 9 18.13V22" />
                                    </svg>
                                    GitHub
                                </a>
                                <a href="https://twitter.com/floorp_browser" target="_blank" rel="noopener noreferrer" className="btn btn-outline btn-sm">
                                    <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="mr-1">
                                        <path d="M22 4s-.7 2.1-2 3.4c1.6 10-9.4 17.3-18 11.6 2.2.1 4.4-.6 6-2C3 15.5.5 9.6 3 5c2.2 2.6 5.6 4.1 9 4-.9-4.2 4-6.6 7-3.8 1.1 0 3-1.2 3-1.2z" />
                                    </svg>
                                    X（Twitter）
                                </a>
                                <a href="https://docs.floorp.app/docs/features/" target="_blank" rel="noopener noreferrer" className="btn btn-outline btn-sm">
                                    <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="mr-1">
                                        <circle cx="12" cy="12" r="10" />
                                        <path d="M9.09 9a3 3 0 0 1 5.83 1c0 2-3 3-3 3" />
                                        <path d="M12 17h.01" />
                                    </svg>
                                    {t('finishPage.support')}
                                </a>
                            </div>
                        </div>
                    </div>
                </div>

                <div className="flex flex-col items-center">
                    <button
                        className="btn btn-success btn-md md:btn-lg shadow-xl hover:shadow-success/20 transform hover:-translate-y-1 transition-all duration-300 px-6 md:px-8 text-white font-bold"
                        onClick={closeWelcomePage}
                    >
                        <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="mr-2">
                            <path d="M5 12h14"></path>
                            <path d="m12 5 7 7-7 7"></path>
                        </svg>
                        {t('finishPage.getStarted')}
                    </button>

                    <p className="text-xs md:text-sm opacity-70 mt-3 text-center max-w-md px-4">
                        {t('finishPage.hint')}
                    </p>
                </div>
            </div>

            <div className="mt-6">
                <Navigation />
            </div>

            <style dangerouslySetInnerHTML={{
                __html: `
                @keyframes fadeIn {
                    from { opacity: 0; transform: translateY(20px); }
                    to { opacity: 1; transform: translateY(0); }
                }
            `}} />
        </main>
    );
}
