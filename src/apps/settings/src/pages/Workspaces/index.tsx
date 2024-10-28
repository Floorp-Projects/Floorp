/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Flex, VStack, Text } from "@chakra-ui/react";
import React, { useEffect } from "react";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import { useTranslation } from "react-i18next";
import Preferences from "./preferences";
import Backup from "./backup";
import type { WorkspacesFormData } from "@/type";
import { getWorkspaceSettings, saveWorkspaceSettings } from "./dataManager";

export default function Workspaces() {
  const { t } = useTranslation();
  const methods = useForm<WorkspacesFormData>({
    defaultValues: {},
  });

  const { setValue } = methods;
  const watchAll = useWatch({
    control: methods.control,
  });

  useEffect(() => {
    const fetchDefaultValues = async () => {
      const values = await getWorkspaceSettings();
      for (const key in values) {
        setValue(
          key as keyof WorkspacesFormData,
          values[key as keyof WorkspacesFormData],
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
    saveWorkspaceSettings(watchAll as WorkspacesFormData);
  }, [watchAll]);

  return (
    <Flex direction="column" alignItems="center" maxW="700px" mx="auto" py={8}>
      <Text fontSize="3xl" mb={10}>
        {t("workspaces.workspaces")}
      </Text>
      <Text mb={8}>{t("workspaces.workspacesDescription")}</Text>
      <VStack align="stretch" spacing={6} w="100%">
        <FormProvider {...methods}>
          <Preferences />
          <Backup />
        </FormProvider>
      </VStack>
    </Flex>
  );
}
