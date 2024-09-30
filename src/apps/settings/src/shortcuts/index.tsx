import {
  Card,
  CardHeader,
  CardBody,
  CardFooter,
  SimpleGrid,
  Heading,
  Button,
  Box,
  Text,
} from "@chakra-ui/react";
import SimpleSidebar from "../modules/sidebar";
import React from "react";

export default function Home() {
  return (
    <>
      <div style={{ display: "flex", height: "100vh" }}>
        <section style={{ width: "15rem" }}>
          <SimpleSidebar />
        </section>

        <main style={{ display: "flex", flexDirection: "column" }}>
          <Text>Hello World!</Text>
          <Box display={"flex"}>Shortcuts!</Box>
        </main>
      </div>
    </>
  );
}
