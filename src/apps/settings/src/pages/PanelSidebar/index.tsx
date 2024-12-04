/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useEffect } from "react";
import { Flex, Text, useColorModeValue, VStack } from "@chakra-ui/react";
import { useTranslation } from "react-i18next";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import Preferences from "./prerferences";
import type { PanelSidebarFormData } from "@/type";
import {
  getPanelSidebarSettings,
  savePanelSidebarSettings,
} from "./dataManager";

export default function PanelSidebar() {
  const { t } = useTranslation();
  const textColor = useColorModeValue("gray.800", "gray.100");

  const methods = useForm<PanelSidebarFormData>({
    defaultValues: {},
  });
  const { setValue } = methods;
  const watchAll = useWatch({
    control: methods.control,
  });

  useEffect(() => {
    const fetchDefaultValues = async () => {
      const values = await getPanelSidebarSettings();
      for (const key in values) {
        setValue(
          key as keyof PanelSidebarFormData,
          values[key as keyof PanelSidebarFormData],
        );
      }
    };

    fetchDefaultValues();
    document?.documentElement?.addEventListener("onfocus", fetchDefaultValues);
    return () => {
      document?.documentElement?.removeEventListener(
        "onfocus",
        fetchDefaultValues,
      );
    };
  }, [setValue]);

  useEffect(() => {
    savePanelSidebarSettings(watchAll as PanelSidebarFormData);
  }, [watchAll]);

  return (
    <Flex direction="column" alignItems="center" maxW="700px" mx="auto" py={8}>
      <Text fontSize="3xl" mb={10} color={textColor}>
        {t("panelSidebar.title")}
      </Text>
      <Text mb={8}>{t("panelSidebar.description")}</Text>
      <VStack align="stretch" spacing={6} w="100%">
        <FormProvider {...methods}>
          <Preferences />
        </FormProvider>
      </VStack>
    </Flex>
  );
}
