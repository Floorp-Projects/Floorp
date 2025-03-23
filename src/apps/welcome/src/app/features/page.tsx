import Navigation from "../../components/Navigation";
import { useTranslation } from "react-i18next";

export default function FeaturesPage() {
    const { t } = useTranslation();

    const panelSidebarImage = (
        <div className="w-full h-full flex items-center justify-center">
            <div className="w-full max-w-[200px] aspect-[4/3] bg-base-300 rounded-lg p-2 overflow-hidden shadow-lg">
                <div className="flex h-full">
                    <div className="w-12 bg-base-200 h-full rounded-l-md flex flex-col gap-2 p-1">
                        <div className="w-full aspect-square bg-primary/20 rounded-md flex items-center justify-center text-sm">🌐</div>
                        <div className="w-full aspect-square bg-secondary/20 rounded-md flex items-center justify-center text-sm">📝</div>
                        <div className="w-full aspect-square bg-accent/20 rounded-md flex items-center justify-center text-sm">🧩</div>
                        <div className="flex-1"></div>
                        <div className="w-full aspect-square bg-base-300 rounded-md flex items-center justify-center text-sm">⚙️</div>
                    </div>

                    <div className="flex-1 p-2">
                        <div className="text-xs font-bold mb-1">{t('featuresPage.notebook')}</div>
                        <div className="h-3 bg-base-content/10 rounded-sm mb-1 w-full"></div>
                        <div className="h-3 bg-base-content/10 rounded-sm mb-1 w-5/6"></div>
                        <div className="h-3 bg-base-content/10 rounded-sm mb-1 w-4/6"></div>
                        <div className="h-3 bg-base-content/10 rounded-sm mb-2 w-3/6"></div>
                        <div className="flex justify-end">
                            <div className="btn btn-xs btn-primary scale-75">Save</div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );

    const workspaceImage = (
        <div className="w-full h-full flex items-center justify-center">
            <div className="w-full max-w-[200px] aspect-[4/3] bg-base-300 rounded-lg p-2 overflow-hidden shadow-lg">
                <div className="text-xs font-bold mb-1">Workspaces</div>
                <div className="grid grid-cols-3 gap-1">
                    <div className="flex flex-col items-center">
                        <div className="w-8 h-8 rounded-lg bg-primary/30 flex items-center justify-center text-base mb-1">💼</div>
                        <span className="text-[10px]">Work</span>
                    </div>
                    <div className="flex flex-col items-center">
                        <div className="w-8 h-8 rounded-lg bg-secondary/30 flex items-center justify-center text-base mb-1">🏠</div>
                        <span className="text-[10px]">Personal</span>
                    </div>
                    <div className="flex flex-col items-center">
                        <div className="w-8 h-8 rounded-lg bg-accent/30 flex items-center justify-center text-base mb-1">📚</div>
                        <span className="text-[10px]">Study</span>
                    </div>
                    <div className="flex flex-col items-center">
                        <div className="w-8 h-8 rounded-lg bg-error/30 flex items-center justify-center text-base mb-1">🎮</div>
                        <span className="text-[10px]">Gaming</span>
                    </div>
                    <div className="flex flex-col items-center">
                        <div className="w-8 h-8 rounded-lg bg-success/30 flex items-center justify-center text-base mb-1">🎬</div>
                        <span className="text-[10px]">Videos</span>
                    </div>
                    <div className="flex flex-col items-center">
                        <div className="w-8 h-8 rounded-lg border-2 border-dashed border-base-content/20 flex items-center justify-center text-sm mb-1">+</div>
                        <span className="text-[10px]">Add</span>
                    </div>
                </div>
            </div>
        </div>
    );

    const pwaImage = (
        <div className="w-full h-full flex items-center justify-center">
            <div className="w-full max-w-[200px] aspect-[4/3] bg-base-300 rounded-lg p-2 overflow-hidden shadow-lg">
                <div className="flex flex-col h-full">
                    <div className="bg-base-200 rounded-md p-1 flex items-center mb-1">
                        <div className="w-3 h-3 rounded-full bg-error mr-1"></div>
                        <div className="w-3 h-3 rounded-full bg-warning mr-1"></div>
                        <div className="w-3 h-3 rounded-full bg-success"></div>
                        <div className="flex-1 text-center text-[10px] font-semibold">Floorp Web App</div>
                    </div>
                    <div className="flex-1 flex items-center justify-center">
                        <div className="text-center">
                            <div className="w-12 h-12 rounded-xl bg-primary/20 flex items-center justify-center text-2xl mx-auto mb-1">🚀</div>
                            <div className="text-xs font-bold">Floorp App</div>
                            <div className="text-[10px] opacity-70">Added to desktop</div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );

    const mouseGestureImage = (
        <div className="w-full h-full flex items-center justify-center">
            <div className="w-full max-w-[200px] aspect-[4/3] bg-base-300 rounded-lg p-2 overflow-hidden shadow-lg">
                <div className="relative h-full w-full">
                    <div className="absolute top-1/2 left-1/4 transform -translate-x-1/2 -translate-y-1/2">
                        <svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="currentColor" className="text-base-content">
                            <path d="M13.64,21.97C13.14,22.21 12.54,22 12.31,21.5L10.13,16.76L7.62,18.78C7.45,18.92 7.24,19 7,19A1,1 0 0,1 6,18V3A1,1 0 0,1 7,2C7.24,2 7.47,2.09 7.64,2.23L7.65,2.22L19.14,11.86C19.57,12.22 19.62,12.85 19.27,13.27C19.12,13.45 18.91,13.57 18.7,13.61L15.54,14.23L17.74,18.96C18,19.46 17.76,20.05 17.26,20.28L13.64,21.97Z" />
                        </svg>
                    </div>

                    <svg viewBox="0 0 100 100" className="absolute inset-0 w-full h-full">
                        <path
                            d="M25,50 L75,50 L75,75"
                            fill="none"
                            stroke="currentColor"
                            strokeWidth="2"
                            strokeDasharray="4 2"
                            className="text-primary"
                        />
                    </svg>

                    <div className="absolute bottom-2 right-2 bg-base-200 rounded-lg px-1 py-0.5 text-[10px] font-bold">
                        {t('featuresPage.nextTab')}
                    </div>
                </div>
            </div>
        </div>
    );

    const getTranslatedFeatures = (path: string): string[] => {
        const features = t(path, { returnObjects: true });
        return Array.isArray(features) ? features : [];
    };

    const panelSidebarFeatures = getTranslatedFeatures('featuresPage.panelSidebar.features');
    const workspacesFeatures = getTranslatedFeatures('featuresPage.workspaces.features');
    const pwaFeatures = getTranslatedFeatures('featuresPage.pwa.features');
    const mouseGestureFeatures = getTranslatedFeatures('featuresPage.mouseGesture.features');

    return (
        <div className="flex flex-col gap-8">
            <div className="text-center">
                <h1 className="text-4xl font-bold mb-4">{t('featuresPage.title')}</h1>
                <p className="text-lg">{t('featuresPage.subtitle')}</p>
            </div>
            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                <div className="card bg-base-200 shadow-xl h-full">
                    <div className="card-body">
                        <div className="flex items-center mb-4">
                            <div className="text-primary mr-3">
                                <svg xmlns="http://www.w3.org/2000/svg" width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><rect width="18" height="18" x="3" y="3" rx="2" /><path d="M9 3v18" /></svg>
                            </div>
                            <h2 className="text-2xl font-bold">{t('featuresPage.panelSidebar.title')}</h2>
                        </div>
                        <div className="flex flex-col md:flex-row gap-4 items-center">
                            <div className="md:w-1/2">
                                <p className="mb-3">
                                    {t('featuresPage.panelSidebar.description')}
                                </p>
                                <ul className="list-disc list-inside space-y-1">
                                    {panelSidebarFeatures.map((feature, index) => (
                                        <li key={index}>{feature}</li>
                                    ))}
                                </ul>
                            </div>
                            <div className="md:w-1/2">
                                {panelSidebarImage}
                            </div>
                        </div>
                    </div>
                </div>

                <div className="card bg-base-200 shadow-xl h-full">
                    <div className="card-body">
                        <div className="flex items-center mb-4">
                            <div className="text-primary mr-3">
                                <svg xmlns="http://www.w3.org/2000/svg" width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><rect width="7" height="7" x="3" y="3" rx="1" /><rect width="7" height="7" x="14" y="3" rx="1" /><rect width="7" height="7" x="14" y="14" rx="1" /><rect width="7" height="7" x="3" y="14" rx="1" /></svg>
                            </div>
                            <h2 className="text-2xl font-bold">{t('featuresPage.workspaces.title')}</h2>
                        </div>
                        <div className="flex flex-col md:flex-row gap-4 items-center">
                            <div className="md:w-1/2">
                                <p className="mb-3">
                                    {t('featuresPage.workspaces.description')}
                                </p>
                                <ul className="list-disc list-inside space-y-1">
                                    {workspacesFeatures.map((feature, index) => (
                                        <li key={index}>{feature}</li>
                                    ))}
                                </ul>
                            </div>
                            <div className="md:w-1/2">
                                {workspaceImage}
                            </div>
                        </div>
                    </div>
                </div>

                <div className="card bg-base-200 shadow-xl h-full">
                    <div className="card-body">
                        <div className="flex items-center mb-4">
                            <div className="text-primary mr-3">
                                <svg xmlns="http://www.w3.org/2000/svg" width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M12 2H2v10l9.29 9.29c.94.94 2.48.94 3.42 0l6.58-6.58c.94-.94.94-2.48 0-3.42L12 2Z" /><path d="M7 7h.01" /></svg>
                            </div>
                            <h2 className="text-2xl font-bold">{t('featuresPage.pwa.title')}</h2>
                        </div>
                        <div className="flex flex-col md:flex-row gap-4 items-center">
                            <div className="md:w-1/2">
                                <p className="mb-3">
                                    {t('featuresPage.pwa.description')}
                                </p>
                                <ul className="list-disc list-inside space-y-1">
                                    {pwaFeatures.map((feature, index) => (
                                        <li key={index}>{feature}</li>
                                    ))}
                                </ul>
                            </div>
                            <div className="md:w-1/2">
                                {pwaImage}
                            </div>
                        </div>
                    </div>
                </div>

                <div className="card bg-base-200 shadow-xl h-full">
                    <div className="card-body">
                        <div className="flex items-center mb-4">
                            <div className="text-primary mr-3">
                                <svg xmlns="http://www.w3.org/2000/svg" width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="m18 8 4 4-4 4" /><path d="m2 12 10 0" /><path d="m12 2 0 20" /></svg>
                            </div>
                            <h2 className="text-2xl font-bold">{t('featuresPage.mouseGesture.title')}</h2>
                        </div>
                        <div className="flex flex-col md:flex-row gap-4 items-center">
                            <div className="md:w-1/2">
                                <p className="mb-3">
                                    {t('featuresPage.mouseGesture.description')}
                                </p>
                                <ul className="list-disc list-inside space-y-1">
                                    {mouseGestureFeatures.map((feature, index) => (
                                        <li key={index}>{feature}</li>
                                    ))}
                                </ul>
                            </div>
                            <div className="md:w-1/2">
                                {mouseGestureImage}
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            <Navigation />
        </div>
    );
} 