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
  Flex,
  Switch,
} from "@chakra-ui/react";
import Card from "../../components/Card";
import React from "react";
import { Controller, useFormContext } from "react-hook-form";
import type { DesignFormData } from "@/type";
import { useTranslation } from "react-i18next";

export default function Tabbar() {
  const { t } = useTranslation();
  const { control } = useFormContext<DesignFormData>();
  return (
    <Card
      icon={<IconIcRoundTab style={{ fontSize: "24px", color: "#137333" }} />}
      title={t("design.tabBar")}
      footerLink="https://support.google.com/chrome/?p=settings_workspaces"
      footerLinkText={t("design.customizeTabBar")}
    >
      <VStack align="stretch" spacing={4} paddingInlineStart={"10px"}>
        <Text fontSize="lg">{t("design.style")}</Text>
        <Text fontSize="sm" mt={-1}>
          {t("design.styleDescription")}
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
              <Radio value="horizontal">{t("design.horizontal")}</Radio>
              <Radio value="multirow">{t("design.multirow")}</Radio>
            </RadioGroup>
          )}
        />
        <Alert status="info" rounded={"md"}>
          <AlertIcon />
          <AlertDescription>
            {t("design.verticalTabIsRemovedFromNoraneko")}
            <br />
            <Link color="blue.500" href="https://support.mozilla.org">
              {t("design.learnMore")}
            </Link>
          </AlertDescription>
        </Alert>
        <Divider />
        <Text fontSize="lg">{t("design.position")}</Text>
        <Text fontSize="sm" mt={-1}>
          {t("design.positionDescription")}
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
              <Radio value="default">{t("design.default")}</Radio>
              <Radio value="hide-horizontal-tabbar">
                {t("design.hideTabBar")}
              </Radio>
              <Radio value="optimise-to-vertical-tabbar">
                {t("design.hideTabBarAndTitleBar")}
              </Radio>
              <Radio value="bottom-of-navigation-toolbar">
                {t("design.showTabBarOnBottomOfNavigationBar")}
              </Radio>
              <Radio value="bottom-of-window">
                {t("design.showTabBarOnBottomOfWindow")}
              </Radio>
            </RadioGroup>
          )}
        />
      </VStack>
    </Card>
  );
}
