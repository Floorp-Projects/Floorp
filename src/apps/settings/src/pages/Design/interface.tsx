import { useForm, Controller, useWatch } from "react-hook-form";
import { RadioGroup, Stack } from "@chakra-ui/react";
import ThemeCard from "../../components/ThemeCard";
import React, { useEffect } from "react";

interface FormData {
  theme: number;
}

const options = [
  {
    value: 1,
    title: "Proton",
    image:
      "https://floorp.app/_next/image?url=%2F_next%2Fstatic%2Fmedia%2Fhero.c2bc9f0f.webp&w=1080&q=75",
  },
  {
    value: 2,
    title: "Lepton",
    image:
      "https://floorp.app/_next/image?url=%2F_next%2Fstatic%2Fmedia%2Fhero.c2bc9f0f.webp&w=1080&q=75",
  },
  {
    value: 3,
    title: "Photon",
    image:
      "https://floorp.app/_next/image?url=%2F_next%2Fstatic%2Fmedia%2Fhero.c2bc9f0f.webp&w=1080&q=75",
  },
  {
    value: 4,
    title: "Firefox UI Fix",
    image:
      "https://floorp.app/_next/image?url=%2F_next%2Fstatic%2Fmedia%2Fhero.c2bc9f0f.webp&w=1080&q=75",
  },
  {
    value: 5,
    title: "Fluerial",
    image:
      "https://floorp.app/_next/image?url=%2F_next%2Fstatic%2Fmedia%2Fhero.c2bc9f0f.webp&w=1080&q=75",
  },
];

export default function Interface() {
  const { control } = useForm<FormData>({
    defaultValues: {
      theme: 1,
    },
  });

  const watchAll = useWatch({
    control,
  });

  useEffect(() => {
    console.log(watchAll);
  }, [watchAll]);

  return (
    <Controller
      name="theme"
      control={control}
      render={({ field: { onChange, value } }) => (
        <RadioGroup
          onChange={(val) => onChange(Number(val))}
          value={value.toString()}
        >
          <Stack direction="row" spacing={4}>
            {options.map((option) => (
              <ThemeCard
                key={option.value}
                title={option.title}
                image={option.image}
                value={option.value.toString()}
                isChecked={value === option.value}
                onChange={() => onChange(option.value)}
              />
            ))}
          </Stack>
        </RadioGroup>
      )}
    />
  );
}
