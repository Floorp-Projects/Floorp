import { Flex, VStack, Text } from "@chakra-ui/react";
import React, { useEffect } from "react";
import Interface from "./interface";
import Tabbar from "./tabbar";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import { saveDesignSettings } from "./saveDesignPref";
import type { DesignFormData } from "@/type";

export default function Design() {
  const methods = useForm<DesignFormData>({
    defaultValues: {
      design: "lepton",
      faviconColor: false,
      style: "multirow",
      position: "bottom-of-window",
    },
  });

  const watchAll = useWatch({
    control: methods.control,
  });

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
