/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  VStack,
  Text,
  RadioGroup,
  Radio,
  Alert,
  AlertIcon,
  AlertDescription,
  Link,
  Divider,
} from "@chakra-ui/react";
import Card from "../../components/Card";
import React from "react";
import { Controller, useFormContext } from "react-hook-form";
import type { DesignFormData } from "@/type";
import { useTranslation } from "react-i18next";

export default function Backup() {
  const { t } = useTranslation();
  const { control } = useFormContext<DesignFormData>();
  return (
    <Card
      icon={
        <IconMaterialSymbolsBackup
          style={{ fontSize: "24px", color: "#137333" }}
        />
      }
      title={t("workspaces.backup")}
      footerLink="https://support.google.com/chrome/?p=settings_workspaces"
    >
      <VStack align="stretch" spacing={4} paddingInlineStart={"10px"}>
        <Text fontSize="lg">{t("workspaces.backupComingSoon")}</Text>
      </VStack>
    </Card>
  );
}
