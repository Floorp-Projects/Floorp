/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Flex, VStack, Text } from "@chakra-ui/react";
import React, { useEffect } from "react";
import Interface from "./interface";
import Tabbar from "./tabbar";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import { getDesignSettings, saveDesignSettings } from "./saveDesignPref";
import type { DesignFormData } from "@/type";

export default function Design() {
  const methods = useForm<DesignFormData>({
    defaultValues: {},
  });
  const { setValue } = methods;
  const watchAll = useWatch({
    control: methods.control,
  });

  useEffect(() => {
    const fetchDefaultValues = async () => {
      const values = await getDesignSettings();
      console.log(values)
      for (const key in values) {
        setValue(key, values[key]);
      }
    };
    fetchDefaultValues();
  }, [setValue]);

  useEffect(() => {
    saveDesignSettings(watchAll as DesignFormData);
  }, [watchAll]);

  return (
    <Flex direction="column" alignItems="center" maxW="700px" mx="auto" py={8}>
      <Text fontSize="3xl" mb={10}>
        Look & Feel
      </Text>
      <Text mb={8}>
        Customize position of toolbars, change the style of tab bar, and
        customize appearance of Noraneko.
      </Text>

      <VStack align="stretch" spacing={6} w="100%">
        <FormProvider {...methods}>
          <Interface />
          <Tabbar />
        </FormProvider>
      </VStack>
    </Flex>
  );
}
