import Home from "./pages/Home";
import Design from "./pages/Design";
import Workspaces from "./pages/Workspaces";
import About from "./pages/About";
import { useTranslation } from "react-i18next";

export const usePageData = () => {
  const { t } = useTranslation();
  return {
    home: {
      path: "/",
      icon: (
        <IconCarbonHome style={{ fontSize: "16px", color: "currentColor" }} />
      ),
      text: t("pages.home"),
      component: <Home />,
    },
    design: {
      path: "/design",
      icon: (
        <IconMdiDesign style={{ fontSize: "16px", color: "currentColor" }} />
      ),
      text: t("pages.lookAndFeel"),
      component: <Design />,
    },
    sidebar: {
      path: "/sidebar",
      icon: (
        <IconLucideSidebar
          style={{ fontSize: "16px", color: "currentColor" }}
        />
      ),
      text: t("pages.noranekoSidebar"),
      component: null,
    },
    workspaces: {
      path: "/workspaces",
      icon: (
        <IconMaterialSymbolsLightSelectWindow
          style={{ fontSize: "16px", color: "currentColor" }}
        />
      ),
      text: t("pages.workspaces"),
      component: <Workspaces />,
    },
    shortcuts: {
      path: "/shortcuts",
      icon: (
        <IconIcOutlineKeyboard
          style={{ fontSize: "16px", color: "currentColor" }}
        />
      ),
      text: t("pages.keyboardShortcuts"),
      component: null,
    },
    webapps: {
      path: "/webapps",
      icon: (
        <IconMdiAppBadgeOutline
          style={{ fontSize: "16px", color: "currentColor" }}
        />
      ),
      text: t("pages.webApps"),
      component: null,
    },
    accounts: {
      path: "/accounts",
      icon: (
        <IconMdiAccount style={{ fontSize: "16px", color: "currentColor" }} />
      ),
      text: t("pages.profileAndAccount"),
      component: null,
    },
    about: {
      path: "/about",
      icon: (
        <IconMdiAboutCircleOutline
          style={{ fontSize: "16px", color: "currentColor" }}
        />
      ),
      text: t("pages.aboutNoraneko"),
      component: <About />,
    },
  };
};
