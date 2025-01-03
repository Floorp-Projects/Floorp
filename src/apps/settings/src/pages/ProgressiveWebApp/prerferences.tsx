/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  VStack,
  Text,
  Divider,
  Flex,
  Switch,
  FormLabel,
  FormControl,
  FormHelperText,
  useDisclosure,
} from "@chakra-ui/react";
import Card from "@/components/Card";
import React from "react";
import { Controller, useFormContext } from "react-hook-form";
import { useTranslation } from "react-i18next";
import RestartWarningDialog from "@/components/RestartWarningDialog";

export default function Preferences() {
  const { t } = useTranslation();
  const { control } = useFormContext();

  const {
    isOpen: isOpenEnablePwa,
    onOpen: onOpenEnablePwa,
    onClose: onCloseEnablePwa,
  } = useDisclosure();

  return (
    <>
      <RestartWarningDialog
        description={t("progressiveWebApp.needRestartDescriptionForEnableAndDisable")}
        onClose={onCloseEnablePwa}
        isOpen={isOpenEnablePwa}
      />
      <Card
        icon={
          <IconMdiAppBadgeOutline style={{ fontSize: "24px", color: "#871fff" }} />
        }
        title={t("progressiveWebApp.basicSettings")}
        footerLink="https://docs.floorp.app/docs/features/how-to-use-pwa"
        footerLinkText={t("progressiveWebApp.learnMore")}
      >
        <VStack align="stretch" spacing={4} paddingInlineStart={"10px"}>
          <Text fontSize="lg">{t("progressiveWebApp.enableDisable")}</Text>
          <FormControl>
            <Flex justifyContent="space-between">
              <FormLabel flex={1}>
                {t("progressiveWebApp.enablePwa")}
              </FormLabel>
              <Controller
                name="enabled"
                control={control}
                render={({ field: { onChange, value } }) => (
                  <Switch
                    colorScheme={"blue"}
                    onChange={(e) => {
                      onOpenEnablePwa();
                      onChange(e.target.checked);
                    }}
                    isChecked={value}
                  />
                )}
              />
            </Flex>
            <FormHelperText mt={0}>
              {t("progressiveWebApp.enablePwaDescription")}
            </FormHelperText>
          </FormControl>

          <Divider />

          <Text fontSize="lg">{t("progressiveWebApp.otherSettings")}</Text>
          <FormControl>
            <Flex justifyContent="space-between">
              <FormLabel flex={1}>
                {t("progressiveWebApp.showToolbar")}
              </FormLabel>
              <Controller
                name="showToolbar"
                control={control}
                render={({ field: { onChange, value } }) => (
                  <Switch
                    colorScheme={"blue"}
                    onChange={(e) => onChange(e.target.checked)}
                    isChecked={value}
                  />
                )}
              />
            </Flex>
            <FormHelperText mt={0}>
              {t("progressiveWebApp.showToolbarDescription")}
            </FormHelperText>
          </FormControl>
        </VStack>
      </Card>
    </>
  );
}
