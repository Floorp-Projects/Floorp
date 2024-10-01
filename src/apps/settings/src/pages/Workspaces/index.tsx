import {
  Flex,
  VStack,
  Text,
  Switch,
  Input,
  Select,
  MenuList,
  MenuItem,
  Menu,
  MenuButton,
  Button,
} from "@chakra-ui/react";
import Card from "../../components/Card";
import React from "react";

export default function Workspaces() {
  return (
    <Flex direction="column" alignItems="flex-start" maxW="800px" mx="auto" py={8}>
      <Text fontSize="3xl" fontWeight="bold" mb={6}>ワークスペース</Text>
      <Text mb={8}>
        ワークスペースを使用すると、タブやウィンドウを整理し、作業を効率的に管理できます。
      </Text>

      <VStack align="stretch" spacing={6} w="100%">
        <Card
          title="ワークスペースの設定"
          icon={<IconMdiCog style={{ color: '#3182F6', fontSize: '24px' }} />}
        >
          <VStack align="stretch" spacing={4} mt={4}>
            <Flex justifyContent="space-between" alignItems="center">
              <Text>ワークスペース機能を有効にする</Text>
              <Switch />
            </Flex>
            <Flex justifyContent="space-between" alignItems="center">
              <Text>ワークスペースのデフォルト名</Text>
              <Input width="100px" type="number" />
            </Flex>
            <Flex justifyContent="space-between" alignItems="center">
              <Text>ワークスペースコンテナーのカスタマイズ</Text>
              <Menu>
                {({ isOpen }) => (
                  <>
                    <MenuButton isActive={isOpen} as={Button} rightIcon={<IconMdiCog />}>
                      {isOpen ? 'Close' : 'Open'}
                    </MenuButton>
                    <MenuList>
                      <MenuItem>Download</MenuItem>
                    </MenuList>
                  </>
                )}
              </Menu>
            </Flex>
          </VStack>
        </Card>
      </VStack>
    </Flex>
  );
}
