/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { action, CLICK_INTERVAL, ClickTracker } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/Actions.sys.mjs"
);

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

const XHTMLNS = "http://www.w3.org/1999/xhtml";

const domEl = {
  nodeType: 1,
  ELEMENT_NODE: 1,
  namespaceURI: XHTMLNS,
};

add_task(function test_createInputState() {
  for (let type of ["none", "key", "pointer" /*"wheel"*/]) {
    const state = new action.State();
    const id = "device";
    const actionSequence = {
      type,
      id,
      actions: [],
    };
    action.Chain.fromJSON(state, [actionSequence]);
    equal(state.inputStateMap.size, 1);
    equal(state.inputStateMap.get(id).constructor.type, type);
  }
});

add_task(function test_defaultPointerParameters() {
  let state = new action.State();
  const inputTickActions = [
    { type: "pointer", subtype: "pointerDown", button: 0 },
  ];
  const chain = action.Chain.fromJSON(state, chainForTick(inputTickActions));
  const pointerAction = chain[0][0];
  equal(
    state.getInputSource(pointerAction.id).pointer.constructor.type,
    "mouse"
  );
});

add_task(function test_processPointerParameters() {
  for (let subtype of ["pointerDown", "pointerUp"]) {
    for (let pointerType of [2, true, {}, []]) {
      const inputTickActions = [
        {
          type: "pointer",
          parameters: { pointerType },
          subtype,
          button: 0,
        },
      ];
      let message = `Action sequence with parameters: {pointerType: ${pointerType} subtype: ${subtype}}`;
      checkFromJSONErrors(
        inputTickActions,
        /Expected "pointerType" to be a string/,
        message
      );
    }

    for (let pointerType of ["", "foo"]) {
      const inputTickActions = [
        {
          type: "pointer",
          parameters: { pointerType },
          subtype,
          button: 0,
        },
      ];
      let message = `Action sequence with parameters: {pointerType: ${pointerType} subtype: ${subtype}}`;
      checkFromJSONErrors(
        inputTickActions,
        /Expected "pointerType" to be one of/,
        message
      );
    }
  }

  for (let pointerType of ["mouse" /*"touch"*/]) {
    let state = new action.State();
    const inputTickActions = [
      {
        type: "pointer",
        parameters: { pointerType },
        subtype: "pointerDown",
        button: 0,
      },
    ];
    const chain = action.Chain.fromJSON(state, chainForTick(inputTickActions));
    const pointerAction = chain[0][0];
    equal(
      state.getInputSource(pointerAction.id).pointer.constructor.type,
      pointerType
    );
  }
});

add_task(function test_processPointerDownAction() {
  for (let button of [-1, "a"]) {
    const inputTickActions = [
      { type: "pointer", subtype: "pointerDown", button },
    ];
    checkFromJSONErrors(
      inputTickActions,
      /Expected "button" to be a positive integer/,
      `pointerDown with {button: ${button}}`
    );
  }
  let state = new action.State();
  const inputTickActions = [
    { type: "pointer", subtype: "pointerDown", button: 5 },
  ];
  const chain = action.Chain.fromJSON(state, chainForTick(inputTickActions));
  equal(chain[0][0].button, 5);
});

add_task(function test_validateActionDurationAndCoordinates() {
  for (let [type, subtype] of [
    ["none", "pause"],
    ["pointer", "pointerMove"],
  ]) {
    for (let duration of [-1, "a"]) {
      const inputTickActions = [{ type, subtype, duration }];
      checkFromJSONErrors(
        inputTickActions,
        /Expected "duration" to be a positive integer/,
        `{subtype} with {duration: ${duration}}`
      );
    }
  }
  for (let name of ["x", "y"]) {
    const actionItem = {
      type: "pointer",
      subtype: "pointerMove",
      duration: 5000,
    };
    actionItem[name] = "a";
    checkFromJSONErrors(
      [actionItem],
      /Expected ".*" to be an integer/,
      `${name}: "a", subtype: pointerMove`
    );
  }
});

add_task(function test_processPointerMoveActionOriginValidation() {
  for (let origin of [-1, { a: "blah" }, []]) {
    const inputTickActions = [
      { type: "pointer", duration: 5000, subtype: "pointerMove", origin },
    ];
    checkFromJSONErrors(
      inputTickActions,
      /Expected "origin" to be undefined, "viewport", "pointer", or an element/,
      `actionItem.origin: (${getTypeString(origin)})`
    );
  }
});

