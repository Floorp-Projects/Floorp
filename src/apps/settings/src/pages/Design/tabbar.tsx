import {
  VStack,
  Text,
  RadioGroup,
} from "@chakra-ui/react";
import Card from "../../components/Card";
import React from "react";

export default function Tabbar() {
  return (
    <Card
      icon={
        <IconCarbonWorkspace style={{ fontSize: "24px", color: "#3182F6" }} />
      }
      title="Tab Bar"
      footerLink="https://support.google.com/chrome/?p=settings_workspaces"
      footerLinkText="How to use workspaces"
    >
      <VStack align="stretch" spacing={4} paddingInlineStart={"10px"}>
        <Text fontSize="lg">Tab Bar Style</Text>
        <Text fontSize="sm" mt={-1}>
          Select the style og the tabbar. "Multi-row" stacks tab on multiple
          rows reducing tab size
        </Text>
      </VStack>
    </Card>
  );
}
