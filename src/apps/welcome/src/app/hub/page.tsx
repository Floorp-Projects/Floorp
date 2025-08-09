import { useTranslation } from "react-i18next";
import Navigation from "../../components/Navigation.tsx";
import { Settings } from "lucide-react";
import gearImage from "./Floorp_Icon_Gear_Gradient.png";

export default function HubIntroPage() {
  const { t } = useTranslation();

  return (
    <div className="flex flex-col gap-8">
      <div className="text-center">
        <h1 className="text-4xl font-bold mb-4">{t("hubPage.title")}</h1>
        <p className="text-lg">{t("hubPage.subtitle")}</p>
      </div>

      <div className="card bg-base-200">
        <div className="card-body p-0 overflow-hidden">
          <div className="grid grid-cols-1 lg:grid-cols-2 min-h-[400px]">
            {/* Left side - Icon and branding */}
            <div className="p-8 flex items-center justify-center">
              <div className="text-center">
                <div className="relative mb-6">
                  <img
                    src={gearImage}
                    alt="Floorp Hub"
                    className="w-32 h-32 lg:w-40 lg:h-40 mx-auto"
                  />
                </div>
                <h2 className="text-2xl lg:text-3xl font-bold mb-2">
                  {t("hubPage.sectionTitle")}
                </h2>
                <p className="text-base-content/70 text-sm lg:text-base max-w-xs mx-auto">
                  {t("hubPage.subtitle")}
                </p>
              </div>
            </div>

            {/* Right side - Content and actions */}
            <div className="p-8 flex flex-col justify-center">
              <div className="mb-6">
                <div className="flex items-center mb-4 gap-2">
                  <div className="text-primary">
                    <Settings size={24} />
                  </div>
                  <h3 className="text-xl font-semibold">
                    {t("hubPage.sectionTitle")}
                  </h3>
                </div>
                <p className="text-base-content/80 leading-relaxed mb-6">
                  {t("hubPage.description")}
                </p>
              </div>

              <div className="space-y-3">
                <button
                  type="button"
                  className="btn btn-primary btn-lg w-full shadow-lg hover:shadow-xl transform hover:-translate-y-0.5 transition-all duration-200 text-sm"
                  onClick={() => {
                    globalThis.open("about:hub", "_blank");
                  }}
                >
                  <Settings size={20} className="mr-2" />
                  {t("hubPage.openHub")}
                </button>
                <button
                  type="button"
                  className="btn btn-outline btn-lg w-full hover:shadow-lg transform hover:-translate-y-0.5 transition-all duration-200 text-sm"
                  onClick={() => {
                    globalThis.open("about:preferences", "_blank");
                  }}
                >
                  {t("hubPage.openPreferences")}
                </button>
              </div>

              <div className="mt-6 p-4 bg-base-300/50 rounded-lg border-l-4 border-primary">
                <p className="text-sm text-base-content/70">
                  <span className="font-bold">{t("hubPage.tip")}</span>
                </p>
              </div>
            </div>
          </div>
        </div>
      </div>

      <Navigation />
    </div>
  );
}