add_task(function test_processPointerMoveActionOriginStringValidation() {
  for (let origin of ["", "viewports", "pointers"]) {
    const inputTickActions = [
      { type: "pointer", duration: 5000, subtype: "pointerMove", origin },
    ];
    checkFromJSONErrors(
      inputTickActions,
      /Expected "origin" to be undefined, "viewport", "pointer", or an element/,
      `actionItem.origin: ${origin}`
    );
  }
});

add_task(function test_processPointerMoveActionElementOrigin() {
  let state = new action.State();
  const inputTickActions = [
    {
      type: "pointer",
      duration: 5000,
      subtype: "pointerMove",
      origin: domEl,
      x: 0,
      y: 0,
    },
  ];
  const chain = action.Chain.fromJSON(state, chainForTick(inputTickActions));
  deepEqual(chain[0][0].origin.element, domEl);
});

add_task(function test_processPointerMoveActionDefaultOrigin() {
  let state = new action.State();
  const inputTickActions = [
    { type: "pointer", duration: 5000, subtype: "pointerMove", x: 0, y: 0 },
  ];
  const chain = action.Chain.fromJSON(state, chainForTick(inputTickActions));
  // The default is viewport coordinates which have an origin at [0,0] and don't depend on inputSource
  deepEqual(chain[0][0].origin.getOriginCoordinates(null, null), {
    x: 0,
    y: 0,
  });
});

add_task(function test_processPointerMoveAction() {
  let state = new action.State();
  const actionItems = [
    {
      duration: 5000,
      type: "pointerMove",
      origin: undefined,
      x: 0,
      y: 0,
    },
    {
      duration: undefined,
      type: "pointerMove",
      origin: domEl,
      x: 0,
      y: 0,
    },
    {
      duration: 5000,
      type: "pointerMove",
      x: 1,
      y: 2,
      origin: undefined,
    },
  ];
  const actionSequence = {
    id: "some_id",
    type: "pointer",
    actions: actionItems,
  };
  let chain = action.Chain.fromJSON(state, [actionSequence]);
  equal(chain.length, actionItems.length);
  for (let i = 0; i < actionItems.length; i++) {
    let actual = chain[i][0];
    let expected = actionItems[i];
    equal(actual.duration, expected.duration);
    equal(actual.x, expected.x);
    equal(actual.y, expected.y);

    let originClass;
    if (expected.origin === undefined || expected.origin == "viewport") {
      originClass = "ViewportOrigin";
    } else if (expected.origin === "pointer") {
      originClass = "PointerOrigin";
    } else {
      originClass = "ElementOrigin";
    }
    deepEqual(actual.origin.constructor.name, originClass);
  }
});

add_task(function test_computePointerDestinationViewport() {
  const state = new action.State();
  const inputTickActions = [
    {
      type: "pointer",
      subtype: "pointerMove",
      x: 100,
      y: 200,
      origin: "viewport",
    },
  ];
  const chain = action.Chain.fromJSON(state, chainForTick(inputTickActions));
  const actionItem = chain[0][0];
  const inputSource = state.getInputSource(actionItem.id);
  // these values should not affect the outcome
  inputSource.x = "99";
  inputSource.y = "10";
  const target = actionItem.origin.getTargetCoordinates(
    inputSource,
    [actionItem.x, actionItem.y],
    null
  );
  equal(actionItem.x, target[0]);
  equal(actionItem.y, target[1]);
});

add_task(function test_computePointerDestinationPointer() {
  const state = new action.State();
  const inputTickActions = [
    {
      type: "pointer",
      subtype: "pointerMove",
      x: 100,
      y: 200,
      origin: "pointer",
    },
  ];
  const chain = action.Chain.fromJSON(state, chainForTick(inputTickActions));
  const actionItem = chain[0][0];
  const inputSource = state.getInputSource(actionItem.id);
  inputSource.x = 10;
  inputSource.y = 99;
  const target = actionItem.origin.getTargetCoordinates(
    inputSource,
    [actionItem.x, actionItem.y],
    null
  );
  equal(actionItem.x + inputSource.x, target[0]);
  equal(actionItem.y + inputSource.y, target[1]);
});

