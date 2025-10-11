import type React from "react";
import { useTranslation } from "react-i18next";
import type { Slide } from "../app/data/slides";
import installer_png from "../assets/installer.png";

interface SlideshowProps {
    slides: Slide[];
    currentSlide: number;
    onPrevSlide: () => void;
    onNextSlide: () => void;
    onGoToSlide: (index: number) => void;
}

const Slideshow: React.FC<SlideshowProps> = ({
    slides,
    currentSlide,
    onPrevSlide,
    onNextSlide,
    onGoToSlide,
}) => {
    const { t } = useTranslation();

    return (
        <div className="flex flex-1 w-full card bg-base-200 shadow-md relative">
            <div className="fixed top-4 left-4 flex items-center space-x-2 gap-2 z-20">
                <div className="avatar">
                    <div className="w-8 ring-primary ring-offset-base-100 ring-offset-1">
                        <img src={installer_png} alt="Floorp Logo" />
                    </div>
                </div>
                <h3 className="text-lg font-semibold">
                    {t("app.installer.title")}
                </h3>
            </div>

            <div className="card-body p-6 flex flex-col h-full">
                <div className="flex flex-col md:flex-row flex-1 gap-8 items-center justify-center">
                    <div className="flex-1 flex justify-center items-center">
                        <div className="w-54 h-54 p-8 rounded-2xl text-primary flex justify-center items-center bg-primary-content">
                            {slides[currentSlide].icon}
                        </div>
                    </div>

                    <div className="flex-1 flex flex-col h-full">
                        <div className="flex-1 overflow-y-auto">
                            <h2 className="text-4xl font-bold mb-6">
                                {slides[currentSlide].title}
                            </h2>
                            <p className="text-lg">
                                {slides[currentSlide].description}
                            </p>
                        </div>
                    </div>
                </div>
            </div>

            <div className="absolute bottom-6 left-0 right-0 flex justify-between items-center px-6">
                <button
                    className="btn btn-circle"
                    onClick={onPrevSlide}
                    aria-label={t("slides.navigation.previous", "前へ")}
                >
                    <svg
                        width="24"
                        height="24"
                        viewBox="0 0 24 24"
                        fill="none"
                        stroke="currentColor"
                        strokeWidth="2"
                        strokeLinecap="round"
                        strokeLinejoin="round"
                    >
                        <path d="M15 18l-6-6 6-6" />
                    </svg>
                </button>

                <div className="flex gap-2 justify-center">
                    {slides.map((_, index) => (
                        <button
                            key={index}
                            className={`btn btn-sm ${
                                index === currentSlide
                                    ? "btn-primary"
                                    : "btn-ghost"
                            }`}
                            onClick={() => onGoToSlide(index)}
                            aria-label={t(
                                "slides.navigation.goto",
                                "スライド{{number}}へ",
                                {
                                    number: index + 1,
                                },
                            )}
                        >
                            {index + 1}
                        </button>
                    ))}
                </div>

                <button
                    className="btn btn-circle"
                    onClick={onNextSlide}
                    aria-label={t("slides.navigation.next", "次へ")}
                >
                    <svg
                        width="24"
                        height="24"
                        viewBox="0 0 24 24"
                        fill="none"
                        stroke="currentColor"
                        strokeWidth="2"
                        strokeLinecap="round"
                        strokeLinejoin="round"
                    >
                        <path d="M9 18l6-6-6-6" />
                    </svg>
                </button>
            </div>
        </div>
    );
};

export default Slideshow;
