import {
  Card,
  CardHeader,
  CardBody,
  CardFooter,
  Heading,
  Button,
  Box,
  Text,
} from "@chakra-ui/react";
import SimpleSidebar from "./modules/sidebar";
// import { getBoolPref, getIntPref, setBoolPref } from "./dev";

export default function Home() {
  // setBoolPref("noraneko.settings.dev", true);
  // getBoolPref("bidi.browser.ui").then((v) => {
  //   console.log(v);
  // });

  return (
    <>
      <div style={{ display: "flex", height: "100vh" }}>
        <section style={{ width: "15rem" }}>
          <SimpleSidebar />
        </section>

        <main style={{ display: "flex", flexDirection: "column" }}>
          <Text>Hello World!</Text>
          <Box display={"flex"}>
            <Card width={"200px"} height={"300px"}>
              <CardHeader>
                <Heading size="md">
                  {" "}
                  Customer dashboard {process.env.NODE_ENV}
                </Heading>
              </CardHeader>
              <CardBody>
                View a summary of all your customers over the last month.
              </CardBody>
              <CardFooter>
                <Button>View here</Button>
              </CardFooter>
            </Card>
            <Card width={"200px"} height={"300px"}>
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
          </Box>
        </main>
      </div>

      {/* <div className={styles.page}>
        <main className={styles.main}>
          <div>The left space</div>


        </main>
      </div> */}
    </>
  );
}