add_task(function test_processPointerAction() {
  for (let pointerType of ["mouse", "touch"]) {
    const actionItems = [
      {
        duration: 2000,
        type: "pause",
      },
      {
        type: "pointerMove",
        duration: 2000,
        x: 0,
        y: 0,
      },
      {
        type: "pointerUp",
        button: 1,
      },
    ];
    let actionSequence = {
      type: "pointer",
      id: "some_id",
      parameters: {
        pointerType,
      },
      actions: actionItems,
    };
    const state = new action.State();
    const chain = action.Chain.fromJSON(state, [actionSequence]);
    equal(chain.length, actionItems.length);
    for (let i = 0; i < actionItems.length; i++) {
      const actual = chain[i][0];
      const expected = actionItems[i];
      equal(actual.type, expected.type === "pause" ? "none" : "pointer");
      equal(actual.subtype, expected.type);
      equal(actual.id, actionSequence.id);
      if (expected.type === "pointerUp") {
        equal(actual.button, expected.button);
      } else {
        equal(actual.duration, expected.duration);
      }
      if (expected.type !== "pause") {
        equal(
          state.getInputSource(actual.id).pointer.constructor.type,
          pointerType
        );
      }
    }
  }
});

add_task(function test_processPauseAction() {
  for (let type of ["none", "key", "pointer"]) {
    const state = new action.State();
    const actionSequence = {
      type,
      id: "some_id",
      actions: [{ type: "pause", duration: 5000 }],
    };
    const actionItem = action.Chain.fromJSON(state, [actionSequence])[0][0];
    equal(actionItem.type, "none");
    equal(actionItem.subtype, "pause");
    equal(actionItem.id, "some_id");
    equal(actionItem.duration, 5000);
  }
  const state = new action.State();
  const actionSequence = {
    type: "none",
    id: "some_id",
    actions: [{ type: "pause" }],
  };
  const actionItem = action.Chain.fromJSON(state, [actionSequence])[0][0];
  equal(actionItem.duration, undefined);
});

add_task(function test_processActionSubtypeValidation() {
  for (let type of ["none", "key", "pointer"]) {
    const message = `type: ${type}, subtype: dancing`;
    const inputTickActions = [{ type, subtype: "dancing" }];
    checkFromJSONErrors(
      inputTickActions,
      new RegExp(`Expected known subtype for type`),
      message
    );
  }
});

add_task(function test_processKeyActionDown() {
  for (let value of [-1, undefined, [], ["a"], { length: 1 }, null]) {
    const inputTickActions = [{ type: "key", subtype: "keyDown", value }];
    const message = `actionItem.value: (${getTypeString(value)})`;
    checkFromJSONErrors(
      inputTickActions,
      /Expected "value" to be a string that represents single code point/,
      message
    );
  }

  const state = new action.State();
  const actionSequence = {
    type: "key",
    id: "keyboard",
    actions: [{ type: "keyDown", value: "\uE004" }],
  };
  const actionItem = action.Chain.fromJSON(state, [actionSequence])[0][0];

  equal(actionItem.type, "key");
  equal(actionItem.id, "keyboard");
  equal(actionItem.subtype, "keyDown");
  equal(actionItem.value, "\ue004");
});

add_task(function test_processInputSourceActionSequenceValidation() {
  checkFromJSONErrors(
    [{ type: "swim", subtype: "pause", id: "some id" }],
    /Expected known action type/,
    "actionSequence type: swim"
  );

  checkFromJSONErrors(
    [{ type: "none", subtype: "pause", id: -1 }],
    /Expected "id" to be a string/,
    "actionSequence id: -1"
  );

  checkFromJSONErrors(
    [{ type: "none", subtype: "pause", id: undefined }],
    /Expected "id" to be a string/,
    "actionSequence id: undefined"
  );

  const state = new action.State();
  const actionSequence = [
    { type: "none", subtype: "pause", id: "some_id", actions: -1 },
  ];
  const errorRegex = /Expected "actionSequence.actions" to be an array/;
  const message = "actionSequence actions: -1";

  Assert.throws(
    () => action.Chain.fromJSON(state, actionSequence),
    /InvalidArgumentError/,
    message
  );
  Assert.throws(
    () => action.Chain.fromJSON(state, actionSequence),
    errorRegex,
    message
  );
});

add_task(function test_processInputSourceActionSequence() {
  const state = new action.State();
  const actionItem = { type: "pause", duration: 5 };
  const actionSequence = {
    type: "none",
    id: "some id",
    actions: [actionItem],
  };
  const chain = action.Chain.fromJSON(state, [actionSequence]);
  equal(chain.length, 1);
  const tickActions = chain[0];
  equal(tickActions.length, 1);
  equal(tickActions[0].type, "none");
  equal(tickActions[0].subtype, "pause");
  equal(tickActions[0].duration, 5);
  equal(tickActions[0].id, "some id");
});

