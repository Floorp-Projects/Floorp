/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GridItem, VStack, Divider } from "@chakra-ui/react";
import MenuItem from "../components/MenuItem";
import { useMediaQuery } from "@chakra-ui/react";
import { useLocation } from "react-router-dom";
import { pages } from "../pageData";
import { useEffect } from "react";

function Sidebar() {
  const [isMobile] = useMediaQuery("(max-width: 768px)");
  const location = useLocation();

  useEffect(() => {
    if (document) {
      const title =
        pages[location.pathname === "/" ? "home" : location.pathname.slice(1)]
          ?.text ?? "Noraneko Settings";
      document.title = title;
    }
  }, [location.pathname]);

  return (
    <GridItem
      zIndex={1000}
      w={isMobile ? "75px" : "300px"}
      maxH={"calc(100vh - 100px)"}
      position={"fixed"}
      overflowY={"scroll"}
    >
      <VStack align="stretch" spacing={2}>
        {Object.keys(pages).map((key) => {
          return (
            <MenuItem
              key={key}
              to={pages[key].path}
              icon={pages[key].icon}
              text={pages[key].text}
              selected={location.pathname === pages[key].path}
            />
          );
        })}
        <Divider />
        <MenuItem
          icon={
            <IconMdiSettings
              style={{ fontSize: "16px", color: "currentColor" }}
            />
          }
          text="Firefox Settings"
          onClick={() => {
            window.NRAddTab("about:preferences");
          }}
        />
      </VStack>
    </GridItem>
  );
}

export default Sidebar;
