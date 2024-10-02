import {
  Flex,
  VStack,
  Text,
  Switch,
  Input,
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
    <Flex direction="column" alignItems="center" maxW="700px" mx="auto" py={8}>
      <Text fontSize="3xl" mb={10}>Workspaces</Text>
      <Text mb={8}>
        Workspaces allow you to organize tabs and windows, and manage your work efficiently.
      </Text>

      <VStack align="stretch" spacing={6} w="100%">
        <Card
          icon={<IconCarbonWorkspace style={{ fontSize: '24px', color: '#3182F6' }} />}
          title="Basic Settings"
          footerLink="https://support.google.com/chrome/?p=settings_workspaces"
          footerLinkText="How to use workspaces"
        >
          <VStack align="stretch" spacing={4} mt={4} paddingInlineStart={"10px"}>
            <Flex justifyContent="space-between" alignItems="center">
              <Text>Enable Workspace Function</Text>
              <Switch size="lg" colorScheme={"blue"} />
            </Flex>
            <Flex justifyContent="space-between" alignItems="center">
              <Text>Workspace Default Name</Text>
              <Input width="200px" type="text" />
            </Flex>
            <Flex justifyContent="space-between" alignItems="center">
              <Text>Customize Workspace Container</Text>
              <Menu>
                {({ isOpen }) => (
                  <>
                    <MenuButton isActive={isOpen} as={Button} width="125px" rightIcon={<IconMdiCog />}>
                      {isOpen ? 'Close' : 'Open'}
                    </MenuButton>
                    <MenuList>
                      <MenuItem>ワークスペースを追加</MenuItem>
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