add_task(function test_processInputSourceActionSequencePointer() {
  const state = new action.State();
  const actionItem = { type: "pointerDown", button: 1 };
  const actionSequence = {
    type: "pointer",
    id: "9",
    actions: [actionItem],
    parameters: {
      pointerType: "mouse", // TODO "pen"
    },
  };
  const chain = action.Chain.fromJSON(state, [actionSequence]);
  equal(chain.length, 1);
  const tickActions = chain[0];
  equal(tickActions.length, 1);
  equal(tickActions[0].type, "pointer");
  equal(tickActions[0].subtype, "pointerDown");
  equal(tickActions[0].button, 1);
  equal(tickActions[0].id, "9");
  const inputSource = state.getInputSource(tickActions[0].id);
  equal(inputSource.constructor.type, "pointer");
  equal(inputSource.pointer.constructor.type, "mouse");
});

add_task(function test_processInputSourceActionSequenceKey() {
  const state = new action.State();
  const actionItem = { type: "keyUp", value: "a" };
  const actionSequence = {
    type: "key",
    id: "9",
    actions: [actionItem],
  };
  const chain = action.Chain.fromJSON(state, [actionSequence]);
  equal(chain.length, 1);
  const tickActions = chain[0];
  equal(tickActions.length, 1);
  equal(tickActions[0].type, "key");
  equal(tickActions[0].subtype, "keyUp");
  equal(tickActions[0].value, "a");
  equal(tickActions[0].id, "9");
});

add_task(function test_processInputSourceActionSequenceInputStateMap() {
  const state = new action.State();
  const id = "1";
  const actionItem = { type: "pause", duration: 5000 };
  const actionSequence = {
    type: "key",
    id,
    actions: [actionItem],
  };
  action.Chain.fromJSON(state, [actionSequence]);
  equal(state.inputStateMap.size, 1);
  equal(state.inputStateMap.get(id).constructor.type, "key");

  // Construct a different state with the same input id
  const state1 = new action.State();
  const actionItem1 = { type: "pointerDown", button: 0 };
  const actionSequence1 = {
    type: "pointer",
    id,
    actions: [actionItem1],
  };
  action.Chain.fromJSON(state1, [actionSequence1]);
  equal(state1.inputStateMap.size, 1);

  // Overwrite the state in the initial map with one of a different type
  state.inputStateMap.set(id, state1.inputStateMap.get(id));
  equal(state.inputStateMap.get(id).constructor.type, "pointer");

  const message = "Wrong state for input id type";
  Assert.throws(
    () => action.Chain.fromJSON(state, [actionSequence]),
    /InvalidArgumentError/,
    message
  );
  Assert.throws(
    () => action.Chain.fromJSON(state, [actionSequence]),
    /Expected input source \[object String\] "1" to be type pointer/,
    message
  );
});

add_task(function test_extractActionChainValidation() {
  for (let actions of [-1, "a", undefined, null]) {
    const state = new action.State();
    let message = `actions: ${getTypeString(actions)}`;
    Assert.throws(
      () => action.Chain.fromJSON(state, actions),
      /InvalidArgumentError/,
      message
    );
    Assert.throws(
      () => action.Chain.fromJSON(state, actions),
      /Expected "actions" to be an array/,
      message
    );
  }
});

add_task(function test_extractActionChainEmpty() {
  const state = new action.State();
  deepEqual(action.Chain.fromJSON(state, []), []);
});

add_task(function test_extractActionChain_oneTickOneInput() {
  const state = new action.State();
  const actionItem = { type: "pause", duration: 5000 };
  const actionSequence = {
    type: "none",
    id: "some id",
    actions: [actionItem],
  };
  const actionsByTick = action.Chain.fromJSON(state, [actionSequence]);
  equal(1, actionsByTick.length);
  equal(1, actionsByTick[0].length);
  equal(actionsByTick[0][0].id, actionSequence.id);
  equal(actionsByTick[0][0].type, "none");
  equal(actionsByTick[0][0].subtype, "pause");
  equal(actionsByTick[0][0].duration, actionItem.duration);
});

