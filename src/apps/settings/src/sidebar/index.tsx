import { GridItem, VStack, Divider, Icon } from "@chakra-ui/react";
import MenuItem from "../components/MenuItem";

function Sidebar() {
  return (
    <GridItem zIndex={1000} w={"250px"} position={"fixed"}>
      <VStack align="stretch" spacing={2}>
        <MenuItem
          icon={<Icon as={"svg"} />}
          text="ホーム"
          to="/"
          selected
        />
        <MenuItem icon={<Icon as={"svg"} />} text="デザイン" to="/design" />
        <MenuItem icon={<Icon as={"svg"} />} text="Noraneko サイドバー" to="/sidebar" />
        <MenuItem
          icon={<Icon as={"svg"} />}
          text="キーボードショートカット"
          to="/shortcuts"
        />
        <MenuItem
          icon={<Icon as={"svg"} />}
          text="ウェブアプリ"
          to="/webapps"
        />
        <MenuItem
          icon={<Icon as={"svg"} />}
          text="プロファイルとアカウント"
          to="/accounts"
        />
        <Divider />
        <MenuItem
          icon={<Icon as={"svg"} />}
          text="Firefox の設定"
          to="/settings"
        />
      </VStack>
    </GridItem>
  );
}

export default Sidebar;
