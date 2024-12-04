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
  Input,
  Alert,
  AlertDescription,
  AlertIcon,
  Link,
  Radio,
  RadioGroup,
  Stack,
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
    isOpen: isOpenEnablePanelSidebar,
    onOpen: onOpenEnablePanelSidebar,
    onClose: onCloseEnablePanelSidebar,
  } = useDisclosure();

  return (
    <>
      <RestartWarningDialog
        description={t(
          "panelSidebar.needRestartDescriptionForEnableAndDisable",
        )}
        onClose={onCloseEnablePanelSidebar}
        isOpen={isOpenEnablePanelSidebar}
      />
      <Card
        icon={
          <IconLucideSidebar style={{ fontSize: "24px", color: "#6e05d1" }} />
        }
        title={t("panelSidebar.basicSettings")}
        footerLink="https://docs.floorp.app/docs/features/how-to-use-workspaces"
        footerLinkText={t("panelSidebar.howToUseAndCustomize")}
      >
        <VStack align="stretch" spacing={4} paddingInlineStart={"10px"}>
          <Text fontSize="lg">{t("panelSidebar.enableOrDisable")}</Text>
          <FormControl>
            <Flex justifyContent="space-between">
              <FormLabel flex={1}>
                {t("panelSidebar.enablePanelSidebar")}
              </FormLabel>
              <Controller
                name="enabled"
                control={control}
                render={({ field: { onChange, value } }) => (
                  <Switch
                    colorScheme={"blue"}
                    onChange={(e) => {
                      onOpenEnablePanelSidebar();
                      onChange(e.target.checked);
                    }}
                    isChecked={value}
                  />
                )}
              />
            </Flex>
            <FormHelperText mt={0}>
              {t("panelSidebar.enableDescription")}
            </FormHelperText>
          </FormControl>

          <Divider />

          <Text fontSize="lg">{t("panelSidebar.otherSettings")}</Text>
          <FormControl>
            <Flex justifyContent="space-between">
              <FormLabel flex={1}>
                {t("panelSidebar.autoUnloadOnClose")}
              </FormLabel>
              <Controller
                name="autoUnload"
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
          </FormControl>

          <FormControl>
            <Flex justifyContent="space-between">
              <FormLabel flex={1}>{t("panelSidebar.position")}</FormLabel>
              <Controller
                name="position_start"
                control={control}
                render={({ field: { onChange, value } }) => (
                  <RadioGroup
                    value={value ? "start" : "end"}
                    onChange={(val) => onChange(val === "start")}
                  >
                    <Stack direction="row" spacing={4}>
                      <Radio value="end">
                        {t("panelSidebar.positionLeft")}
                      </Radio>
                      <Radio value="start">
                        {t("panelSidebar.positionRight")}
                      </Radio>
                    </Stack>
                  </RadioGroup>
                )}
              />
            </Flex>
          </FormControl>

          <FormControl>
            <Flex justifyContent="space-between">
              <FormLabel flex={1}>{t("panelSidebar.globalWidth")}</FormLabel>
              <Controller
                name="globalWidth"
                control={control}
                render={({ field: { onChange, value } }) => (
                  <Input
                    type="number"
                    value={value}
                    width={"150px"}
                    onChange={(e) => onChange(Number(e.target.value))}
                  />
                )}
              />
            </Flex>
            <FormHelperText>
              {t("panelSidebar.globalWidthDescription")}
            </FormHelperText>
            <Alert status="info" rounded={"md"} mt={4}>
              <AlertIcon />
              <AlertDescription>
                {t("panelSidebar.iconProviderRemoved")}
                <br />
                <Link
                  color="blue.500"
                  href="https://docs.floorp.app/docs/features/how-to-use-workspaces"
                >
                  {t("design.learnMore")}
                </Link>
              </AlertDescription>
            </Alert>
          </FormControl>
        </VStack>
      </Card>
    </>
  );
}
