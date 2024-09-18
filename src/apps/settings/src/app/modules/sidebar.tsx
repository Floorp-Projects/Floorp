"use client";

import React, { ReactNode } from "react";
import {
  IconButton,
  Box,
  CloseButton,
  Flex,
  Icon,
  useColorModeValue,
  Text,
  Drawer,
  DrawerContent,
  useDisclosure,
  type BoxProps,
  type FlexProps,
} from "@chakra-ui/react";
import Image from "next/image";
import noranekoIcon from "../icon_c_aq.svg";
import { Link } from "@chakra-ui/react";
import NextLink from "next/link";

interface LinkItemProps {
  name: string;
  href: string;
}
const LinkItems: Array<LinkItemProps> = [
  { name: "Home", href: "/" },
  { name: "Shortcuts", href: "/shortcuts" },
];

export default function SimpleSidebar() {
  return (
    <Box>
      <SidebarContent display={{ base: "none", md: "block" }} />
    </Box>
  );
}

interface SidebarProps extends BoxProps {}

const SidebarContent = ({ ...rest }: SidebarProps) => {
  return (
    <Box
      bg={useColorModeValue("white", "gray.900")}
      borderRight="1px"
      borderRightColor={useColorModeValue("gray.200", "gray.700")}
      w={"15rem"}
      h="full"
      pos={"fixed"}
      {...rest}
    >
      <Flex h="20" alignItems="center" mx="8" justifyContent="space-between">
        <Image
          priority
          src={noranekoIcon}
          alt="noraneko"
          width={60}
          height={60}
        />
        <Text fontSize="2xl" fontWeight="bold">
          Settings
        </Text>
      </Flex>
      {LinkItems.map((link) => (
        <NavItem key={link.name} href={link.href}>
          {link.name}
        </NavItem>
      ))}
    </Box>
  );
};

interface NavItemProps extends FlexProps {
  children: string;
  href: string;
}
const NavItem = ({ href, children, ...rest }: NavItemProps) => {
  return (
    <Box
      as="a"
      href={href}
      style={{ textDecoration: "none" }}
      _focus={{ boxShadow: "none" }}
    >
      <Flex
        align="center"
        p="4"
        mx="4"
        borderRadius="lg"
        role="group"
        cursor="pointer"
        _hover={{
          bg: "cyan.400",
          color: "white",
        }}
        {...rest}
      >
        {children}
      </Flex>
    </Box>
  );
};
