import { Controller, useFormContext } from "react-hook-form";
import type { zFloorpDesignConfigsType } from "../../../../common/scripts/global-types/type";
import ThemeCard from "../../components/ThemeCard";
import Card from "../../components/Card";
import type { DesignFormData } from "@/type";
import {
  Divider,
  Flex,
  Grid,
  RadioGroup,
  Switch,
  Text,
  VStack,
} from "@chakra-ui/react";

const options: {
  value: zFloorpDesignConfigsType["globalConfigs"]["userInterface"];
  title: string;
  image: string;
}[] = [
  {
    value: "proton",
    title: "Proton",
    image:
      "https://floorp.app/_next/image?url=%2F_next%2Fstatic%2Fmedia%2Fhero.c2bc9f0f.webp&w=1080&q=75",
  },
  {
    value: "lepton",
    title: "Lepton",
    image:
      "https://floorp.app/_next/image?url=%2F_next%2Fstatic%2Fmedia%2Fhero.c2bc9f0f.webp&w=1080&q=75",
  },
  {
    value: "photon",
    title: "Photon",
    image:
      "https://floorp.app/_next/image?url=%2F_next%2Fstatic%2Fmedia%2Fhero.c2bc9f0f.webp&w=1080&q=75",
  },
  {
    value: "protonfix",
    title: "Firefox UI Fix",
    image:
      "https://floorp.app/_next/image?url=%2F_next%2Fstatic%2Fmedia%2Fhero.c2bc9f0f.webp&w=1080&q=75",
  },
  {
    value: "fluerial",
    title: "Fluerial",
    image:
      "https://floorp.app/_next/image?url=%2F_next%2Fstatic%2Fmedia%2Fhero.c2bc9f0f.webp&w=1080&q=75",
  },
];

export default function Interface() {
  const { control } = useFormContext<DesignFormData>();
  return (
    <Card
      icon={<IconMdiPen style={{ fontSize: "24px", color: "#3182F6" }} />}
      title="Interface"
      footerLink="https://support.google.com/chrome/?p=settings_workspaces"
      footerLinkText="About Interface Design"
    >
      <VStack align="stretch" spacing={4} paddingInlineStart={"10px"}>
        <Text fontSize="lg">Design</Text>
        <Text fontSize="sm" mt={-1}>
          Select the design of the interface. "Lepton" is the default design.
        </Text>
        <Controller
          name="design"
          control={control}
          render={({ field: { onChange, value } }) => (
            <RadioGroup
              onChange={(val) => onChange(val)}
              value={value}
            >
              <Grid
                templateColumns={"repeat(auto-fill, minmax(140px, 1fr))"}
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
        <Divider />
        <Text fontSize="lg">Other Interface Settings</Text>
        <Controller
          name="faviconColor"
          control={control}
          render={({ field: { onChange, value } }) => (
            <Flex justifyContent="space-between" alignItems="center">
              <Text>Use Favicon Color to Background of Navigation Bar</Text>
              <Switch
                size="lg"
                colorScheme={"blue"}
                onChange={(e) => onChange(e.target.checked)}
                checked={value}
              />
            </Flex>
          )}
        />
      </VStack>
    </Card>
  );
}
