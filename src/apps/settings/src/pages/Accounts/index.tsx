/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useState, useEffect } from "react";
import { Flex, Text, useColorModeValue, VStack } from "@chakra-ui/react";
import { useTranslation } from "react-i18next";
import Profile from "./profile";
import Accounts from "./accounts";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import { useAccountAndProfileData } from "./dataManager";
import type { AccountsFormData } from "@/type";

export default function ProfileAndAccount() {
  const { t } = useTranslation();
  const textColor = useColorModeValue("gray.800", "gray.100");

  const methods = useForm<AccountsFormData>({
    defaultValues: {},
  });

  const { setValue } = methods;
  const watchAll = useWatch({
    control: methods.control,
  });

  const [accountAndProfileData, setAccountAndProfileData] =
    useState<AccountsFormData | null>(null);

  useEffect(() => {
    async function fetchAccountAndProfileData() {
      const data = await useAccountAndProfileData();
      setAccountAndProfileData(data);
    }
    fetchAccountAndProfileData();
  }, []);

  useEffect(() => {
    if (accountAndProfileData?.asyncNoesViaMozillaAccount) {
      setValue(
        "asyncNoesViaMozillaAccount",
        accountAndProfileData.asyncNoesViaMozillaAccount,
      );
    }
  }, [accountAndProfileData, setValue]);

  return (
    <Flex direction="column" alignItems="center" maxW="700px" mx="auto" py={8}>
      <Text fontSize="3xl" mb={10} color={textColor}>
        {t("accounts.profileAndAccount")}
      </Text>
      <Text mb={8}>{t("accounts.profileDescription")}</Text>
      <VStack align="stretch" spacing={6} w="100%">
        <FormProvider {...methods}>
          <Accounts accountAndProfileData={accountAndProfileData} />
          <Profile accountAndProfileData={accountAndProfileData} />
        </FormProvider>
      </VStack>
    </Flex>
  );
}
