import { GridItem, VStack, Divider, Icon } from "@chakra-ui/react";
import MenuItem from "../components/MenuItem";
import { useState } from "react";
import { useMediaQuery } from "@chakra-ui/react";

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
    text: "ホーム"
  },
  sidebar: {
    path: "/sidebar",
    icon: <IconLucideSidebar style={{ fontSize: "16px", color: "currentColor" }} />,
    text: "Noraneko サイドバー"
  },
  workspaces: {
    path: "/workspaces",
    icon: <IconMaterialSymbolsLightSelectWindow style={{ fontSize: "16px", color: "currentColor" }} />,
    text: "ワークスペース"
  },
  shortcuts: {
    path: "/shortcuts",
    icon: <IconCarbonHome style={{ fontSize: "16px", color: "currentColor" }} />,
    text: "キーボードショートカット"
  },
  webapps: {
    path: "/webapps",
    icon: <IconMdiAppBadgeOutline style={{ fontSize: "16px", color: "currentColor" }} />,
    text: "ウェブアプリ"
  },
  accounts: {
    path: "/accounts",
    icon: <IconMdiAccount style={{ fontSize: "16px", color: "currentColor" }} />,
    text: "プロファイルとアカウント"
  }
}

function Sidebar() {
  const [selected, setSelected] = useState<string>("home")
  const [isMobile] = useMediaQuery("(max-width: 768px)")

  return (
    <GridItem zIndex={1000} w={isMobile ? "75px" : "300px"} position={"fixed"}>
      <VStack align="stretch" spacing={2}>
        {
          Object.keys(data).map((key) => {
            return (
              <MenuItem
                key={key}
                to={data[key].path}
                icon={data[key].icon}
                text={data[key].text}
                selected={selected === key ? true : undefined}
                onClick={() => {
                  setSelected(key);
                }}
              />
            );
          })
        }
        <Divider />
        <MenuItem
          icon={
            <IconMdiSettings style={{ fontSize: "16px", color: "currentColor" }} />
          }
          text="Firefox の設定"
          onClick={() => {
            window.open("about:preferences#privacy", "_blank");
          }}
        />
      </VStack>
    </GridItem>
  );
}

export default Sidebar;
