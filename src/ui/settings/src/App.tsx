/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import Dashboard from "@/app/dashboard/page.tsx";
import Design from "@/app/design/page.tsx";
import PanelSidebar from "@/app/sidebar/page.tsx";
import Workspaces from "@/app/workspaces/page.tsx";
import ProgressiveWebApp from "@/app/pwa/page.tsx";
import About from "./app/about/noraneko.tsx";
import { Route, Routes, Navigate } from "react-router-dom";
import { SidebarProvider } from "@/components/ui/sidebar.tsx";
import { AppSidebar } from "@/components/app-sidebar.tsx";
import { Header } from "@/header/header.tsx";

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
                <Route path="/" element={<Navigate to="/overview/home" replace />} />
                <Route path="/overview/home" element={<Dashboard />} />
                <Route path="/features/design" element={<Design />} />
                <Route path="/features/sidebar" element={<PanelSidebar />} />
                <Route path="/features/workspaces" element={<Workspaces />} />
                <Route path="/features/webapps" element={<ProgressiveWebApp />} />
                <Route path="/about/browser" element={<About />} />
                {
                  /*
                  <Route path="/accounts" element={<ProfileAndAccount />} />
                  <Route path="/search" element={<SearchResults />} />
                 */
                }
              </Routes>
            </div>
          </div>
        </div>
      </div>
    </SidebarProvider>
  );
}
