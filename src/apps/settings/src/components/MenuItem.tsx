import { HStack, Text } from "@chakra-ui/react";
import { Link } from "react-router-dom";

const LinkItems = [
  { name: "ホーム", href: "/" },
  { name: "設定", href: "/preferences" },
  { name: "ショートカット", href: "/shortcuts" },
];

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
}) => (
  <Link to={to ?? ""} onClick={onClick}>
    <HStack
      spacing={2}
      align="center"
      px={4}
      py={3}
      borderRadius="0 25px 25px 0"
      bg={selected ? "blue.50" : "transparent"}
      _hover={{ bg: selected ? "blue.100" : "gray.100" }}
      transition="background-color 0.2s"
    >
      {icon}
      <Text>{text}</Text>
    </HStack>
  </Link>
);

export default MenuItem;
