import type React from "react";
import { useRadio, type UseRadioProps } from "@chakra-ui/react";
import RadioCard from "./RadioCard";

interface ThemeCardProps extends UseRadioProps {
  title: string;
  image: string;
  isChecked: boolean;
}

const ThemeCard: React.FC<ThemeCardProps> = (props) => {
  const { getInputProps, getRadioProps } = useRadio(props);

  return (
    <RadioCard
      {...props}
      isChecked={props.isChecked}
      inputProps={getInputProps()}
      radioProps={getRadioProps()}
    />
  );
};

export default ThemeCard;
