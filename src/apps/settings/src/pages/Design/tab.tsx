/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  VStack,
  Text,
  RadioGroup,
  Radio,
  Divider,
  Flex,
  Switch,
  Input,
} from "@chakra-ui/react";
import Card from "../../components/Card";
import React from "react";
import { Controller, useFormContext } from "react-hook-form";
import type { DesignFormData } from "@/type";
import { useTranslation } from "react-i18next";

export default function Tab() {
  const { t } = useTranslation();
  const { control } = useFormContext<DesignFormData>();
  return (
    <Card
      icon={
        <IconFluentMdl2Tab style={{ fontSize: "24px", color: "#ff7708" }} />
      }
      title={"タブ"}
      footerLink="https://noraneko.example.com/help/customize-tab-bar"
      footerLinkText={"タブのカスタマイズについて"}
    >
      <VStack align="stretch" spacing={4} paddingInlineStart={"10px"}>
        <Text fontSize="lg">スクロール</Text>
        <Controller
          name="tabScroll"
          control={control}
          render={({ field: { onChange, value } }) => (
            <Flex justifyContent="space-between" alignItems="center">
              <Text>タブをスクロールで切り替える</Text>
              <Switch
                colorScheme={"blue"}
                onChange={(e) => onChange(e.target.checked)}
                isChecked={value}
              />
            </Flex>
          )}
        />
        <Controller
          name="tabScrollReverse"
          control={control}
          render={({ field: { onChange, value } }) => (
            <Flex justifyContent="space-between" alignItems="center">
              <Text>タブのスクロール方向を反転する</Text>
              <Switch
                colorScheme={"blue"}
                onChange={(e) => onChange(e.target.checked)}
                isChecked={value}
              />
            </Flex>
          )}
        />
        <Controller
          name="tabScrollWrap"
          control={control}
          render={({ field: { onChange, value } }) => (
            <Flex justifyContent="space-between" alignItems="center">
              <Text>タブのスクロールをループさせる</Text>
              <Switch
                colorScheme={"blue"}
                onChange={(e) => onChange(e.target.checked)}
                isChecked={value}
              />
            </Flex>
          )}
        />
        <Divider />
        <Text fontSize="lg">新しいタブを開く位置</Text>
        <Controller
          name="tabOpenPosition"
          control={control}
          render={({ field: { onChange, value, ref } }) => (
            <RadioGroup
              display="flex"
              flexDirection="column"
              gap={4}
              onChange={(val) => onChange(Number(val))}
              value={value?.toString()}
              ref={ref}
            >
              <Radio value="-1">デフォルトの設定（Firefox に従う）</Radio>
              <Radio value="0">タブバーの最後尾に開く</Radio>
              <Radio value="1">現在のタブの隣に開く</Radio>
            </RadioGroup>
          )}
        />
        <Divider />
        <Text fontSize="lg">その他のタブ設定</Text>
        <Controller
          name="tabPinTitle"
          control={control}
          render={({ field: { onChange, value } }) => (
            <Flex justifyContent="space-between" alignItems="center">
              <Text>ピン止めされたタブのタイトルを表示する</Text>
              <Switch
                colorScheme={"blue"}
                onChange={(e) => onChange(e.target.checked)}
                isChecked={value}
              />
            </Flex>
          )}
        />
        <Controller
          name="tabDubleClickToClose"
          control={control}
          render={({ field: { onChange, value } }) => (
            <Flex justifyContent="space-between" alignItems="center">
              <Text>タブをダブルクリックで閉じる</Text>
              <Switch
                colorScheme={"blue"}
                onChange={(e) => onChange(e.target.checked)}
                isChecked={value}
              />
            </Flex>
          )}
        />
        <Controller
          name="tabMinWidth"
          control={control}
          render={({ field: { onChange, value } }) => (
            <Flex justifyContent="space-between" alignItems="center">
              <Text>タブの最小幅</Text>
              <Input
                width="200px"
                min={60}
                max={100}
                type="number"
                onChange={(e) => onChange(Number(e.target.value))}
                value={value}
              />
            </Flex>
          )}
        />
        <Controller
          name="tabMinHeight"
          control={control}
          render={({ field: { onChange, value } }) => (
            <Flex justifyContent="space-between" alignItems="center">
              <Text>タブの最小高さ</Text>
              <Input
                width="200px"
                height="40px"
                min={60}
                max={100}
                type="number"
                value={value}
                onChange={(e) => onChange(Number(e.target.value))}
              />
            </Flex>
          )}
        />
      </VStack>
    </Card>
  );
}
