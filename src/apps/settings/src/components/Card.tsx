import { Box, HStack, Icon, Text } from "@chakra-ui/react";

function Card({ title, children, icon }: { title: string; children: React.ReactNode, icon?: React.ReactNode }) {
    return (
    <Box borderWidth={1} borderRadius="md" p={4}>
      <HStack>
        {icon}
        <Text fontFamily={`"Google Sans",Roboto,Arial,sans-serif`} lineHeight={"1.75em"} fontSize={"1.375rem"} fontWeight={"400"}>{title}</Text>
      </HStack>
      {children}
    </Box>
  );
}
export default Card;
