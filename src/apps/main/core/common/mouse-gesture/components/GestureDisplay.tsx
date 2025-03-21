/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createSignal, onCleanup, createRoot, Show } from "solid-js";
import { render } from "@nora/solid-xul";
import type { GestureDirection } from "../config.ts";

export function GestureDisplayUI(props: {
    trail: { x: number; y: number }[];
    actionName: string;
    isVisible: boolean;
    feedbackText?: string;
    feedbackVisible?: boolean;
    directions?: GestureDirection[];
}) {
    const directionSymbols = () => {
        if (!props.directions || props.directions.length === 0) return "";

        const directionSymbols: Record<GestureDirection, string> = {
            up: "↑",
            down: "↓",
            left: "←",
            right: "→",
        };

        return props.directions
            .map((dir) => directionSymbols[dir] || dir)
            .join(" ");
    };

    const getTrailElements = () => {
        if (props.trail.length < 2) return null;

        return props.trail.map((point, index) => {
            if (index === 0) return null;

            const prevPoint = props.trail[index - 1];
            const dx = point.x - prevPoint.x;
            const dy = point.y - prevPoint.y;
            const length = Math.sqrt(dx * dx + dy * dy);
            const angle = Math.atan2(dy, dx) * 180 / Math.PI;

            if (length < 2) return null;

            return (
                <div
                    style={{
                        position: "absolute",
                        height: "3px",
                        width: `${length}px`,
                        "background-color": "#FF0000",
                        left: `${prevPoint.x}px`,
                        top: `${prevPoint.y}px`,
                        transform: `rotate(${angle}deg)`,
                        "transform-origin": "left center",
                        "border-radius": "1.5px",
                    }}
                />
            );
        });
    };

    return (
        <>
            <Show when={props.isVisible}>
                <div
                    style={{
                        position: "fixed",
                        top: "0",
                        left: "0",
                        width: "100vw",
                        height: "100vh",
                        "pointer-events": "none",
                        "z-index": "999999",
                    }}
                >
                    <div
                        style={{
                            position: "absolute",
                            top: "0",
                            left: "0",
                            width: "100%",
                            height: "100%",
                            "pointer-events": "none",
                            overflow: "visible",
                        }}
                    >
                        {getTrailElements()}
                    </div>

                    <Show when={props.actionName}>
                        <div
                            style={{
                                position: "fixed",
                                bottom: "20px",
                                left: "50%",
                                transform: "translateX(-50%)",
                                "background-color": "rgba(0, 0, 0, 0.7)",
                                color: "white",
                                padding: "8px 16px",
                                "border-radius": "4px",
                                "font-size": "16px",
                                "font-weight": "bold",
                                "z-index": "1000000",
                            }}
                        >
                            {props.actionName}
                        </div>
                    </Show>
                </div>
            </Show>

            <Show when={props.feedbackVisible}>
                <div
                    style={{
                        position: "fixed",
                        top: "50%",
                        left: "50%",
                        transform: "translate(-50%, -50%)",
                        "background-color": "rgba(38, 38, 38, 0.95)",
                        color: "white",
                        padding: "16px 24px",
                        "border-radius": "4px",
                        "font-size": "16px",
                        "font-weight": "500",
                        "z-index": "10000",
                        "box-shadow": "0 4px 16px rgba(0, 0, 0, 0.5)",
                        "text-align": "center",
                        border: "1px solid rgba(82, 82, 82, 0.5)",
                        animation: "mouseGestureFeedbackIn 0.2s forwards",
                    }}
                >
                    {props.feedbackText}
                    <Show when={directionSymbols()}>
                        <div
                            style={{
                                "font-size": "14px",
                                color: "rgba(255, 255, 255, 0.8)",
                                "margin-top": "8px",
                            }}
                        >
                            {directionSymbols()}
                        </div>
                    </Show>
                </div>
                <style>
                    {`
                    @keyframes mouseGestureFeedbackIn {
                        from { opacity: 0; transform: translate(-50%, -50%) scale(0.8); }
                        to { opacity: 1; transform: translate(-50%, -50%) scale(1); }
                    }
                    @keyframes mouseGestureFeedbackOut {
                        from { opacity: 1; transform: translate(-50%, -50%) scale(1); }
                        to { opacity: 0; transform: translate(-50%, -50%) scale(1.1); }
                    }
                    `}
                </style>
            </Show>
        </>
    );
}

