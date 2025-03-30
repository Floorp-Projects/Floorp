import React, { useMemo } from 'react';
import { useTranslation } from 'react-i18next';

export interface Slide {
    id: number;
    title: string;
    description: string;
    icon: React.ReactNode;
}

// スライドデータをカスタムフックとして提供
export const useSlides = (): Slide[] => {
    const { t } = useTranslation();

    // メモ化してパフォーマンスを最適化
    const slides = useMemo<Slide[]>(() => [
        {
            id: 1,
            title: t('slides.privacy.title'),
            description: t('slides.privacy.description'),
            icon: (
                <svg className="w-16 h-16 text-primary" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
                    <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z" />
                </svg>
            )
        },
        {
            id: 2,
            title: t('slides.performance.title'),
            description: t('slides.performance.description'),
            icon: (
                <svg className="w-16 h-16 text-primary" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
                    <polyline points="13 2 3 14 12 14 11 22 21 10 12 10 13 2" />
                </svg>
            )
        },
        {
            id: 3,
            title: t('slides.customization.title'),
            description: t('slides.customization.description'),
            icon: (
                <svg className="w-16 h-16 text-primary" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
                    <circle cx="19.5" cy="12.5" r="2.5" />
                    <circle cx="12" cy="10" r="2" />
                    <circle cx="5" cy="14" r="2" />
                    <path d="M12 10v6" />
                    <path d="M19.5 12.5v9" />
                    <path d="M5 14l0 6" />
                </svg>
            )
        },
        {
            id: 4,
            title: t('slides.extensions.title'),
            description: t('slides.extensions.description'),
            icon: (
                <svg className="w-16 h-16 text-primary" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
                    <rect x="4" y="4" width="16" height="16" rx="2" />
                    <path d="M4 12h16" />
                    <path d="M12 4v16" />
                </svg>
            )
        },
        {
            id: 5,
            title: t('slides.multiplatform.title'),
            description: t('slides.multiplatform.description'),
            icon: (
                <svg className="w-16 h-16 text-primary" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
                    <rect x="2" y="4" width="20" height="16" rx="2" />
                    <path d="M12 16v.01" />
                    <path d="M8 12h8" />
                </svg>
            )
        }
    ], [t]);

    return slides;
};