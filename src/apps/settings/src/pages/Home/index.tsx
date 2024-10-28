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
  Box,
} from "@chakra-ui/react";
import { useColorModeValue } from "@chakra-ui/react";
import Card from "@/components/Card";
import { useHomeData } from "./dataManager";
import { useTranslation } from "react-i18next";
import React from "react";
import type { HomeData } from "@/type";

export default function Home() {
  const { t } = useTranslation();
  const color = useColorModeValue("white", "#1a1a1a");
  const [homeData, setHomeData] = React.useState<HomeData | null>(null);

  React.useEffect(() => {
    async function fetchHomeData() {
      const data = await useHomeData();
      setHomeData(data);
    }
    fetchHomeData();
  }, []);

  function isLoggedInToMozillaAccount() {
    return homeData?.accountName !== null;
  }

  return (
    <GridItem display="flex" justifyContent="center">
      <VStack align="stretch" alignItems="center" maxW={"900px"} spacing={6}>
        <Flex alignItems="center" flexDirection={"column"}>
          <Box
            m={5}
            p={"3px"}
            background={
              isLoggedInToMozillaAccount()
                ? "linear-gradient(150deg, #0060df , #c60084 ,#ffa436)"
                : "none"
            }
            borderRadius={"full"}
          >
            <Box background={color} p={1} borderRadius={"full"}>
              <Avatar
                size="xl"
                src={homeData?.accountImage}
                color="white"
                bg="blue.600"
              />
            </Box>
          </Box>
          <Text fontSize="3xl">
            {t("home.welcome", {
              name: homeData?.accountName ?? t("home.defaultAccountName"),
            })}
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
            footerLink="https://support.mozilla.org/kb/enhanced-tracking-protection-firefox-desktop"
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
            title={t("home.browserSupport.title")}
            footerLink="https://docs.floorp.app/docs/features/"
            footerLinkText={t("home.browserSupport.footerLinkText")}
            icon={
              <IconMdiHelpCircle
                style={{ fontSize: "24px", color: "#3182F6" }}
              />
            }
          >
            <Text fontSize="sm">{t("home.browserSupport.description")}</Text>
          </Card>
        </Grid>
      </VStack>
    </GridItem>
  );
}
