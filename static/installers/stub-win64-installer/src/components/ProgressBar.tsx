import React from 'react';
import { useTranslation } from 'react-i18next';

interface ProgressBarProps {
    status: string;
    error: string;
    currentSlide: number;
    totalSlides: number;
}

const ProgressBar: React.FC<ProgressBarProps> = ({ status, error, currentSlide, totalSlides }) => {
    const { t } = useTranslation();

    return (
        <div className="mt-auto pt-6">
            <div className="card bg-base-100 shadow-lg p-4">
                <div className="flex flex-col md:flex-row md:items-center md:justify-between gap-4">
                    <div className="flex items-center gap-3">
                        <div className="loading loading-spinner loading-md text-primary"></div>
                        <div>
                            <h3 className="font-bold">{t('app.progress.installing')}</h3>
                            <p className="text-sm">{status}</p>
                        </div>
                    </div>

                    <div className="flex-1 max-w-3xl">
                        <progress className="progress progress-primary w-full"></progress>
                    </div>

                    <div className="text-right">
                        <span className="text-sm opacity-70">
                            {t('app.progress.slideCounter', {
                                current: currentSlide + 1,
                                total: totalSlides
                            })}
                        </span>
                    </div>
                </div>

                {error && (
                    <div className="alert alert-error mt-4">
                        <span>{error}</span>
                    </div>
                )}
            </div>
        </div>
    );
};

export default ProgressBar; 