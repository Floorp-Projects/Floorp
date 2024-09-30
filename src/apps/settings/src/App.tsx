import { getBoolPref, getIntPref, setBoolPref } from "./dev";
import { Box } from "@chakra-ui/react";
import Sidebar from "./sidebar";
import { BrowserRouter as Router, Route, Routes } from 'react-router-dom';
import Home from "./pages/Home/Home";
import Header from "./header";

export default function App() {
  setBoolPref("noraneko.settings.dev", true);
  getBoolPref("bidi.browser.ui").then((v) => {
    console.log(v);
  });
  return (
    <Router>
      <Box>
        <Header/>
        <Box mt={"80px"}>
          <Sidebar />
          <Box>
            <Box p={"0px 48px"} ml={"300px"}>
              <Routes>
                <Route path="/" element={<Home />} />
              </Routes>
            </Box>
          </Box>
        </Box>
      </Box>
    </Router>
  );
}
