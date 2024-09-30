import { GridItem, VStack, Divider, Icon } from "@chakra-ui/react";
import MenuItem from "../components/MenuItem";

function Sidebar() {
  return (
    <GridItem zIndex={1000} w={"300px"} position={"fixed"}>
      <VStack align="stretch" spacing={2}>
        <MenuItem
          icon={
            <IconCarbonHome style={{ fontSize: "16px", color: "#000000" }} />
          }
          text="ホーム"
          to="/"
          selected
        />
        <MenuItem
          icon={
            <IconLucideSidebar style={{ fontSize: "16px", color: "#000000" }} />
          }
          text="Noraneko サイドバー"
          to="/sidebar"
        />
        <MenuItem
          icon={
            <IconCarbonHome style={{ fontSize: "16px", color: "#000000" }} />
          }
          text="キーボードショートカット"
          to="/shortcuts"
        />
        <MenuItem
          icon={
            <IconMdiAppBadgeOutline
              style={{ fontSize: "16px", color: "#000000" }}
            />
          }
          text="ウェブアプリ"
          to="/webapps"
        />
        <MenuItem
          icon={
            <IconMdiAccount style={{ fontSize: "16px", color: "#000000" }} />
          }
          text="プロファイルとアカウント"
          to="/accounts"
        />
        <Divider />
        <MenuItem
          icon={
            <IconMdiSettings style={{ fontSize: "16px", color: "#000000" }} />
          }
          text="Firefox の設定"
          onClick={() => {
            window.location.href = "about:preferences#privacy";
          }}
        />
      </VStack>
    </GridItem>
  );
}

export default Sidebar;
