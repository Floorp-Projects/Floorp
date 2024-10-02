import { getBoolPref, getIntPref, setBoolPref } from "./dev";
import { Box, ChakraProvider, useMediaQuery } from "@chakra-ui/react";
import Sidebar from "./sidebar";
import { BrowserRouter as Router, Route, Routes } from "react-router-dom";
import Home from "./pages/Home";
import Header from "./header";
import Workspaces from "./pages/Workspaces";
import customTheme from "./theme";


export default function App() {
  const [isMobile] = useMediaQuery("(max-width: 768px)")
  setBoolPref("noraneko.settings.dev", true);
  getBoolPref("bidi.browser.ui").then((v) => {
    console.log(v);
  });
  return (
    <ChakraProvider theme={customTheme}>
      <Router>
        <Box>
          <Header />
          <Box mt={"100px"}>
            <Sidebar />
            <Box>
              <Box p={"0px 48px"} mb={"48px"} ml={isMobile ? "75px" : "300px"}>
                <Routes>
                  <Route path="/" element={<Home />} />
                  <Route path="/workspaces" element={<Workspaces />} />
                </Routes>
              </Box>
            </Box>
          </Box>
        </Box>
      </Router>
    </ChakraProvider>
  );
}
