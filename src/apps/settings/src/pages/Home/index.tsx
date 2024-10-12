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
          <Text fontSize="3xl">{t("welcome")}</Text>
        </Flex>
        <Text>
          Manage your account, privacy, and security to make your Noraneko
          experience convenient and safe.
        </Text>

        <Grid
          templateColumns={{ base: "repeat(1, 1fr)", lg: "repeat(2, 1fr)" }}
          gap={4}
        >
          <Card
            title="Setup"
            footerLink="https://support.ablaze.one/setup"
            footerLinkText="Login to Mozilla & Ablaze Account"
            icon={
              <IconMdiRocketLaunch
                style={{ fontSize: "24px", color: "#3182F6" }}
              />
            }
          >
            <Text fontSize="sm" mb={2}>
              Complete the setup to get the most out of Noraneko.
            </Text>
            <Text fontWeight="bold" fontSize="sm" mb={1}>
              Progress
            </Text>
            <Progress
              value={50}
              size="sm"
              colorScheme="blue"
              mb={1}
              h={"4px"}
            />
            <Text fontSize="sm">Step 2/4</Text>
          </Card>
          <Card
            title="Privacy and Tracking Protection"
            footerLink="https://support.google.com/chrome/answer/95647?hl=ja"
            footerLinkText="Privacy and Tracking Protection"
            icon={
              <IconIcSharpPrivacyTip
                style={{ fontSize: "24px", color: "#137333" }}
              />
            }
          >
            <Text fontSize="sm">
              Noraneko will work to properly protect your data and privacy by
              default, but you can also manage it manually.
            </Text>
          </Card>
          <Card
            title="Ablaze Account"
            footerLink="https://accounts.ablaze.one/signin"
            footerLinkText="Ablaze Account"
            icon={
              <IconMdiAccount style={{ fontSize: "24px", color: "#ff7708" }} />
            }
          >
            <Text fontSize="sm">
              Ablaze Account is used to sync some of your data with other
              devices. It is also used to sync data that cannot be synced with a
              Mozilla Account.
            </Text>
          </Card>
          <Card
            title="Manage Extensions"
            footerLink="about:addons"
            footerLinkText="Manage Extensions"
            icon={
              <IconCodiconExtensions
                style={{ fontSize: "24px", color: "#8400ff" }}
              />
            }
          >
            <Text fontSize="sm">
              Noraneko allows you to manage the extensions available on
              addons.mozilla.org.
            </Text>
          </Card>
          <Card
            title="Noraneko Support"
            footerLink="https://support.mozilla.org/products/firefox/"
            footerLinkText="Noraneko Support"
            icon={
              <IconMdiHelpCircle
                style={{ fontSize: "24px", color: "#3182F6" }}
              />
            }
          >
            <Text fontSize="sm">
              You can use the support written by Noraneko contributors.
            </Text>
          </Card>
        </Grid>
      </VStack>
    </GridItem>
  );
}
