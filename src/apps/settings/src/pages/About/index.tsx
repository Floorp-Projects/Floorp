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
  Image,
  Spacer,
} from "@chakra-ui/react";
import { useTranslation } from "react-i18next";
import Card from "@/components/Card";
import { useConstantsData } from "./dataManager";
import type { ConstantsData } from "@/type";

export default function About() {
  const { t } = useTranslation();
  const textColor = useColorModeValue("gray.800", "gray.100");

  const [constantsData, setConstantsData] =
    React.useState<ConstantsData | null>(null);

  React.useEffect(() => {
    async function fetchConstantsData() {
      const data = await useConstantsData();
      setConstantsData(data);
    }
    fetchConstantsData();
  }, []);

  return (
    <Flex direction="column" alignItems="center" maxW="700px" mx="auto" py={8}>
      <Text fontSize="3xl" mb={10} color={textColor}>
        {t("about.aboutBrowser")}
      </Text>
      <Card
        title={t("about.aboutBrowser")}
        icon={
          <Image
            src={"chrome://branding/content/about-logo@2x.png"}
            boxSize="48px"
            alt="logo"
          />
        }
        footerLink="https://noraneko.example.com/about"
        footerLinkText={t("about.releaseNotes")}
      >
        <Text fontSize="md" mb={3} color={textColor} maxW={"700px"}>
          {t("about.browserVersion", {
            browserVersion: constantsData?.MOZ_APP_VERSION ?? "unknown",
            firefoxVersion: constantsData?.MOZ_APP_VERSION_DISPLAY ?? "unknown",
            isOfficialBuild: constantsData?.MOZ_OFFICIAL_BRANDING
              ? t("about.isOfficialBuild")
              : "",
          })}
        </Text>

        <Text color={textColor} mb={4}>
          {t("about.browserDescription")}
        </Text>
      </Card>

      <Spacer my={4} />

      <Card>
        <Text mt={-2} fontSize={"sm"} maxW={"600px"}>
          {t("about.browser")}
        </Text>
        <Text color={textColor} fontSize="sm" mb={2}>
          {t("about.copyright")}
        </Text>
        <Text fontSize="sm" mb={4} color={textColor}>
          {t("about.openSourceLicense")}
        </Text>
        <Link
          href="https://docs.floorp.app/ja/docs/other/contributors"
          target="_blank"
          fontSize="sm"
          display="flex"
          alignItems="center"
          color={useColorModeValue("blue.500", "blue.400")}
        >
          {t("about.openSourceLicenseNotice")}
          <IconIcOutlineOpenInNew
            style={{ fontSize: "16px", marginLeft: "4px" }}
          />
        </Link>
      </Card>
    </Flex>
  );
}