export class GestureDisplay {
    private mountContainer: HTMLDivElement | null = null;
    private trailSignal = createSignal<{ x: number; y: number }[]>([]);
    private actionSignal = createSignal<string>("");
    private visibleSignal = createSignal<boolean>(false);
    private feedbackTextSignal = createSignal<string>("");
    private feedbackVisibleSignal = createSignal<boolean>(false);
    private directionsSignal = createSignal<GestureDirection[]>([]);
    private feedbackElement: HTMLElement | null = null;

    constructor() {
        this.createMountPoint();
        this.initializeComponent();
    }

    private createMountPoint(): void {
        if (!document || !document.body) return;

        const existingContainer = document.getElementById("mouse-gesture-display-container");
        if (existingContainer && existingContainer.parentNode) {
            existingContainer.parentNode.removeChild(existingContainer);
        }

        this.mountContainer = document.createElement("div");
        this.mountContainer.id = "mouse-gesture-display-container";
        document.body.appendChild(this.mountContainer);

        render(() => (
            <div
                style={{
                    position: "fixed",
                    top: "0",
                    left: "0",
                    width: "100%",
                    height: "100%",
                    "pointer-events": "none",
                    "z-index": "99999",
                    overflow: "visible",
                }}
            />
        ), this.mountContainer, {
            hotCtx: import.meta.hot,
        });
    }

    private initializeComponent(): void {
        if (!this.mountContainer) return;

        createRoot((dispose) => {
            const [trail] = this.trailSignal;
            const [actionName] = this.actionSignal;
            const [isVisible] = this.visibleSignal;
            const [feedbackText] = this.feedbackTextSignal;
            const [feedbackVisible] = this.feedbackVisibleSignal;
            const [directions] = this.directionsSignal;

            render(() => (
                <GestureDisplayUI
                    trail={trail()}
                    actionName={actionName()}
                    isVisible={isVisible()}
                    feedbackText={feedbackText()}
                    feedbackVisible={feedbackVisible()}
                    directions={directions()}
                />
            ), this.mountContainer, {
                hotCtx: import.meta.hot,
            });

            onCleanup(() => dispose());
        });
    }

    public show(): void {
        this.visibleSignal[1](true);
    }
    public hide(): void {
        this.visibleSignal[1](false);
    }
    public updateTrail(points: { x: number; y: number }[]): void {
        const newPoints = [...points];

        if (newPoints.length > 100) {
            const stride = Math.ceil(newPoints.length / 100);
            const optimizedPoints = [];

            for (let i = 0; i < newPoints.length; i += stride) {
                optimizedPoints.push(newPoints[i]);
            }

            if (optimizedPoints[optimizedPoints.length - 1] !== newPoints[newPoints.length - 1]) {
                optimizedPoints.push(newPoints[newPoints.length - 1]);
            }

            this.trailSignal[1](optimizedPoints);
        } else {
            this.trailSignal[1](newPoints);
        }

        if (!this.visibleSignal[0]()) {
            this.visibleSignal[1](true);
        }
    }

    public updateActionName(name: string): void {
        this.actionSignal[1](name);
    }

    public showFeedback(text: string, directions?: GestureDirection[]): void {
        this.feedbackTextSignal[1](text);
        if (directions) {
            this.directionsSignal[1](directions);
        }
        this.feedbackVisibleSignal[1](true);
    }

    public hideFeedback(): void {
        this.feedbackVisibleSignal[1](false);
    }

    public destroy(): void {
        if (this.mountContainer && this.mountContainer.parentNode) {
            this.mountContainer.parentNode.removeChild(this.mountContainer);
        }
        this.mountContainer = null;
    }
}
