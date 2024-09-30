import {
  Flex,
  HStack,
  Icon,
  Input,
  InputGroup,
  InputLeftElement,
  Spacer,
  Text,
} from "@chakra-ui/react";

function Header() {
  return (
    <Flex
      justifyContent="space-between"
      alignItems="center"
      mb={4}
      top={"0px"}
      padding={"10px 2.5px"}
      position={"fixed"}
      width={"100%"}
      backgroundColor={"rgba(255,255,255, 0.7)"}
      backdropFilter={"blur(10px)"}
      zIndex={1000}
      borderBottom={"#eee 1px solid"}
    >
      <Text py={0} px={5} fontSize="2xl" pr={10}>
        Noraneko 設定
      </Text>
      <HStack spacing={2} maxW={"100%"} flex="1 1 auto">
        <InputGroup
          borderRadius={"8px"}
          maxW={"750px"}
          bg={"#f1f3f4"}
          border={"1px solid transparent"}
        >
          <InputLeftElement pointerEvents="none">
            <Icon as={"svg"} color="gray.300" />
          </InputLeftElement>
          <Input
            type="text"
            placeholder="設定を検索"
            _placeholder={{ color: "#000000" }}
          />
        </InputGroup>
      </HStack>
      <Spacer />
    </Flex>
  );
}

export default Header;
