/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createMemo } from "solid-js";
import { getConfig } from "../config.ts";

export function GestureTrail(props: { points: { x: number; y: number }[] }) {
    const config = createMemo(() => getConfig());
    const pathData = createMemo(() => {
        if (props.points.length < 2) return "";

        let d = `M ${props.points[0].x} ${props.points[0].y}`;

        for (let i = 1; i < props.points.length; i++) {
            d += ` L ${props.points[i].x} ${props.points[i].y}`;
        }

        return d;
    });

    const strokeColor = createMemo(() => {
        const configColor = config().trailColor;
        return configColor || "#37ff00";
    });

    const strokeWidth = createMemo(() => {
        const configWidth = config().trailWidth;
        return configWidth || 3;
    });

    return (
        <path
            d={pathData()}
            stroke={strokeColor()}
            stroke-width={strokeWidth()}
            fill="none"
            stroke-linecap="round"
            stroke-linejoin="round"
            stroke-opacity="1"
            style="vector-effect: non-scaling-stroke; pointer-events: none;"
        />
    );
}