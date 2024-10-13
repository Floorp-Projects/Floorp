/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  Flex,
  VStack,
  Text,
  Avatar,
  Grid,
  GridItem,
  Progress,
} from "@chakra-ui/react";
import Card from "../../components/Card";
import { useTranslation } from "react-i18next";

export default function Home() {
  const { t } = useTranslation();
  return (
    <GridItem display="flex" justifyContent="center">
      <VStack align="stretch" alignItems="center" maxW={"900px"} spacing={6}>
        <Flex alignItems="center" flexDirection={"column"}>
          <Avatar
            size="xl"
            src="chrome://browser/skin/fxa/avatar-color.svg"
            m={4}
          />
          <Text fontSize="3xl">
            {t("home.welcome", { name: "名無しのユーザー" })}
          </Text>
        </Flex>
        <Text>{t("home.description")}</Text>

        <Grid
          templateColumns={{ base: "repeat(1, 1fr)", lg: "repeat(2, 1fr)" }}
          gap={4}
        >
          <Card
            title={t("home.setup.title")}
            footerLink="https://support.ablaze.one/setup"
            footerLinkText={t("home.setup.footerLinkText")}
            icon={
              <IconMdiRocketLaunch
                style={{ fontSize: "24px", color: "#3182F6" }}
              />
            }
          >
            <Text fontSize="sm" mb={2}>
              {t("home.setup.setupDescription")}
            </Text>
            <Text fontWeight="bold" fontSize="sm" mb={1}>
              {t("home.setup.setupProgressTitle")}
            </Text>
            <Progress
              value={50}
              size="sm"
              colorScheme="blue"
              mb={1}
              h={"4px"}
            />
            <Text fontSize="sm">
              {t("home.setup.step", { step: 2, total: 4 })}
            </Text>
          </Card>
          <Card
            title={t("home.privacyAndTrackingProtection.title")}
            footerLink="https://support.google.com/chrome/answer/95647?hl=ja"
            footerLinkText={t(
              "home.privacyAndTrackingProtection.footerLinkText",
            )}
            icon={
              <IconIcSharpPrivacyTip
                style={{ fontSize: "24px", color: "#137333" }}
              />
            }
          >
            <Text fontSize="sm">
              {t("home.privacyAndTrackingProtection.description")}
            </Text>
          </Card>
          <Card
            title={t("home.ablazeAccount.title")}
            footerLink="https://accounts.ablaze.one/signin"
            footerLinkText={t("home.ablazeAccount.footerLinkText")}
            icon={
              <IconMdiAccount style={{ fontSize: "24px", color: "#ff7708" }} />
            }
          >
            <Text fontSize="sm">{t("home.ablazeAccount.description")}</Text>
          </Card>
          <Card
            title={t("home.manageExtensions.title")}
            footerLink="about:addons"
            footerLinkText={t("home.manageExtensions.footerLinkText")}
            icon={
              <IconCodiconExtensions
                style={{ fontSize: "24px", color: "#8400ff" }}
              />
            }
          >
            <Text fontSize="sm">{t("home.manageExtensions.description")}</Text>
          </Card>
          <Card
            title={t("home.noranekoSupport.title")}
            footerLink="https://support.mozilla.org/products/firefox/"
            footerLinkText={t("home.noranekoSupport.footerLinkText")}
            icon={
              <IconMdiHelpCircle
                style={{ fontSize: "24px", color: "#3182F6" }}
              />
            }
          >
            <Text fontSize="sm">{t("home.noranekoSupport.description")}</Text>
          </Card>
        </Grid>
      </VStack>
    </GridItem>
  );
}
