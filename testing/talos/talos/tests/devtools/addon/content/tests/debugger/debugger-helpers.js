/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { openToolboxAndLog, reloadPageAndLog } = require("../head");
const InspectorUtils = require("InspectorUtils");

/*
 * These methods are used for working with debugger state changes in order
 * to make it easier to manipulate the ui and test different behavior. These
 * methods roughly reflect those found in debugger/test/mochi/head.js with
 * a few exceptions. The `dbg` object is not exactly the same, and the methods
 * have been simplified. We may want to consider unifying them in the future
 */

const DEBUGGER_POLLING_INTERVAL = 50;

function waitForState(dbg, predicate, msg) {
  return new Promise(resolve => {
    if (msg) {
      dump(`Waiting for state change: ${msg}\n`);
    }
    if (predicate(dbg.store.getState())) {
      if (msg) {
        dump(`Finished waiting for state change: ${msg}\n`);
      }
      return resolve();
    }

    const unsubscribe = dbg.store.subscribe(() => {
      if (predicate(dbg.store.getState())) {
        if (msg) {
          dump(`Finished waiting for state change: ${msg}\n`);
        }
        unsubscribe();
        resolve();
      }
    });
    return false;
  });
}

function waitForDispatch(dbg, type) {
  return new Promise(resolve => {
    dbg.store.dispatch({
      type: "@@service/waitUntil",
      predicate: action => {
        if (action.type === type) {
          return action.status
            ? action.status === "done" || action.status === "error"
            : true;
        }
        return false;
      },
      run: (dispatch, getState, action) => {
        resolve(action);
      },
    });
  });
}

async function waitUntil(predicate, msg) {
  if (msg) {
    dump(`Waiting until: ${msg}\n`);
  }
  return new Promise(resolve => {
    const timer = setInterval(() => {
      if (predicate()) {
        clearInterval(timer);
        if (msg) {
          dump(`Finished Waiting until: ${msg}\n`);
        }
        resolve();
      }
    }, DEBUGGER_POLLING_INTERVAL);
  });
}

function findSource(dbg, url) {
  const sources = dbg.selectors.getSourceList(dbg.getState());
  return sources.find(s => (s.url || "").includes(url));
}

function getCM(dbg) {
  const el = dbg.win.document.querySelector(".CodeMirror");
  return el.CodeMirror;
}

function waitForText(dbg, url, text) {
  return waitUntil(() => {
    // the welcome box is removed once text is displayed
    const welcomebox = dbg.win.document.querySelector(".welcomebox");
    if (welcomebox) {
      return false;
    }
    const cm = getCM(dbg);
    const editorText = cm.doc.getValue();
    return editorText.includes(text);
  }, "text is visible");
}

function waitForSymbols(dbg) {
  return waitUntil(() => {
    const state = dbg.store.getState();
    const source = dbg.selectors.getSelectedSource(state);
    return dbg.selectors.getSymbols(state, source);
  }, "has file metadata");
}

function waitForSources(dbg, expectedSources) {
  const { selectors } = dbg;
  function countSources(state) {
    return selectors.getSourceCount(state) >= expectedSources;
  }
  return waitForState(dbg, countSources, "count sources");
}

function waitForSource(dbg, sourceURL) {
  const { selectors } = dbg;
  function hasSource(state) {
    return selectors.getSourceByURL(state, sourceURL);
  }
  return waitForState(dbg, hasSource, `has source ${sourceURL}`);
}

async function waitForPaused(dbg) {
  const onLoadedScope = waitForLoadedScopes(dbg);
  const {
    selectors: { getSelectedScope, getIsPaused, getCurrentThread },
  } = dbg;
  const onStateChange = waitForState(dbg, state => {
    const thread = getCurrentThread(state);
    return getSelectedScope(state, thread) && getIsPaused(state, thread);
  });
  return Promise.all([onLoadedScope, onStateChange]);
}

async function waitForResumed(dbg) {
  const {
    selectors: { getIsPaused, getCurrentThread },
  } = dbg;
  return waitForState(
    dbg,
    state => !getIsPaused(state, getCurrentThread(state))
  );
}

async function waitForElement(dbg, name) {
  await waitUntil(() => dbg.win.document.querySelector(name));
  return dbg.win.document.querySelector(name);
}

async function waitForLoadedScopes(dbg) {
  const element = '.scopes-list .tree-node[aria-level="1"]';
  return waitForElement(dbg, element);
}

function createContext(panel) {
  const { store, selectors, actions } = panel.getVarsForTests();

  return {
    actions,
    selectors,
    getState: store.getState,
    win: panel.panelWin,
    store,
  };
}
exports.createContext = createContext;

