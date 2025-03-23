import { useLocation, useNavigate } from "react-router-dom";
import { useTranslation } from "react-i18next";

const navigationItems = [
    { path: "/", labelKey: "navigation.welcome" },
    { path: "/localization", labelKey: "navigation.languageSupport" },
    { path: "/features", labelKey: "navigation.featureIntroduction" },
    { path: "/customize", labelKey: "navigation.initialSettings" },
    { path: "/finish", labelKey: "navigation.complete" },
];

export default function ProgressBar() {
    const { t } = useTranslation();
    const location = useLocation();
    const navigate = useNavigate();
    const currentIndex = navigationItems.findIndex((item) => item.path === location.pathname);

    return (
        <div className="w-full mb-12">
            <ul className="steps steps-horizontal w-full">
                {navigationItems.map((item, index) => (
                    <li
                        key={item.path}
                        className={`step ${index <= currentIndex ? "step-primary" : ""}`}
                        onClick={() => navigate(item.path)}
                    >
                        {t(item.labelKey)}
                    </li>
                ))}
            </ul>
        </div>
    );
}