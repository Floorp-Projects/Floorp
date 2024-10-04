import {
  Flex,
  VStack,
  Text,
  Switch,
  Input,
  MenuList,
  MenuItem,
  Menu,
  MenuButton,
  Button,
} from "@chakra-ui/react";
import Card from "../../components/Card";
import React from "react";
import Interface from "./interface";
import Tabbar from "./tabbar";

export default function Design() {
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
        <Interface />
        <Tabbar />
      </VStack>
    </Flex>
  );
}