function selectSource(dbg, url) {
  dump(`Selecting source: ${url}\n`);
  const line = 1;
  const source = findSource(dbg, url);
  const cx = dbg.selectors.getContext(dbg.getState());
  dbg.actions.selectLocation(cx, { sourceId: source.id, line });
  return waitForState(
    dbg,
    state => {
      const source = dbg.selectors.getSelectedSource(state);
      if (!source) {
        return false;
      }
      const sourceTextContent = dbg.selectors.getSelectedSourceTextContent(
        state
      );
      if (!sourceTextContent) {
        return false;
      }

      // wait for symbols -- a flat map of all named variables in a file -- to be calculated.
      // this is a slow process and becomes slower the larger the file is
      return dbg.selectors.getSymbols(state, source);
    },
    "selected source"
  );
}

function evalInContent(dbg, tab, testFunction) {
  dump(`Run function in content process: ${testFunction}\n`);
  // Load a frame script using a data URI so we can run a script
  // inside of the content process and trigger debugger functionality
  // as needed
  const messageManager = tab.linkedBrowser.messageManager;
  return messageManager.loadFrameScript(
    "data:,(" +
      encodeURIComponent(`
        function () {
          const iframe = content.document.querySelector("iframe");
          const win = iframe.contentWindow;
          win.eval("${testFunction}");
        }`) +
      ")()",
    true
  );
}

async function openDebuggerAndLog(label, expected) {
  const onLoad = async (toolbox, panel) => {
    const dbg = await createContext(panel);
    await waitForSource(dbg, expected.sourceURL);
    await selectSource(dbg, expected.file);
    await waitForText(dbg, expected.file, expected.text);
    await waitForSymbols(dbg);
  };

  const toolbox = await openToolboxAndLog(
    label + ".jsdebugger",
    "jsdebugger",
    onLoad
  );
  return toolbox;
}
exports.openDebuggerAndLog = openDebuggerAndLog;

async function reloadDebuggerAndLog(label, toolbox, expected) {
  const onReload = async () => {
    const panel = await toolbox.getPanelWhenReady("jsdebugger");
    const dbg = await createContext(panel);
    await waitForDispatch(dbg, "NAVIGATE");
    await waitForSources(dbg, expected.sources);
    await waitForText(dbg, expected.file, expected.text);
    await waitForSymbols(dbg);
  };
  await reloadPageAndLog(`${label}.jsdebugger`, toolbox, onReload);
}
exports.reloadDebuggerAndLog = reloadDebuggerAndLog;

async function addBreakpoint(dbg, line, url) {
  dump(`add breakpoint\n`);
  const source = findSource(dbg, url);
  const location = {
    sourceId: source.id,
    line,
  };

  await selectSource(dbg, url);

  const cx = dbg.selectors.getContext(dbg.getState());
  await dbg.actions.addBreakpoint(cx, location);
}
exports.addBreakpoint = addBreakpoint;

async function removeBreakpoints(dbg, line, url) {
  dump(`remove all breakpoints\n`);
  const breakpoints = dbg.selectors.getBreakpointsList(dbg.getState());

  const onBreakpointsCleared = waitForState(
    dbg,
    state => dbg.selectors.getBreakpointCount(state) === 0
  );
  const cx = dbg.selectors.getContext(dbg.getState());
  await dbg.actions.removeBreakpoints(cx, breakpoints);
  return onBreakpointsCleared;
}
exports.removeBreakpoints = removeBreakpoints;

async function pauseDebugger(dbg, tab, testFunction, { line, file }) {
  await addBreakpoint(dbg, line, file);
  const onPaused = waitForPaused(dbg);
  await evalInContent(dbg, tab, testFunction);
  return onPaused;
}
exports.pauseDebugger = pauseDebugger;

async function resume(dbg) {
  const onResumed = waitForResumed(dbg);
  const cx = dbg.selectors.getThreadContext(dbg.getState());
  dbg.actions.resume(cx);
  return onResumed;
}
exports.resume = resume;

async function step(dbg, stepType) {
  const resumed = waitForResumed(dbg);
  const cx = dbg.selectors.getThreadContext(dbg.getState());
  dbg.actions[stepType](cx);
  await resumed;
  return waitForPaused(dbg);
}
exports.step = step;

async function hoverOnToken(dbg, cx, textToWaitFor, textToHover) {
  await waitForText(dbg, null, textToWaitFor);
  const tokenElement = [
    ...dbg.win.document.querySelectorAll(".CodeMirror span"),
  ].find(el => el.textContent === "window");

  // Set the :hover state on the token Element, otherwise the preview popup
  // will not be displayed.
  InspectorUtils.addPseudoClassLock(tokenElement, ":hover", true);

  const mouseOverEvent = new dbg.win.MouseEvent("mouseover", {
    bubbles: true,
    cancelable: true,
    view: dbg.win,
  });
  tokenElement.dispatchEvent(mouseOverEvent);

  const setPreviewDispatch = waitForDispatch(dbg, "SET_PREVIEW");
  const tokenPosition = { line: 21, column: 3 };
  dbg.actions.updatePreview(cx, tokenElement, tokenPosition, getCM(dbg));
  await setPreviewDispatch;

  // Remove the :hover state.
  InspectorUtils.removePseudoClassLock(tokenElement, ":hover");
}
exports.hoverOnToken = hoverOnToken;
