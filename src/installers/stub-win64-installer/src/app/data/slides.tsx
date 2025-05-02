import type React from "react";
import { useMemo } from "react";
import { useTranslation } from "react-i18next";

export interface Slide {
    id: number;
    title: string;
    description: string;
    icon: React.ReactNode;
}

export const useSlides = (): Slide[] => {
    const { t } = useTranslation();

    const slides = useMemo<Slide[]>(
        () => [
            {
                id: 1,
                title: t("slides.slide1.title"),
                description: t("slides.slide1.description"),
                icon: <img src="/src/assets/slide1.svg" />,
            },
            {
                id: 2,
                title: t("slides.slide2.title"),
                description: t("slides.slide2.description"),
                icon: <img src="/src/assets/slide2.svg" />,
            },
            {
                id: 3,
                title: t("slides.slide3.title"),
                description: t("slides.slide3.description"),
                icon: <img src="/src/assets/slide3.svg" />,
            },
            {
                id: 4,
                title: t("slides.slide4.title"),
                description: t("slides.slide4.description"),
                icon: <img src="/src/assets/slide4.svg" />,
            },
            {
                id: 5,
                title: t("slides.slide5.title"),
                description: t("slides.slide5.description"),
                icon: <img src="/src/assets/slide5.svg" />,
            },
        ],
        [t],
    );

    return slides;
};
