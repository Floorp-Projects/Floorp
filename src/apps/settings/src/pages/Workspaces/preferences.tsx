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
  Flex,
  Switch,
  FormLabel,
  FormControl,
  FormHelperText,
} from "@chakra-ui/react";
import Card from "../../components/Card";
import React from "react";
import { Controller, useFormContext } from "react-hook-form";
import { useTranslation } from "react-i18next";
import type { WorkspacesFormData } from "../../type";

export default function Preferences() {
  const { t } = useTranslation();
  const { control } = useFormContext<WorkspacesFormData>();

  return (
    <Card
      icon={
        <IconMaterialSymbolsLightSelectWindow
          style={{ fontSize: "24px", color: "#3164ff" }}
        />
      }
      title="ワークスペースの基本設定"
      footerLink="https://support.google.com/chrome/?p=settings_workspaces"
      footerLinkText="ワークスペースの使い方とカスタマイズについて"
    >
      <VStack align="stretch" spacing={4} paddingInlineStart={"10px"}>
        <Text fontSize="lg">有効化・無効化</Text>
        <FormControl>
          <Flex justifyContent="space-between">
            <FormLabel flex={1}>ワークスペース機能を有効にする</FormLabel>
            <Controller
              name="enabled"
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
            ワークスペースのボタンがツールバーに配置されていない限り、この設定を有効にしてもワークスペースは機能しません。ワークスペースを完全に無効にしたい場合は、この設定を使用してください。
          </FormHelperText>
        </FormControl>

        <Divider />

        <Text fontSize="lg">その他のワークスペース設定</Text>
        <FormControl>
          <Flex justifyContent="space-between">
            <FormLabel flex={1}>
              ワークスペース選択時にワークスペースのポップアップを閉じる
            </FormLabel>
            <Controller
              name="closePopupAfterClick"
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
            <FormLabel flex={1}>ワークスペース名をアイコンの隣に表示</FormLabel>
            <Controller
              name="showWorkspaceNameOnToolbar"
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
            <FormLabel flex={1}>
              Noraneko サイドバー上でワークスペースを管理する
            </FormLabel>
            <Controller
              name="manageOnBms"
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
            この機能を有効にすると、Noraneko
            サイドバー上のみでワークスペースを管理できます。ツールバーボタンを使用したワークスペースの管理はできなくなります。
          </FormHelperText>
        </FormControl>
      </VStack>
    </Card>
  );
}
