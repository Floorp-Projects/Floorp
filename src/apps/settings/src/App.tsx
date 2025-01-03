/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Box, useMediaQuery } from "@chakra-ui/react";
import Sidebar from "./sidebar";
import { BrowserRouter as Router, Route, Routes } from "react-router-dom";
import Home from "./pages/Home";
import Header from "./header";
import Workspaces from "./pages/Workspaces";
import Design from "./pages/Design";
import About from "./pages/About";
import SearchResults from "./pages/Search/index";
import PanelSidebar from "./pages/PanelSidebar";
import ProgressiveWebApp from "./pages/ProgressiveWebApp";
import ProfileAndAccount from "./pages/Accounts";

export default function App() {
  const [isMobile] = useMediaQuery("(max-width: 768px)");
  return (
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
                <Route path="/design" element={<Design />} />
                <Route path="/accounts" element={<ProfileAndAccount />} />
                <Route path="/about" element={<About />} />
                <Route path="/search" element={<SearchResults />} />
                <Route path="/sidebar" element={<PanelSidebar />} />
                <Route path="/webapps" element={<ProgressiveWebApp />} />
              </Routes>
            </Box>
          </Box>
        </Box>
      </Box>
    </Router>
  );
}
