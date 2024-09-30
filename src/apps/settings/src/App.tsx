import { getBoolPref, getIntPref, setBoolPref } from "./dev";
import { Box, ChakraProvider, extendTheme } from "@chakra-ui/react";
import { mode, StyleFunctionProps } from '@chakra-ui/theme-tools';
import Sidebar from "./sidebar";
import { BrowserRouter as Router, Route, Routes } from "react-router-dom";
import Home from "./pages/Home/Home";
import Header from "./header";

const createGlobalStyles = (props: Record<string, any>) => ({
  body: {
    color: mode('chakra-ui-text-color', 'whiteAlpha.900')(props),
    bg: mode('chakra-ui-body-bg', '#1a1a1a')(props),
  },
});

const createDrawerStyles = (props: StyleFunctionProps | Record<string, any>) => ({
  dialog: {
    bg: mode('chakra-ui-body-bg', '#141214')(props),
  },
});

const theme = {
  config: {
    initialColorMode: "system",
  },
  styles: {
    global: createGlobalStyles,
  },
  components: {
    Drawer: {
      baseStyle: createDrawerStyles,
    },
  },
};


export default function App() {
  setBoolPref("noraneko.settings.dev", true);
  getBoolPref("bidi.browser.ui").then((v) => {
    console.log(v);
  });
  return (
    <ChakraProvider theme={extendTheme(theme)}>
      <Router>
        <Box>
          <Header />
          <Box mt={"100px"}>
            <Sidebar />
            <Box>
              <Box p={"0px 48px"} mb={"48px"} ml={"300px"}>
                <Routes>
                  <Route path="/" element={<Home />} />
                </Routes>
              </Box>
            </Box>
          </Box>
        </Box>
      </Router>
    </ChakraProvider>
  );
}
