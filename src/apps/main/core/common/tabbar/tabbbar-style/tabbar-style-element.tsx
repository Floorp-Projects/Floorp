/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import hideHorizontalTabbar from "./styles/hide-horizontal-tabbar.css?inline";
import optimiseToVerticalTabbar from "./styles/optimise-vertical-tabbar.css?inline";
import bottomOfNavigationToolbar from "./styles/bottom-of-navigation-toolbar.css?inline";
import bottomOfWindow from "./styles/bottom-of-window.css?inline";
import type { zFloorpDesignConfigsType } from "@core/common/designs/configs";

export function TabbarStyleModifyCSSElement(props: { style: zFloorpDesignConfigsType["tabbar"]["tabbarPosition"] }) {
    const styleSheet = () => {
        switch (props.style) {
            case "hide-horizontal-tabbar":
                return hideHorizontalTabbar;
            case "optimise-to-vertical-tabbar":
                return optimiseToVerticalTabbar;
            case "bottom-of-navigation-toolbar":
                return bottomOfNavigationToolbar;
            case "bottom-of-window":
                return bottomOfWindow;
        }
    };

    return <style id="floorp-tabbar-modify-css">{styleSheet()}</style>;
}
