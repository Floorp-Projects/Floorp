import {
  Box,
  Divider,
  HStack,
  Text,
  VStack,
  Link,
  Spacer,
} from "@chakra-ui/react";
import type React from "react";

function Card({
  title,
  children,
  icon,
  footerLink,
  footerChildren,
  footerLinkText,
}: {
  title: string;
  children: React.ReactNode;
  icon?: React.ReactNode;
  footerLink?: string;
  footerLinkText?: string;
  footerChildren?: React.ReactNode;
}) {
  return (
    <Box borderWidth={1} borderRadius="md" p={"15px 20px"}>
      <HStack>
        {icon}
        <Text
          fontFamily={`"Google Sans",Roboto,A1rial,sans-serif`}
          lineHeight={"1.75em"}
          fontSize={"1.375rem"}
          fontWeight={"400"}
        >
          {title}
        </Text>
      </HStack>
      {children}
      {(footerLinkText && footerLink) || footerChildren ? (
        <Footer footerLink={footerLink} footerLinkText={footerLinkText} />
      ) : null}
    </Box>
  );
}
export default Card;

function Footer({
  footerChildren,
  footerLink,
  footerLinkText,
}: {
  footerChildren?: React.ReactNode;
  footerLink?: string;
  footerLinkText?: string;
}) {
  return (
    <>
      <Divider ml={"-20px"} pr={"40px"} mt={4} />
      <Link href={footerLink} _hover={{ textDecoration: "none" }}>
        <HStack align="flex-start" m={"12.5px 10px -5px 5px"} p={"10px"}  rounded={"15px"} _hover={{ bg: "chakra-body-bg" }}>
          {footerLink ? (
            <Text color="#8ab4f8" fontSize="sm" >
              {footerLinkText}
            </Text>
          ) : (
            footerChildren
          )}
          <Spacer />
          <IconIcOutlineOpenInNew style={{ fontSize: '16px' }} />
        </HStack>
      </Link>
    </>
  );
}
