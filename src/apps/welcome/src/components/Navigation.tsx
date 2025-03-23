import { useLocation, useNavigate } from "react-router-dom";
import { useTranslation } from "react-i18next";

const navigationItems = [
    { path: "/", labelKey: "navigation.welcome" },
    { path: "/localization", labelKey: "navigation.language" },
    { path: "/features", labelKey: "navigation.features" },
    { path: "/customize", labelKey: "navigation.customize" },
    { path: "/finish", labelKey: "navigation.complete" },
];

export default function Navigation() {
    const { t } = useTranslation();
    const location = useLocation();
    const navigate = useNavigate();
    const currentIndex = navigationItems.findIndex((item) => item.path === location.pathname);

    const goToNext = () => {
        if (currentIndex < navigationItems.length - 1) {
            navigate(navigationItems[currentIndex + 1].path);
        }
    };

    const goToPrevious = () => {
        if (currentIndex > 0) {
            navigate(navigationItems[currentIndex - 1].path);
        }
    };

    return (
        <>
            <button
                type="button"
                className="btn btn-circle btn-outline fixed left-50 top-1/2 transform  z-20"
                onClick={goToPrevious}
                disabled={currentIndex === 0}
                aria-label={t("navigation.previous")}
            >
                <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                    <path d="M15 18l-6-6 6-6" />
                </svg>
            </button>
            <button
                type="button"
                className="btn btn-circle btn-primary fixed right-50 top-1/2 transform  z-20"
                onClick={goToNext}
                disabled={currentIndex === navigationItems.length - 1}
                aria-label={t("navigation.next")}
            >
                {currentIndex === navigationItems.length - 2 ? (
                    <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                        <path d="M5 12h14" />
                        <path d="M12 5l7 7-7 7" />
                    </svg>
                ) : (
                    <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                        <path d="M9 18l6-6-6-6" />
                    </svg>
                )}
            </button>
        </>
    );
}