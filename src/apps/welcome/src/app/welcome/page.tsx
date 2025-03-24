import Navigation from "../../components/Navigation";
import { useTranslation } from "react-i18next";
import { Shield, Zap, Palette, ArrowRight } from "lucide-react";
import { useNavigate } from "react-router-dom";

export default function WelcomePage() {
    const { t } = useTranslation();
    const navigate = useNavigate();

    const handleStart = () => {
        navigate("/localization");
    };

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

                        <button
                            className="btn btn-primary btn-lg shadow-xl hover:shadow-primary/20 transform hover:-translate-y-1 transition-all duration-300 px-8 text-white font-bold mt-4"
                            onClick={handleStart}
                        >
                            <ArrowRight className="mr-2" size={20} />
                            {t('welcomePage.start')}
                        </button>
                    </div>
                </div>
            </div>

            <div className="card bg-base-200 rounded-2xl w-full">
                <div className="card-body">
                    <h2 className="card-title">{t('welcomePage.features')}</h2>
                    <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                        <div className="stat">
                            <div className="stat-figure text-primary">
                                <Shield size={24} />
                            </div>
                            <div className="stat-title">{t('welcomePage.privacy')}</div>
                            <div className="stat-desc">{t('welcomePage.privacyDesc')}</div>
                        </div>
                        <div className="stat">
                            <div className="stat-figure text-primary">
                                <Zap size={24} />
                            </div>
                            <div className="stat-title">{t('welcomePage.speed')}</div>
                            <div className="stat-desc">{t('welcomePage.speedDesc')}</div>
                        </div>
                        <div className="stat">
                            <div className="stat-figure text-primary">
                                <Palette size={24} />
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