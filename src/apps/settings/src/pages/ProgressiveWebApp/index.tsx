/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useEffect } from "react";
import { Flex, Text, useColorModeValue, VStack } from "@chakra-ui/react";
import { useTranslation } from "react-i18next";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import Preferences from "./prerferences";
import InstalledApps from "./installedApps";
import type { TProgressiveWebAppFormData } from "@/type";
import {
  getProgressiveWebAppSettingsAndInstalledApps,
  saveProgressiveWebAppSettings,
} from "./dataManager";

export default function ProgressiveWebApp() {
  const { t } = useTranslation();
  const textColor = useColorModeValue("gray.800", "gray.100");

  const methods = useForm<TProgressiveWebAppFormData>({
    defaultValues: {},
  });
  const { setValue } = methods;
  const watchAll = useWatch({
    control: methods.control,
  });

  useEffect(() => {
    const fetchDefaultValues = async () => {
      const values = await getProgressiveWebAppSettingsAndInstalledApps();
      for (const key in values) {
        setValue(
          key as keyof TProgressiveWebAppFormData,
          values[key as keyof TProgressiveWebAppFormData],
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
    saveProgressiveWebAppSettings(watchAll as TProgressiveWebAppFormData);
  }, [watchAll]);

  return (
    <Flex direction="column" alignItems="center" maxW="700px" mx="auto" py={8}>
      <Text fontSize="3xl" mb={10} color={textColor}>
        {t("progressiveWebApp.title")}
      </Text>
      <Text mb={8}>{t("progressiveWebApp.description")}</Text>
      <VStack align="stretch" spacing={6} w="100%">
        <FormProvider {...methods}>
          <Preferences />
          <InstalledApps />
        </FormProvider>
      </VStack>
    </Flex>
  );
}
