/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import {
  Flex,
  Text,
  useColorModeValue,
  Link,
  Image,
  Spacer,
} from "@chakra-ui/react";
import Card from "../../components/Card";

export default function About() {
  const bgColor = useColorModeValue("white", "gray.900");
  const textColor = useColorModeValue("gray.800", "gray.100");

  return (
    <Flex
      direction="column"
      alignItems="center"
      maxW="700px"
      mx="auto"
      py={8}
      bg={bgColor}
    >
      <Text fontSize="3xl" mb={10} color={textColor}>
        About Noraneko
      </Text>
      <Card
        title="Noraneko"
        icon={
          <Image
            src={"chrome://branding/content/about-logo@2x.png"}
            boxSize="48px"
            alt="ロゴ"
          />
        }
        footerLink="https://noraneko.example.com/about"
        footerLinkText="More Information"
      >
        <Text fontSize="md" mb={3} color={textColor} w={"700px"}>
          Noraneko version: 12.0.0 Firefox version: 128.0.1
        </Text>

        <Text color={textColor} mb={4}>
          Noraneko is one of the domestically developed browsers in Japan. It is
          developed under Ablaze based on Firefox to make the web better.
        </Text>
      </Card>

      <Spacer my={4} />

      <Card>
        <Text mt={-2} fontSize={"sm"} w={"700px"}>
          Noraneko
        </Text>
        <Text color={textColor} fontSize="sm" mb={2}>
          © 2024 Noraneko Project. All rights reserved.
        </Text>
        <Text fontSize="sm" mb={4} color={textColor}>
          Noraneko is open-source software available to everyone under the
          Mozilla Public License 2.0, made possible by the Firefox open-source
          project and other open-source software.
        </Text>
        <Link
          href="https://docs.floorp.app/ja/docs/other/contributors"
          fontSize="sm"
          display="flex"
          alignItems="center"
          color={useColorModeValue("blue.500", "blue.400")}
        >
          Open Source License
          <IconIcOutlineOpenInNew
            style={{ fontSize: "16px", marginLeft: "4px" }}
          />
        </Link>
      </Card>
    </Flex>
  );
}
