import type React from "react";
import { useMemo } from "react";
import { useTranslation } from "react-i18next";

import slide1 from "../../assets/slide1.svg?inline";
import slide2 from "../../assets/slide2.svg?inline";
import slide3 from "../../assets/slide3.svg?inline";
import slide4 from "../../assets/slide4.svg?inline";
import slide5 from "../../assets/slide5.svg?inline";

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
                icon: <img src={slide1} />,
            },
            {
                id: 2,
                title: t("slides.slide2.title"),
                description: t("slides.slide2.description"),
                icon: <img src={slide2} />,
            },
            {
                id: 3,
                title: t("slides.slide3.title"),
                description: t("slides.slide3.description"),
                icon: <img src={slide3} />,
            },
            {
                id: 4,
                title: t("slides.slide4.title"),
                description: t("slides.slide4.description"),
                icon: <img src={slide4} />,
            },
            {
                id: 5,
                title: t("slides.slide5.title"),
                description: t("slides.slide5.description"),
                icon: <img src={slide5} />,
            },
        ],
        [t],
    );

    return slides;
};
