import Navigation from "../../components/Navigation";
import { useTranslation } from "react-i18next";

export default function WelcomePage() {
    const { t } = useTranslation();

    return (
        <div className="flex flex-col items-center gap-8">
            <div className="hero rounded-box p-8">
                <div className="hero-content text-center">
                    <div className="max-w-xl">
                        <div className="avatar mb-6">
                            <div className="w-32 ring-primary ring-offset-base-100 ring-offset-2 mx-auto">
                                <img src="chrome://branding/content/icon128.png" alt="Floorp Logo" />
                            </div>
                        </div>
                        <h1 className="text-5xl font-bold mb-6">{t('welcomePage.welcome')}</h1>

                        <p className="py-4">
                            {t('welcomePage.description')}
                        </p>
                        <p className="py-2">
                            {t('welcomePage.readyToStart')}
                        </p>
                    </div>
                </div>
            </div>

            <div className="card bg-base-200 rounded-2xl w-full">
                <div className="card-body">
                    <h2 className="card-title">{t('welcomePage.features')}</h2>
                    <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                        <div className="stat">
                            <div className="stat-figure text-primary">
                                <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="lucide lucide-shield"><path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10" /></svg>
                            </div>
                            <div className="stat-title">{t('welcomePage.privacy')}</div>
                            <div className="stat-desc">{t('welcomePage.privacyDesc')}</div>
                        </div>
                        <div className="stat">
                            <div className="stat-figure text-primary">
                                <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="lucide lucide-zap"><polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2" /></svg>
                            </div>
                            <div className="stat-title">{t('welcomePage.speed')}</div>
                            <div className="stat-desc">{t('welcomePage.speedDesc')}</div>
                        </div>
                        <div className="stat">
                            <div className="stat-figure text-primary">
                                <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="lucide lucide-palette"><circle cx="13.5" cy="6.5" r=".5" /><circle cx="17.5" cy="10.5" r=".5" /><circle cx="8.5" cy="7.5" r=".5" /><circle cx="6.5" cy="12.5" r=".5" /><path d="M12 2C6.5 2 2 6.5 2 12s4.5 10 10 10c.926 0 1.648-.746 1.648-1.688 0-.437-.18-.835-.437-1.125-.29-.289-.438-.652-.438-1.125a1.64 1.64 0 0 1 1.668-1.668h1.996c3.051 0 5.555-2.503 5.555-5.554C21.965 6.012 17.461 2 12 2z" /></svg>
                            </div>
                            <div className="stat-title">{t('welcomePage.customization')}</div>
                            <div className="stat-desc">{t('welcomePage.customizationDesc')}</div>
                        </div>
                    </div>
                </div>
            </div>

            <Navigation />
        </div>
    );
} 