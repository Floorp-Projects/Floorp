import type React from "react";
import { Box, Card, CardBody, Image, Text } from "@chakra-ui/react";

interface RadioCardProps {
  title: string;
  image: string;
  isChecked: boolean;
  inputProps: React.InputHTMLAttributes<HTMLInputElement>;
  radioProps: React.HTMLAttributes<HTMLDivElement>;
}

const RadioCard: React.FC<RadioCardProps> = ({
  title,
  image,
  isChecked,
  inputProps,
  radioProps,
}) => {
  return (
    <Card
      as="label"
      width="140px"
      height="100px"
      cursor="pointer"
      borderWidth="2px"
      borderRadius="md"
      boxShadow="md"
      borderColor={isChecked ? "blue.500" : "transparent"}
    >
      <CardBody px={2} pt={2} pb={1}>
        <input {...inputProps} />
        <Box {...radioProps}>
          <Image src={image} alt={title} h="62.5px" w="128px" rounded="md" />
          <Text fontSize="15px" fontWeight="600" mt={1.4}>
            {title}
          </Text>
        </Box>
      </CardBody>
    </Card>
  );
};

export default RadioCard;
