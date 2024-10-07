/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  VStack,
  Text,
  RadioGroup,
  Radio,
  Alert,
  AlertIcon,
  AlertDescription,
  Link,
  Divider,
} from "@chakra-ui/react";
import Card from "../../components/Card";
import React from "react";
import { Controller, useFormContext } from "react-hook-form";
import type { DesignFormData } from "@/type";

export default function Tabbar() {
  const { control } = useFormContext<DesignFormData>();
  return (
    <Card
      icon={<IconIcRoundTab style={{ fontSize: "24px", color: "#137333" }} />}
      title="Tab Bar"
      footerLink="https://support.google.com/chrome/?p=settings_workspaces"
      footerLinkText="Customize Tab bar"
    >
      <VStack align="stretch" spacing={4} paddingInlineStart={"10px"}>
        <Text fontSize="lg">Style</Text>
        <Text fontSize="sm" mt={-1}>
          Select the style og the tabbar. "Multi-row" stacks tab on multiple
          rows reducing tab size
        </Text>
        <Controller<DesignFormData, "style">
          name="style"
          control={control}
          render={({ field: { onChange, value, ref } }) => (
            <RadioGroup
              display="flex"
              flexDirection="column"
              gap={4}
              onChange={onChange}
              value={value}
              ref={ref}
            >
              <Radio value="horizontal">Horizontal</Radio>
              <Radio value="multirow">Multi-row</Radio>
            </RadioGroup>
          )}
        />
        <Alert status="info" rounded={"md"}>
          <AlertIcon />
          <AlertDescription>
            Vertical Tab is removed from Noraneko. Use Firefox native Vertical
            Tab Bar instead.
            <br />
            <Link color="blue.500" href="https://support.mozilla.org">
              Learn more
            </Link>
          </AlertDescription>
        </Alert>
        <Divider />
        <Text fontSize="lg">Position</Text>
        <Text fontSize="sm" mt={-1}>
          Placing the tab bar outside the bottom of the window moves the buttons
          for operating the window
        </Text>
        <Controller
          name="position"
          control={control}
          render={({ field: { onChange, value, ref } }) => (
            <RadioGroup
              display="flex"
              flexDirection="column"
              gap={4}
              onChange={onChange}
              value={value}
              ref={ref}
            >
              <Radio value="default">Default</Radio>
              <Radio value="hide-horizontal-tabbar">Hide Tab Bar</Radio>
              <Radio value="optimise-to-vertical-tabbar">
                Hide Tab Bar & Title Bar
              </Radio>
              <Radio value="bottom-of-navigation-toolbar">
                Show Tab Bar on Bottom of Navigation Bar
              </Radio>
              <Radio value="bottom-of-window">
                Show Tab Bar on Bottom of Window
              </Radio>
            </RadioGroup>
          )}
        />
      </VStack>
    </Card>
  );
}
