/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import {
  Text,
  useColorModeValue,
  Link,
  HStack,
  Avatar,
  VStack,
  Divider,
  FormControl,
  FormLabel,
  Switch,
  Flex,
  Spacer,
} from "@chakra-ui/react";
import { useTranslation } from "react-i18next";
import Card from "@/components/Card";
import { Controller } from "react-hook-form";
import type { AccountsFormData } from "@/type";

type AccountsProps = {
  accountAndProfileData: AccountsFormData | null;
};

export default function Accounts(props: AccountsProps) {
  const { t } = useTranslation();
  const textColor = useColorModeValue("gray.800", "gray.100");

  return (
    <Card
      title={t("accounts.mozillaAccountSyncSettings")}
      icon={
        <IconMdiAccountCog style={{ fontSize: "24px", color: "#ff7708" }} />
      }
      footerLink="https://docs.floorp.app/ja/docs/other/accounts"
      footerLinkText={t("accounts.manageAccounts")}
    >
      <HStack spacing={6} align="stretch" mb={4}>
        <Avatar
          size="md"
          src={props.accountAndProfileData?.accountInfo.avatarURL}
        />
        <VStack align="stretch" spacing={0}>
          <Text fontSize="md" color={textColor}>
            {props.accountAndProfileData?.accountInfo.displayName ??
              t("accounts.notLoggedIn")}
          </Text>
          <Text fontSize="md" color={useColorModeValue("gray.500", "gray.400")}>
            {props.accountAndProfileData?.accountInfo.email
              ? t("accounts.syncedTo", {
                  email: props.accountAndProfileData?.accountInfo.email,
                })
              : t("accounts.notSynced")}
          </Text>
        </VStack>
      </HStack>
      <Link
        href="https://docs.floorp.app/ja/docs/other/contributors"
        target="_blank"
        fontSize="sm"
        display="flex"
        alignItems="center"
        color={useColorModeValue("blue.500", "blue.400")}
      >
        {t("accounts.manageMozillaAccount")}
        <IconIcOutlineOpenInNew
          style={{ fontSize: "16px", marginLeft: "4px" }}
        />
      </Link>

      <Divider my={4} />

      <Text fontSize="lg" mb={2}>
        {t("accounts.floorpFeatureSyncSettings")}
      </Text>
      <FormControl>
        <Flex justifyContent="space-between">
          <FormLabel flex={1}>
            {t("accounts.syncFloorpNotesToMozillaAccount")}
          </FormLabel>
          <Controller
            name="asyncNoesViaMozillaAccount"
            render={({ field: { onChange, value } }) => (
              <Switch
                colorScheme={"blue"}
                onChange={(e) => onChange(e.target.checked)}
                isChecked={value}
              />
            )}
          />
        </Flex>
      </FormControl>

      <Spacer my={2} />

      <Link
        fontSize="sm"
        display="flex"
        alignItems="center"
        color={useColorModeValue("blue.500", "blue.400")}
      >
        {t("accounts.manageFirefoxFeatureSync")}
        <IconIcOutlineOpenInNew
          style={{ fontSize: "16px", marginLeft: "4px" }}
        />
      </Link>
    </Card>
  );
}
