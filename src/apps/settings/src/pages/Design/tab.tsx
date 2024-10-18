/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  VStack,
  Text,
  RadioGroup,
  Radio,
  Divider,
  Flex,
  Switch,
  Input,
  Link,
  Alert,
  AlertIcon,
  AlertDescription,
} from "@chakra-ui/react";
import Card from "../../components/Card";
import React from "react";
import { Controller, useFormContext } from "react-hook-form";
import type { DesignFormData } from "@/type";
import { useTranslation } from "react-i18next";
import { register } from "node:module";

export default function Tab() {
  const { t } = useTranslation();
  const { control, getValues } = useFormContext<DesignFormData>();
  return (
    <Card
      icon={
        <IconFluentMdl2Tab style={{ fontSize: "24px", color: "#ff7708" }} />
      }
      title={t("design.tab.title")}
      footerLink="https://noraneko.example.com/help/customize-tab-bar"
      footerLinkText={t("design.tab.footerLinkText")}
    >
      <VStack align="stretch" spacing={4} paddingInlineStart={"10px"}>
        <Text fontSize="lg">{t("design.tab.scroll")}</Text>
        <Text fontSize="sm" mt={-1}>
          {t("design.tab.scrollDescription")}
        </Text>
        <Controller
          name="tabScroll"
          control={control}
          render={({ field: { onChange, value } }) => (
            <Flex justifyContent="space-between" alignItems="center">
              <Text>{t("design.tab.scrollTab")}</Text>
              <Switch
                colorScheme={"blue"}
                onChange={(e) => onChange(e.target.checked)}
                isChecked={value}
              />
            </Flex>
          )}
        />
        <Controller
          name="tabScrollReverse"
          control={control}
          render={({ field: { onChange, value } }) => (
            <Flex justifyContent="space-between" alignItems="center">
              <Text>{t("design.tab.reverseScroll")}</Text>
              <Switch
                isDisabled={!getValues("tabScroll")}
                colorScheme={"blue"}
                onChange={(e) => onChange(e.target.checked)}
                isChecked={value}
              />
            </Flex>
          )}
        />
        <Controller
          name="tabScrollWrap"
          control={control}
          render={({ field: { onChange, value } }) => (
            <Flex justifyContent="space-between" alignItems="center">
              <Text>{t("design.tab.scrollWrap")}</Text>
              <Switch
                isDisabled={!getValues("tabScroll")}
                colorScheme={"blue"}
                onChange={(e) => onChange(e.target.checked)}
                isChecked={value}
              />
            </Flex>
          )}
        />
        {!getValues("tabScroll") &&
        (getValues("tabScrollReverse") || getValues("tabScrollWrap")) ? (
          <Alert status="info" rounded={"md"} mt={2}>
            <AlertIcon />
            <AlertDescription>
              {t("design.tab.scrollPrefInfo", {
                reverseScrollPrefName: t("design.tab.reverseScroll"),
                scrollWrapPrefName: t("design.tab.scrollWrap"),
                scrollPrefName: t("design.tab.scrollTab"),
              })}
            </AlertDescription>
          </Alert>
        ) : null}
        <Divider />
        <Text fontSize="lg">{t("design.tab.openPosition")}</Text>
        <Text fontSize="sm" mt={-1}>
          {t("design.tab.openPositionDescription")}
        </Text>
        <Controller
          name="tabOpenPosition"
          control={control}
          render={({ field: { onChange, value, ref } }) => (
            <RadioGroup
              display="flex"
              flexDirection="column"
              gap={4}
              onChange={(val) => onChange(Number(val))}
              value={value?.toString()}
              ref={ref}
            >
              <Radio value="-1">{t("design.tab.openDefault")}</Radio>
              <Radio value="0">{t("design.tab.openLast")}</Radio>
              <Radio value="1">{t("design.tab.openNext")}</Radio>
            </RadioGroup>
          )}
        />
        <Divider />
        <Text fontSize="lg">{t("design.tab.other")}</Text>
        <Controller
          name="tabPinTitle"
          control={control}
          render={({ field: { onChange, value } }) => (
            <Flex justifyContent="space-between" alignItems="center">
              <Text>{t("design.tab.pinTitle")}</Text>
              <Switch
                colorScheme={"blue"}
                onChange={(e) => onChange(e.target.checked)}
                isChecked={value}
              />
            </Flex>
          )}
        />
        <Controller
          name="tabDubleClickToClose"
          control={control}
          render={({ field: { onChange, value } }) => (
            <Flex justifyContent="space-between" alignItems="center">
              <Text>{t("design.tab.doubleClickToClose")}</Text>
              <Switch
                colorScheme={"blue"}
                onChange={(e) => onChange(e.target.checked)}
                isChecked={value}
              />
            </Flex>
          )}
        />
        <Controller
          name="tabMinWidth"
          control={control}
          render={({ field: { onChange, value } }) => (
            <Flex justifyContent="space-between" alignItems="center">
              <Text>{t("design.tab.minWidth")}</Text>
              <Input
                width="200px"
                min={60}
                max={300}
                type="number"
                onChange={(e) => onChange(Number(e.target.value))}
                value={value}
              />
            </Flex>
          )}
        />
        <Controller
          name="tabMinHeight"
          control={control}
          render={({ field: { onChange, value } }) => (
            <Flex justifyContent="space-between" alignItems="center">
              <Text>{t("design.tab.minHeight")}</Text>
              <Input
                onChange={(e) => onChange(Number(e.target.value))}
                width="200px"
                height="40px"
                min={20}
                max={100}
                type="number"
                value={value}
              />
            </Flex>
          )}
        />
      </VStack>
    </Card>
  );
}
