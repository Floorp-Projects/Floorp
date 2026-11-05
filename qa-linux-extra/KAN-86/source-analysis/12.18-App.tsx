import { MemoryRouter, Route, Routes, useLocation } from "react-router-dom";
import { lazy, Suspense, useEffect } from "react";
import { useTranslation } from "react-i18next";
import ProgressBar from "./components/ProgressBar.tsx";
import { rpc } from "./lib/rpc/rpc.ts";
import { I18nProvider } from "./components/I18nProvider.tsx";
import {
  ReleaseNotesPrompt,
  SetupReleaseNotesProvider,
} from "./components/ReleaseNotesChoice.tsx";
import { FloorpBrand } from "../../../libs/ui/brand.tsx";
import setup from "./setup.module.css";
import { welcomeSteps } from "./components/steps.ts";

const WelcomePage = lazy(() => import("./app/welcome/page.tsx"));
const LocalizationPage = lazy(() => import("./app/localization/page.tsx"));
const HubIntroPage = lazy(() => import("./app/hub/page.tsx"));
const FeaturesPage = lazy(() => import("./app/features/page.tsx"));
const CustomizePage = lazy(() => import("./app/customize/page.tsx"));
const FinishPage = lazy(() => import("./app/finish/page.tsx"));
const SupportPage = lazy(() => import("./app/support/page.tsx"));
const WhatsNewPage = lazy(() => import("./app/whatsnew/page.tsx"));

function WelcomeShell() {
  const { pathname } = useLocation();
  const { t } = useTranslation();
  const step = welcomeSteps.findIndex((item) => item.path === pathname);
  useEffect(() => {
    document.getElementById("setup-title")?.focus({ preventScroll: true });
    globalThis.scrollTo(0, 0);
  }, [pathname]);
  return (
    <div className={setup.frame}>
      <a className="floorp-skip" href="#welcome-content">
        {t("ui.skipToContent", { defaultValue: "Skip to content" })}
      </a>
      <header className={setup.header}>
        <div className={setup.brand}>
          <FloorpBrand onDark />
        </div>
        <ProgressBar />
      </header>
      <main
        id="welcome-content"
        tabIndex={-1}
        className={setup.layout}
      >
        <section className={setup.explanation} aria-labelledby="setup-title">
          <h1 id="setup-title" tabIndex={-1}>
            {t(`setupV5.pages.${step}.title`)}
          </h1>
          <p>{t(`setupV5.pages.${step}.description`)}</p>
          <p>{t(`setupV5.pages.${step}.hint`)}</p>
        </section>
        <div className={setup.pane}>
          <Suspense
            fallback={
              <p role="status">
                {t("ui.loading", { defaultValue: "Loading…" })}
              </p>
            }
          >
            <Routes>
              <Route path="/" element={<WelcomePage />} />
              <Route path="/localization" element={<LocalizationPage />} />
              <Route path="/features" element={<FeaturesPage />} />
              <Route path="/hub" element={<HubIntroPage />} />
              <Route path="/customize" element={<CustomizePage />} />
              <Route path="/support" element={<SupportPage />} />
              <Route path="/finish" element={<FinishPage />} />
            </Routes>
          </Suspense>
        </div>
      </main>
    </div>
  );
}

export default function App() {
  useEffect(() => {
    void rpc.setBoolPref("floorp.browser.welcome.page.shown", true)
      .catch((error) =>
        console.error("[Welcome] Failed to mark page shown", error)
      );
  }, []);
  const url = new URL(globalThis.location.href);
  const isReleaseNotes = url.searchParams.get("releaseNotes") === "1";
  const isUpgrade = !!url.searchParams.get("upgrade");
  return (
    <I18nProvider>
      {isReleaseNotes
        ? <ReleaseNotesPrompt />
        : isUpgrade
        ? (
          <Suspense fallback={null}>
            <WhatsNewPage />
          </Suspense>
        )
        : (
          <SetupReleaseNotesProvider>
            <MemoryRouter>
              <WelcomeShell />
            </MemoryRouter>
          </SetupReleaseNotesProvider>
        )}
    </I18nProvider>
  );
}
