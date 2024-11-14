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
  FormControl,
  FormLabel,
  FormErrorMessage,
} from "@chakra-ui/react";
import Card from "@/components/Card";
import React from "react";
import { Controller, useFormContext } from "react-hook-form";
import type { DesignFormData } from "@/type";
import { useTranslation } from "react-i18next";

export default function Tab() {
  const { t } = useTranslation();
  const { control, getValues } = useFormContext<DesignFormData>();

  return (
    <Card
      icon={
        <IconFluentMdl2Tab style={{ fontSize: "24px", color: "#ff7708" }} />
      }
      title={t("design.tab.title")}
      footerLink="https://docs.floorp.app/docs/features/tab-customization"
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
            <FormControl>
              <Flex justifyContent="space-between">
                <FormLabel flex={1}>{t("design.tab.scrollTab")}</FormLabel>
                <Switch
                  colorScheme={"blue"}
                  onChange={(e) => onChange(e.target.checked)}
                  isChecked={value}
                />
              </Flex>
            </FormControl>
          )}
        />
        <Controller
          name="tabScrollReverse"
          control={control}
          render={({ field: { onChange, value } }) => (
            <FormControl isDisabled={!getValues("tabScroll")}>
              <Flex justifyContent="space-between">
                <FormLabel flex={1}>{t("design.tab.reverseScroll")}</FormLabel>
                <Switch
                  colorScheme={"blue"}
                  onChange={(e) => onChange(e.target.checked)}
                  isChecked={value}
                />
              </Flex>
            </FormControl>
          )}
        />
        <Controller
          name="tabScrollWrap"
          control={control}
          render={({ field: { onChange, value } }) => (
            <FormControl isDisabled={!getValues("tabScroll")}>
              <Flex justifyContent="space-between">
                <FormLabel flex={1}>{t("design.tab.scrollWrap")}</FormLabel>
                <Switch
                  colorScheme={"blue"}
                  onChange={(e) => onChange(e.target.checked)}
                  isChecked={value}
                />
              </Flex>
            </FormControl>
          )}
        />

        {!getValues("tabScroll") &&
        (getValues("tabScrollReverse") || getValues("tabScrollWrap")) ? (
          <Alert status="info" rounded={"md"}>
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
          render={({ field: { onChange, value } }) => (
            <FormControl>
              <RadioGroup
                onChange={(val) => onChange(Number(val))}
                value={value?.toString()}
              >
                <VStack align="stretch" spacing={4}>
                  <Radio value="-1">{t("design.tab.openDefault")}</Radio>
                  <Radio value="0">{t("design.tab.openLast")}</Radio>
                  <Radio value="1">{t("design.tab.openNext")}</Radio>
                </VStack>
              </RadioGroup>
            </FormControl>
          )}
        />

        <Divider />

        <Text fontSize="lg">{t("design.tab.other")}</Text>
        <Controller
          name="tabPinTitle"
          control={control}
          render={({ field: { onChange, value } }) => (
            <FormControl>
              <Flex justifyContent="space-between">
                <FormLabel flex={1}>{t("design.tab.pinTitle")}</FormLabel>
                <Switch
                  colorScheme={"blue"}
                  onChange={(e) => onChange(e.target.checked)}
                  isChecked={value}
                />
              </Flex>
            </FormControl>
          )}
        />
        <Controller
          name="tabDubleClickToClose"
          control={control}
          render={({ field: { onChange, value } }) => (
            <FormControl>
              <Flex justifyContent="space-between">
                <FormLabel flex={1}>
                  {t("design.tab.doubleClickToClose")}
                </FormLabel>
                <Switch
                  colorScheme={"blue"}
                  onChange={(e) => onChange(e.target.checked)}
                  isChecked={value}
                />
              </Flex>
            </FormControl>
          )}
        />
        <Controller
          name="tabMinWidth"
          control={control}
          render={({ field: { onChange, value } }) => (
            <FormControl isInvalid={value < 60 || value > 300}>
              <Flex justifyContent="space-between">
                <FormLabel flex={1}>{t("design.tab.minWidth")}</FormLabel>
                <Input
                  width="200px"
                  type="number"
                  onChange={(e) => onChange(Number(e.target.value))}
                  value={value}
                />
              </Flex>
              <FormErrorMessage w="fit-content" ml="auto">
                {t("design.tab.minWidthError", {
                  min: 60,
                  max: 300,
                })}
              </FormErrorMessage>
            </FormControl>
          )}
        />
        <Controller
          name="tabMinHeight"
          control={control}
          render={({ field: { onChange, value } }) => (
            <FormControl isInvalid={value < 20 || value > 100}>
              <Flex justifyContent="space-between">
                <FormLabel flex={1}>{t("design.tab.minHeight")}</FormLabel>
                <Input
                  onChange={(e) => onChange(Number(e.target.value))}
                  width="200px"
                  height="40px"
                  type="number"
                  value={value}
                />
              </Flex>
              <FormErrorMessage w="fit-content" ml="auto">
                {t("design.tab.minHeightError", {
                  min: 20,
                  max: 100,
                })}
              </FormErrorMessage>
            </FormControl>
          )}
        />
      </VStack>
    </Card>
  );
}
