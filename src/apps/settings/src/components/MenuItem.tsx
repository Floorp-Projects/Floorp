import { HStack, Text, useColorMode } from "@chakra-ui/react";
import { Link } from "react-router-dom";

const MenuItem = ({
  icon,
  text,
  to,
  selected,
  onClick,
}: {
  icon: React.ReactNode;
  text: string;
  to?: string;
  selected?: boolean;
  onClick?: () => void;
}) => {
  const { colorMode } = useColorMode();

  return (
    <Link to={to ?? ""} onClick={onClick}>
      <HStack
        spacing={2}
        align="center"
        px={4}
        py={3}
        borderRadius="0 25px 25px 0"
        bg={selected ? (colorMode === "light" ? "blue.50" : "blue.900") : "transparent"}
        _hover={{
          bg: selected
            ? colorMode === "light"
              ? "blue.100"
              : "blue.800"
            : colorMode === "light"
            ? "gray.100"
            : "gray.700",
        }}
        transition="background-color 0.2s"
      >
        {icon}
        <Text>{text}</Text>
      </HStack>
    </Link>
  );
};

export default MenuItem;
