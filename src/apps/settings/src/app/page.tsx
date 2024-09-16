import Image from "next/image";
import styles from "./page.module.css";

import {
  Card,
  CardHeader,
  CardBody,
  CardFooter,
  SimpleGrid,
  Heading,
  Button,
} from "@chakra-ui/react";

export default function Home() {
  return (
    <div className={styles.page}>
      <main className={styles.main}>
        <SimpleGrid
          spacing={4}
          templateColumns="repeat(auto-fill, minmax(200px, 1fr))"
        >
          <Card>
            <CardHeader>
              <Heading size="md"> Customer dashboard</Heading>
            </CardHeader>
            <CardBody>
              View a summary of all your customers over the last month.
            </CardBody>
            <CardFooter>
              <Button>View here</Button>
            </CardFooter>
          </Card>
          <Card>
            <CardHeader>
              <Heading size="md"> Customer dashboard</Heading>
            </CardHeader>
            <CardBody>
              View a summary of all your customers over the last month.
            </CardBody>
            <CardFooter>
              <Button>View here</Button>
            </CardFooter>
          </Card>
          <Card>
            <CardHeader>
              <Heading size="md"> Customer dashboard</Heading>
            </CardHeader>
            <CardBody>
              View a summary of all your customers over the last month.
            </CardBody>
            <CardFooter>
              <Button>View here</Button>
            </CardFooter>
          </Card>
        </SimpleGrid>
      </main>
    </div>
  );
}
