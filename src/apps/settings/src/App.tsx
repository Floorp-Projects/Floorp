/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import Dashboard from "@/app/dashboard/page.tsx";
import Design from "@/app/design/page.tsx";
import PanelSidebar from "@/app/sidebar/page.tsx";
import Workspaces from "@/app/workspaces/page.tsx";
import { Route, Routes } from "react-router-dom";
import { SidebarProvider } from "@/components/ui/sidebar.tsx";
import { AppSidebar } from "@/components/app-sidebar.tsx";
import { Header } from "@/header/header.tsx";

// import Home from "./pages/Home";
// import Header from "./header";
// import Workspaces from "./pages/Workspaces";
// import Design from "./pages/Design";
// import About from "./pages/About";
// import SearchResults from "./pages/Search/index";
// import PanelSidebar from "./pages/PanelSidebar";
// import ProgressiveWebApp from "./pages/ProgressiveWebApp";
// import ProfileAndAccount from "./pages/Accounts";

export default function App() {
  return (
    <SidebarProvider>
      <div className="flex flex-col h-screen w-screen">
        <div className="flex flex-1">
          <AppSidebar />
          <div className="flex-1 max-w-3xl">
            <Header />
            <div className="p-4">
              <Routes>
                <Route path="/" element={<Dashboard />} />
                <Route path="/design" element={<Design />} />
                <Route path="/sidebar" element={<PanelSidebar />} />
                <Route path="/workspaces" element={<Workspaces />} />
                {
                  /*
                  <Route path="/accounts" element={<ProfileAndAccount />} />
                  <Route path="/about" element={<About />} />
                  <Route path="/search" element={<SearchResults />} />
                  <Route path="/webapps" element={<ProgressiveWebApp />} /> */
                }
              </Routes>
            </div>
          </div>
        </div>
      </div>
    </SidebarProvider>
  );
}
