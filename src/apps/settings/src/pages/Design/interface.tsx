/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Controller, useFormContext } from "react-hook-form";
import ThemeCard from "@/components/ThemeCard";
import Card from "@/components/Card";
import type { DesignFormData } from "@/type";
import {
  Alert,
  AlertDescription,
  AlertIcon,
  Divider,
  Flex,
  Grid,
  RadioGroup,
  Link,
  Switch,
  Text,
  VStack,
  FormControl,
  FormLabel,
} from "@chakra-ui/react";
import { useTranslation } from "react-i18next";
import { useInterfaceDesigns } from "./designs";

export default function Interface() {
  const { control, watch } = useFormContext<DesignFormData>();
  const { t } = useTranslation();
  const options = useInterfaceDesigns();
  const design = watch("design");

  return (
    <Card
      icon={<IconMdiPen style={{ fontSize: "24px", color: "#3182F6" }} />}
      title={t("design.interface")}
      footerLink="https://docs.floorp.app/docs/features/design-customization"
      footerLinkText={t("design.aboutInterfaceDesign")}
    >
      <VStack align="stretch" spacing={4} paddingInlineStart={"10px"}>
        <Text fontSize="lg">{t("design.interface")}</Text>
        <Text fontSize="sm" mt={-1}>
          {t("design.interfaceDescription")}
        </Text>
        <Controller
          name="design"
          control={control}
          render={({ field: { onChange, value } }) => (
            <RadioGroup onChange={(val) => onChange(val)} value={value}>
              <Grid
                templateColumns={"repeat(auto-fill, minmax(180px, 1fr))"}
                gap={3}
              >
                {options.map((option) => (
                  <ThemeCard
                    key={option.value}
                    title={option.title}
                    image={option.image}
                    value={option.value}
                    isChecked={value === option.value}
                    onChange={() => onChange(option.value)}
                  />
                ))}
              </Grid>
            </RadioGroup>
          )}
        />
        {(design === "lepton" ||
          design === "photon" ||
          design === "protonfix") && (
          <Alert status="info" rounded={"md"} mt={2}>
            <AlertIcon />
            <AlertDescription>
              {t("design.advancedLeptonThemeSettingsDescription")}
              <br />
              <Link color="blue.500" href="https://support.mozilla.org">
                {t("design.advancedLeptonThemeSettings")}
              </Link>
            </AlertDescription>
          </Alert>
        )}
        <Divider />
        <Text fontSize="lg">{t("design.otherInterfaceSettings")}</Text>
        <Controller
          name="faviconColor"
          control={control}
          render={({ field: { onChange, value } }) => (
            <FormControl>
              <Flex justifyContent="space-between">
                <FormLabel flex={1}>
                  {t("design.useFaviconColorToBackgroundOfNavigationBar")}
                </FormLabel>
                <Switch
                  colorScheme={"blue"}
                  onChange={(e) => onChange(e.target.checked)}
                  isChecked={value}
                />
              </Flex>
            </FormControl>
          )}
        />
      </VStack>
    </Card>
  );
}
