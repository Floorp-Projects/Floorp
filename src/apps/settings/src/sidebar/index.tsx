/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GridItem, VStack, Divider } from "@chakra-ui/react";
import MenuItem from "../components/MenuItem";
import { useMediaQuery } from "@chakra-ui/react";
import { useLocation } from "react-router-dom";
import { usePageData } from "../pageData";
import { useEffect } from "react";
import { useTranslation } from "react-i18next";

function Sidebar() {
  const [isMobile] = useMediaQuery("(max-width: 768px)");
  const location = useLocation();
  const pageData = usePageData();
  const { t } = useTranslation();

  useEffect(() => {
    if (document) {
      const pathKey =
        location.pathname === "/" ? "home" : location.pathname.slice(1);
      const title =
        pageData[pathKey as keyof typeof pageData]?.text ?? "Noraneko Settings";
      document.title = title;
    }
  }, [location.pathname, pageData]);

  return (
    <GridItem
      zIndex={1000}
      w={isMobile ? "75px" : "300px"}
      maxH={"calc(100vh - 100px)"}
      position={"fixed"}
      overflowY={"scroll"}
    >
      <VStack align="stretch" spacing={2}>
        {Object.keys(usePageData()).map((key) => {
          return (
            <MenuItem
              key={key}
              to={pageData[key as keyof typeof pageData].path}
              icon={pageData[key as keyof typeof pageData].icon}
              text={pageData[key as keyof typeof pageData].text}
              selected={
                location.pathname ===
                pageData[key as keyof typeof pageData].path
              }
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
          text={t("sidebar.firefoxSettings")}
          onClick={() => {
            window.NRAddTab("about:preferences");
          }}
        />
      </VStack>
    </GridItem>
  );
}

export default Sidebar;
