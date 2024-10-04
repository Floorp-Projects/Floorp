import { GridItem, VStack, Divider, Icon } from "@chakra-ui/react";
import MenuItem from "../components/MenuItem";
import { useMediaQuery } from "@chakra-ui/react";
import { openChromeURL } from "../dev";
import { useLocation } from "react-router-dom";

const data: {
  [key: string]: {
    path: string;
    icon: React.ReactNode;
    text: string;
  }
} = {
  home: {
    path: "/",
    icon: <IconCarbonHome style={{ fontSize: "16px", color: "currentColor" }} />,
    text: "Home"
  },
  design: {
    path: "/design",
    icon: <IconMdiDesign style={{ fontSize: "16px", color: "currentColor" }} />,
    text: "Look & Feel"
  },
  sidebar: {
    path: "/sidebar",
    icon: <IconLucideSidebar style={{ fontSize: "16px", color: "currentColor" }} />,
    text: "Noraneko Sidebar"
  },
  workspaces: {
    path: "/workspaces",
    icon: <IconMaterialSymbolsLightSelectWindow style={{ fontSize: "16px", color: "currentColor" }} />,
    text: "Workspaces"
  },
  shortcuts: {
    path: "/shortcuts",
    icon: <IconIcOutlineKeyboard style={{ fontSize: "16px", color: "currentColor" }} />,
    text: "Keyboard Shortcuts"
  },
  webapps: {
    path: "/webapps",
    icon: <IconMdiAppBadgeOutline style={{ fontSize: "16px", color: "currentColor" }} />,
    text: "Web Apps"
  },
  accounts: {
    path: "/accounts",
    icon: <IconMdiAccount style={{ fontSize: "16px", color: "currentColor" }} />,
    text: "Profile and Account"
  }
}

function Sidebar() {
  const [isMobile] = useMediaQuery("(max-width: 768px)")
  const location = useLocation();
  return (
    <GridItem zIndex={1000} w={isMobile ? "75px" : "300px"} maxH={"calc(100vh - 100px)"} position={"fixed"} overflowY={"scroll"}>
      <VStack align="stretch" spacing={2}>
        {
          Object.keys(data).map((key) => {
            return (
              <MenuItem
                key={key}
                to={data[key].path}
                icon={data[key].icon}
                text={data[key].text}
                selected={location.pathname === data[key].path}
              />
            );
          })
        }
        <Divider />
        <MenuItem
          icon={
            <IconMdiSettings style={{ fontSize: "16px", color: "currentColor" }} />
          }
          text="Firefox Settings"
          onClick={() => {
            openChromeURL("about:preferences#privacy");
          }}
        />
      </VStack>
    </GridItem>
  );
}

export default Sidebar;
