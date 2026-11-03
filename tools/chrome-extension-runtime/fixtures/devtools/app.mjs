// SPDX-License-Identifier: MPL-2.0
// Actual framework integrations; extension hooks are supplied only by real packages.
import React from "react";
import { createRoot } from "react-dom/client";
import { createStore } from "redux";
/** @import {ReactCounterProps,CounterState,AuthoredAction,BrowserFrameworkGlobal} from "../../assessment-test-types.ts" */
const browserGlobal =
  /** @type {BrowserFrameworkGlobal} */ (/** @type {unknown} */ (globalThis));
const { document } = browserGlobal;
/** @param {ReactCounterProps} props */
function FloorpCounterLabel({ count }) {
  return React.createElement(
    "output",
    { id: "react-count" },
    `React count: ${count}`,
  );
}
function FloorpCounter() {
  const [count, setCount] = React.useState(0);
  return React.createElement(
    "section",
    null,
    React.createElement("h2", null, "Floorp authored React counter"),
    React.createElement(FloorpCounterLabel, { count }),
    React.createElement("button", {
      id: "react-increment",
      onClick: () => setCount((value) => value + 1),
    }, "Increment real React state"),
  );
}
const root = document.getElementById("react-root");
if (!root) throw Error("Authored React root is missing");
createRoot(root).render(
  React.createElement(FloorpCounter),
);
/** @param {CounterState} state @param {AuthoredAction} action */
const reducer = (state = { count: 0 }, action) =>
  action.type === "AUTHORED_INCREMENT" ? { count: state.count + 1 } : state;
const reduxGlobal = browserGlobal;
const enhancer = reduxGlobal.__REDUX_DEVTOOLS_EXTENSION__?.({
  name: "Floorp authored Redux store",
});
const store = createStore(reducer, enhancer);
const counter = document.getElementById("redux-count");
const button = document.getElementById("redux-increment");
if (!counter || !button) throw Error("Authored Redux controls are missing");
const render = () =>
  counter.textContent = `Redux count: ${store.getState().count}`;
store.subscribe(render);
render();
button.addEventListener(
  "click",
  () => store.dispatch({ type: "AUTHORED_INCREMENT" }),
);
