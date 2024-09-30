import { getBoolPref, getIntPref, setBoolPref } from "./dev";
import { Box } from "@chakra-ui/react";
import PreferencesHeader from "./header";
import Sidebar from "./sidebar";
import { BrowserRouter as Router, Route, Routes } from 'react-router-dom';
import Home from "./pages/Home/Home";

export default function App() {
  setBoolPref("noraneko.settings.dev", true);
  getBoolPref("bidi.browser.ui").then((v) => {
    console.log(v);
  });
  return (
    <Router>
      <Box>
        <PreferencesHeader />
        <Box mt={"80px"}>
          <Sidebar />
          <Box>
            <Box p={"0px 48px"} ml={"250px"}>
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
