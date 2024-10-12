import Home from "./pages/Home";
import Design from "./pages/Design";
import Workspaces from "./pages/Workspaces";
import About from "./pages/About";

export const pages: {
  [key: string]: {
    path: string;
    icon: React.ReactNode;
    text: string;
    component: React.ReactElement | null;
  };
} = {
  home: {
    path: "/",
    icon: (
      <IconCarbonHome style={{ fontSize: "16px", color: "currentColor" }} />
    ),
    text: "Home",
    component: <Home />,
  },
  design: {
    path: "/design",
    icon: <IconMdiDesign style={{ fontSize: "16px", color: "currentColor" }} />,
    text: "Look & Feel",
    component: <Design />,
  },
  sidebar: {
    path: "/sidebar",
    icon: (
      <IconLucideSidebar style={{ fontSize: "16px", color: "currentColor" }} />
    ),
    text: "Noraneko Sidebar",
    component: null,
  },
  workspaces: {
    path: "/workspaces",
    icon: (
      <IconMaterialSymbolsLightSelectWindow
        style={{ fontSize: "16px", color: "currentColor" }}
      />
    ),
    text: "Workspaces",
    component: <Workspaces />,
  },
  shortcuts: {
    path: "/shortcuts",
    icon: (
      <IconIcOutlineKeyboard
        style={{ fontSize: "16px", color: "currentColor" }}
      />
    ),
    text: "Keyboard Shortcuts",
    component: null,
  },
  webapps: {
    path: "/webapps",
    icon: (
      <IconMdiAppBadgeOutline
        style={{ fontSize: "16px", color: "currentColor" }}
      />
    ),
    text: "Web Apps",
    component: null,
  },
  accounts: {
    path: "/accounts",
    icon: (
      <IconMdiAccount style={{ fontSize: "16px", color: "currentColor" }} />
    ),
    text: "Profile and Account",
    component: null,
  },
  about: {
    path: "/about",
    icon: (
      <IconMdiAboutCircleOutline
        style={{ fontSize: "16px", color: "currentColor" }}
      />
    ),
    text: "About Noraneko",
    component: <About />,
  },
};
