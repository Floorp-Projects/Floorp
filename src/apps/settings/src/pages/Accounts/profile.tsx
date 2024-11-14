/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import {
  Flex,
  Text,
  useColorModeValue,
  Link,
  Divider,
  Spacer,
} from "@chakra-ui/react";
import { useTranslation } from "react-i18next";
import Card from "@/components/Card";
import type { AccountsFormData } from "@/type";

type ProfileProps = {
  accountAndProfileData: AccountsFormData | null;
};

export default function Profile({ accountAndProfileData }: ProfileProps) {
  const { t } = useTranslation();
  const textColor = useColorModeValue("gray.800", "gray.100");

  return (
    <Card
      title={t("accounts.profileManagement")}
      icon={
        <IconMingcuteProfileFill
          style={{ fontSize: "24px", color: "#3182F6" }}
        />
      }
      footerLink="https://docs.floorp.app/ja/docs/other/profiles"
      footerLinkText={t("accounts.profileManagement")}
    >
      <Text color={textColor}>
        {t("accounts.profileManagementDescription")}
      </Text>

      <Divider my={4} />

      <Text fontSize="md" mb={3} color={textColor}>
        {t("accounts.currentProfileName", {
          name: accountAndProfileData?.profileName,
        })}
      </Text>

      <Text fontSize="md" mb={1} color={textColor}>
        {t("accounts.profileSaveLocation", {
          path: accountAndProfileData?.profileDir,
        })}
      </Text>

      <Spacer my={2} />

      <Flex gap={4}>
        <Link
          href="https://docs.floorp.app/ja/docs/other/contributors"
          target="_blank"
          fontSize="sm"
          display="flex"
          alignItems="center"
          color={useColorModeValue("blue.500", "blue.400")}
        >
          {t("accounts.openProfileManager")}
          <IconIcOutlineOpenInNew
            style={{ fontSize: "16px", marginLeft: "4px" }}
          />
        </Link>
        <Link
          fontSize="sm"
          display="flex"
          alignItems="center"
          color={useColorModeValue("blue.500", "blue.400")}
        >
          {t("accounts.openProfileSaveLocation")}
          <IconIcOutlineOpenInNew
            style={{ fontSize: "16px", marginLeft: "4px" }}
          />
        </Link>
      </Flex>
    </Card>
  );
}
