import { VStack, Text, RadioGroup, Radio, Alert, AlertIcon, AlertDescription, Link } from "@chakra-ui/react";
import Card from "../../components/Card";
import React, { useEffect } from "react";
import { Controller, useForm, useWatch } from "react-hook-form";

type FormData = {
  style: number;
  postion: number;
};

export default function Tabbar() {
  const { control } = useForm<FormData>({
    defaultValues: {
      style: 0,
      postion: 0,
    },
  });

  const watchAll = useWatch({
    control,
  });

  useEffect(() => {
    console.log(watchAll);
  }, [watchAll]);

  return (
    <Card
      icon={
        <IconCarbonWorkspace style={{ fontSize: "24px", color: "#3182F6" }} />
      }
      title="Tab Bar"
      footerLink="https://support.google.com/chrome/?p=settings_workspaces"
      footerLinkText="Customize Tab bar"
    >
      <VStack align="stretch" spacing={4} paddingInlineStart={"10px"}>
        <Text fontSize="lg">Tab Bar Style</Text>
        <Text fontSize="sm" mt={-1}>
          Select the style og the tabbar. "Multi-row" stacks tab on multiple
          rows reducing tab size
        </Text>
        <Controller
          name="style"
          control={control}
          render={({ field: { onChange, value } }) => (
            <RadioGroup
              display="flex"
              flexDirection="column"
              gap={4}
              onChange={(val) => onChange(Number(val))}
              value={value.toString()}
            >
              <Radio value="0">Horizontal</Radio>
              <Radio value="1">Multi-row</Radio>
            </RadioGroup>
          )}
        />
        <Alert status="info" rounded={"md"}>
          <AlertIcon />
          <AlertDescription>
            Vertical Tab is removed from Noraneko. Use Firefox native Vertical Tab instead
            <br />
            <Link color="blue.500" href="https://support.mozilla.org">
              Learn more
            </Link>
          </AlertDescription>
        </Alert>
      </VStack>
    </Card>
  );
}
