/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test support for event propagation stop in the
// CanvasFrameAnonymousContentHelper event handling mechanism.

// This makes sure the 'domnode' protocol actor type is known when importing
// highlighter.
require("devtools/server/actors/inspector");

const {HighlighterEnvironment} = require("devtools/server/actors/highlighters");

const {
  CanvasFrameAnonymousContentHelper
} = require("devtools/server/actors/highlighters/utils/markup");

const TEST_URL = "data:text/html;charset=utf-8,CanvasFrameAnonymousContentHelper test";

add_task(function*() {
  let doc = yield addTab(TEST_URL);

  let nodeBuilder = () => {
    let root = doc.createElement("div");

    let parent = doc.createElement("div");
    parent.style = "pointer-events:auto;width:300px;height:300px;background:yellow;";
    parent.id = "parent-element";
    root.appendChild(parent);

    let child = doc.createElement("div");
    child.style = "pointer-events:auto;width:200px;height:200px;background:red;";
    child.id = "child-element";
    parent.appendChild(child);

    return root;
  };

  info("Building the helper");
  let env = new HighlighterEnvironment();
  env.initFromWindow(doc.defaultView);
  let helper = new CanvasFrameAnonymousContentHelper(env, nodeBuilder);

  info("Getting the parent and child elements");
  let parentEl = helper.getElement("parent-element");
  let childEl = helper.getElement("child-element");

  info("Adding an event listener on both elements");
  let mouseDownHandled = [];

  function onParentMouseDown(e, id) {
    mouseDownHandled.push(id);
  }
  parentEl.addEventListener("mousedown", onParentMouseDown);

  function onChildMouseDown(e, id) {
    mouseDownHandled.push(id);
    e.stopPropagation();
  }
  childEl.addEventListener("mousedown", onChildMouseDown);

  info("Synthesizing an event on the child element");
  let onDocMouseDown = once(doc, "mousedown");
  synthesizeMouseDown(100, 100, doc.defaultView);
  yield onDocMouseDown;

  is(mouseDownHandled.length, 1, "The mousedown event was handled only once");
  is(mouseDownHandled[0], "child-element",
    "The mousedown event was handled on the child element");

  info("Synthesizing an event on the parent, outside of the child element");
  mouseDownHandled = [];
  onDocMouseDown = once(doc, "mousedown");
  synthesizeMouseDown(250, 250, doc.defaultView);
  yield onDocMouseDown;

  is(mouseDownHandled.length, 1, "The mousedown event was handled only once");
  is(mouseDownHandled[0], "parent-element",
    "The mousedown event was handled on the parent element");

  info("Removing the event listener");
  parentEl.removeEventListener("mousedown", onParentMouseDown);
  childEl.removeEventListener("mousedown", onChildMouseDown);

  env.destroy();
  helper.destroy();

  gBrowser.removeCurrentTab();
});

function synthesizeMouseDown(x, y, win) {
  // We need to make sure the inserted anonymous content can be targeted by the
  // event right after having been inserted, and so we need to force a sync
  // reflow.
  let forceReflow = win.document.documentElement.offsetWidth;
  EventUtils.synthesizeMouseAtPoint(x, y, {type: "mousedown"}, win);
}
