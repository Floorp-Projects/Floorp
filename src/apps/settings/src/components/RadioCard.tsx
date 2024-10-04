import type React from "react";
import { Box, Image, Text } from "@chakra-ui/react";

interface RadioCardProps {
  title: string;
  image: string;
  isChecked: boolean;
  inputProps: React.InputHTMLAttributes<HTMLInputElement>;
  radioProps: React.HTMLAttributes<HTMLDivElement>;
}

const RadioCard: React.FC<RadioCardProps> = ({ title, image, isChecked, inputProps, radioProps }) => {
  return (
    <Box as="label">
      <input {...inputProps} />
      <Box
        {...radioProps}
        cursor="pointer"
        borderWidth="2px"
        borderRadius="md"
        boxShadow="md"
        borderColor={isChecked ? "blue.500" : "gray.200"}
        px={2}
        pt={2}
        pb={1}
        width="138px"
        height="102px"
      >
        <Image
          src={image}
          alt={title}
          h="70px"
          w="118px"
          rounded="md"
        />
        <Text fontSize="15px" fontWeight="600" mt={1.5}>
          {title}
        </Text>
      </Box>
    </Box>
  );
};

export default RadioCard;