add_task(function test_extractActionChain_twoAndThreeTicks() {
  const state = new action.State();
  const mouseActionItems = [
    {
      type: "pointerDown",
      button: 2,
    },
    {
      type: "pointerUp",
      button: 2,
    },
  ];
  const mouseActionSequence = {
    type: "pointer",
    id: "7",
    actions: mouseActionItems,
    parameters: {
      pointerType: "mouse",
    },
  };
  const keyActionItems = [
    {
      type: "keyDown",
      value: "a",
    },
    {
      type: "pause",
      duration: 4,
    },
    {
      type: "keyUp",
      value: "a",
    },
  ];
  let keyActionSequence = {
    type: "key",
    id: "1",
    actions: keyActionItems,
  };
  let actionsByTick = action.Chain.fromJSON(state, [
    keyActionSequence,
    mouseActionSequence,
  ]);
  // number of ticks is same as longest action sequence
  equal(keyActionItems.length, actionsByTick.length);
  equal(2, actionsByTick[0].length);
  equal(2, actionsByTick[1].length);
  equal(1, actionsByTick[2].length);

  equal(actionsByTick[2][0].id, keyActionSequence.id);
  equal(actionsByTick[2][0].type, "key");
  equal(actionsByTick[2][0].subtype, "keyUp");
});

add_task(function test_computeTickDuration() {
  const state = new action.State();
  const expected = 8000;
  const inputTickActions = [
    { type: "none", subtype: "pause", duration: 5000 },
    { type: "key", subtype: "pause", duration: 1000 },
    { type: "pointer", subtype: "pointerMove", duration: 6000, x: 0, y: 0 },
    // invalid because keyDown should not have duration, so duration should be ignored.
    { type: "key", subtype: "keyDown", duration: 100000, value: "a" },
    { type: "pointer", subtype: "pause", duration: expected },
    { type: "pointer", subtype: "pointerUp", button: 0 },
  ];
  const chain = action.Chain.fromJSON(state, chainForTick(inputTickActions));
  equal(1, chain.length);
  const tickActions = chain[0];
  equal(expected, tickActions.getDuration());
});

add_task(function test_computeTickDuration_noDurations() {
  const state = new action.State();
  const inputTickActions = [
    // invalid because keyDown should not have duration, so duration should be ignored.
    { type: "key", subtype: "keyDown", duration: 100000, value: "a" },
    // undefined duration permitted
    { type: "none", subtype: "pause" },
    { type: "pointer", subtype: "pointerMove", button: 0, x: 0, y: 0 },
    { type: "pointer", subtype: "pointerDown", button: 0 },
    { type: "key", subtype: "keyUp", value: "a" },
  ];
  const chain = action.Chain.fromJSON(state, chainForTick(inputTickActions));
  equal(0, chain[0].getDuration());
});

add_task(function test_ClickTracker_setClick() {
  const clickTracker = new ClickTracker();
  const button1 = 1;
  const button2 = 2;

  clickTracker.setClick(button1);
  equal(1, clickTracker.count);

  // Make sure that clicking different mouse buttons doesn't increase the count.
  clickTracker.setClick(button2);
  equal(1, clickTracker.count);

  clickTracker.setClick(button2);
  equal(2, clickTracker.count);

  clickTracker.reset();
  equal(0, clickTracker.count);
});

add_task(function test_ClickTracker_reset_after_timeout() {
  const clickTracker = new ClickTracker();

  clickTracker.setClick(1);
  equal(1, clickTracker.count);

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  setTimeout(() => equal(0, clickTracker.count), CLICK_INTERVAL + 10);
});

// helpers
function getTypeString(obj) {
  return Object.prototype.toString.call(obj);
}

function checkFromJSONErrors(inputTickActions, regex, message) {
  const state = new action.State();

  if (typeof message == "undefined") {
    message = `fromJSON`;
  }
  Assert.throws(
    () => action.Chain.fromJSON(state, chainForTick(inputTickActions)),
    /InvalidArgumentError/,
    message
  );
  Assert.throws(
    () => action.Chain.fromJSON(state, chainForTick(inputTickActions)),
    regex,
    message
  );
}

function chainForTick(tickActions) {
  const actions = [];
  let lastId = 0;
  for (let { type, subtype, parameters, ...props } of tickActions) {
    let id;
    if (!props.hasOwnProperty("id")) {
      id = `${type}_${lastId++}`;
    } else {
      id = props.id;
      delete props.id;
    }
    const inputAction = { type, id, actions: [{ type: subtype, ...props }] };
    if (parameters !== undefined) {
      inputAction.parameters = parameters;
    }
    actions.push(inputAction);
  }
  return actions;
}
