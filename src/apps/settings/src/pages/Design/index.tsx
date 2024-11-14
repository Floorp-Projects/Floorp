/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Flex, VStack, Text } from "@chakra-ui/react";
import React, { useEffect } from "react";
import Interface from "./interface";
import Tabbar from "./tabbar";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import { getDesignSettings, saveDesignSettings } from "./dataManager";
import type { DesignFormData } from "@/type";
import { useTranslation } from "react-i18next";
import Tab from "./tab";

export default function Design() {
  const { t } = useTranslation();
  const methods = useForm<DesignFormData>({
    defaultValues: {},
  });
  const { setValue } = methods;
  const watchAll = useWatch({
    control: methods.control,
  });

  useEffect(() => {
    const fetchDefaultValues = async () => {
      const values = await getDesignSettings();
      for (const key in values) {
        setValue(
          key as keyof DesignFormData,
          values[key as keyof DesignFormData],
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
    saveDesignSettings(watchAll as DesignFormData);
  }, [watchAll]);

  return (
    <Flex direction="column" alignItems="center" maxW="700px" mx="auto" py={8}>
      <Text fontSize="3xl" mb={10}>
        {t("design.tabAndAppearance")}
      </Text>
      <Text mb={8}>{t("design.customizePositionOfToolbars")}</Text>

      <VStack align="stretch" spacing={6} w="100%">
        <FormProvider {...methods}>
          <Interface />
          <Tabbar />
          <Tab />
        </FormProvider>
      </VStack>
    </Flex>
  );
}
