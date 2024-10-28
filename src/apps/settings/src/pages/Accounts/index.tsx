/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { Flex, Text, useColorModeValue, VStack } from "@chakra-ui/react";
import { useTranslation } from "react-i18next";
import Profile from "./profile";
import Accounts from "./accounts";
import { FormProvider, useForm, useWatch } from "react-hook-form";

export default function About() {
  const { t } = useTranslation();
  const textColor = useColorModeValue("gray.800", "gray.100");

  const methods = useForm<any>({
    defaultValues: {},
  });

  const { setValue } = methods;
  const watchAll = useWatch({
    control: methods.control,
  });

  return (
    <Flex direction="column" alignItems="center" maxW="700px" mx="auto" py={8}>
      <Text fontSize="3xl" mb={10} color={textColor}>
        {t("accounts.profileAndAccount")}
      </Text>
      <Text mb={8}>{t("accounts.profileDescription")}</Text>
      <VStack align="stretch" spacing={6} w="100%">
        <FormProvider {...methods}>
          <Accounts />
          <Profile />
        </FormProvider>
      </VStack>
    </Flex>
  );
}
