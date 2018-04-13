/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("chrome://marionette/content/action.js");
const {InvalidArgumentError} = ChromeUtils.import("chrome://marionette/content/error.js", {});

const XHTMLNS = "http://www.w3.org/1999/xhtml";

const domEl = {
  nodeType: 1,
  ELEMENT_NODE: 1,
  namespaceURI: XHTMLNS,
};

action.inputStateMap = new Map();

add_test(function test_createAction() {
  Assert.throws(() => new action.Action(), InvalidArgumentError,
      "Missing Action constructor args");
  Assert.throws(() => new action.Action(1, 2), InvalidArgumentError,
      "Missing Action constructor args");
  Assert.throws(
      () => new action.Action(1, 2, "sometype"), /Expected string/, "Non-string arguments.");
  ok(new action.Action("id", "sometype", "sometype"));

  run_next_test();
});

add_test(function test_defaultPointerParameters() {
  let defaultParameters = {pointerType: action.PointerType.Mouse};
  deepEqual(action.PointerParameters.fromJSON(), defaultParameters);

  run_next_test();
});

add_test(function test_processPointerParameters() {
  let check = (regex, message, arg) => checkErrors(
      regex, action.PointerParameters.fromJSON, [arg], message);
  let parametersData;
  for (let d of ["foo", "", "get", "Get"]) {
    parametersData = {pointerType: d};
    let message = `parametersData: [pointerType: ${parametersData.pointerType}]`;
    check(/Unknown pointerType/, message, parametersData);
  }
  parametersData.pointerType = "mouse"; // TODO "pen";
  deepEqual(action.PointerParameters.fromJSON(parametersData),
      {pointerType: "mouse"}); // TODO action.PointerType.Pen});

  run_next_test();
});

add_test(function test_processPointerUpDownAction() {
  let actionItem = {type: "pointerDown"};
  let actionSequence = {type: "pointer", id: "some_id"};
  for (let d of [-1, "a"]) {
    actionItem.button = d;
    checkErrors(
        /Expected 'button' \(.*\) to be >= 0/, action.Action.fromJSON, [actionSequence, actionItem],
        `button: ${actionItem.button}`);
  }
  actionItem.button = 5;
  let act = action.Action.fromJSON(actionSequence, actionItem);
  equal(act.button, actionItem.button);

  run_next_test();
});

add_test(function test_validateActionDurationAndCoordinates() {
  let actionItem = {};
  let actionSequence = {id: "some_id"};
  let check = function(type, subtype, message = undefined) {
    message = message || `duration: ${actionItem.duration}, subtype: ${subtype}`;
    actionItem.type = subtype;
    actionSequence.type = type;
    checkErrors(/Expected '.*' \(.*\) to be >= 0/,
        action.Action.fromJSON, [actionSequence, actionItem], message);
  };
  for (let d of [-1, "a"]) {
    actionItem.duration = d;
    check("none", "pause");
    check("pointer", "pointerMove");
  }
  actionItem.duration = 5000;
  for (let name of ["x", "y"]) {
    actionItem[name] = "a";
    actionItem.type = "pointerMove";
    actionSequence.type = "pointer";
    checkErrors(/Expected '.*' \(.*\) to be an Integer/,
        action.Action.fromJSON, [actionSequence, actionItem],
        `duration: ${actionItem.duration}, subtype: pointerMove`);
  }
  run_next_test();
});

add_test(function test_processPointerMoveActionOriginValidation() {
  let actionSequence = {type: "pointer", id: "some_id"};
  let actionItem = {duration: 5000, type: "pointerMove"};
  for (let d of [-1, {a: "blah"}, []]) {
    actionItem.origin = d;

    checkErrors(/Expected \'origin\' to be undefined, "viewport", "pointer", or an element/,
        action.Action.fromJSON,
        [actionSequence, actionItem],
        `actionItem.origin: (${getTypeString(d)})`);
  }

  run_next_test();
});

add_test(function test_processPointerMoveActionOriginStringValidation() {
  let actionSequence = {type: "pointer", id: "some_id"};
  let actionItem = {duration: 5000, type: "pointerMove"};
  for (let d of ["a", "", "get", "Get"]) {
    actionItem.origin = d;
    checkErrors(/Unknown pointer-move origin/,
        action.Action.fromJSON,
        [actionSequence, actionItem],
        `actionItem.origin: ${d}`);
  }

  run_next_test();
});

add_test(function test_processPointerMoveActionElementOrigin() {
  let actionSequence = {type: "pointer", id: "some_id"};
  let actionItem = {duration: 5000, type: "pointerMove"};
  actionItem.origin = domEl;
  let a = action.Action.fromJSON(actionSequence, actionItem);
  deepEqual(a.origin, actionItem.origin);
  run_next_test();
});

add_test(function test_processPointerMoveActionDefaultOrigin() {
  let actionSequence = {type: "pointer", id: "some_id"};
  // origin left undefined
  let actionItem = {duration: 5000, type: "pointerMove"};
  let a = action.Action.fromJSON(actionSequence, actionItem);
  deepEqual(a.origin, action.PointerOrigin.Viewport);
  run_next_test();
});

add_test(function test_processPointerMoveAction() {
  let actionSequence = {id: "some_id", type: "pointer"};
  let actionItems = [
    {
      duration: 5000,
      type: "pointerMove",
      origin: undefined,
      x: undefined,
      y: undefined,
    },
    {
      duration: undefined,
      type: "pointerMove",
      origin: domEl,
      x: undefined,
      y: undefined,
    },
    {
      duration: 5000,
      type: "pointerMove",
      x: 0,
      y: undefined,
      origin: undefined,
    },
    {
      duration: 5000,
      type: "pointerMove",
      x: 1,
      y: 2,
      origin: undefined,
    },
  ];
  for (let expected of actionItems) {
    let actual = action.Action.fromJSON(actionSequence, expected);
    ok(actual instanceof action.Action);
    equal(actual.duration, expected.duration);
    equal(actual.x, expected.x);
    equal(actual.y, expected.y);

    let origin = expected.origin;
    if (typeof origin == "undefined") {
      origin = action.PointerOrigin.Viewport;
    }
    deepEqual(actual.origin, origin);

  }
  run_next_test();
});

add_test(function test_computePointerDestinationViewport() {
  let act = {type: "pointerMove", x: 100, y: 200, origin: "viewport"};
  let inputState = new action.InputState.Pointer(action.PointerType.Mouse);
  // these values should not affect the outcome
  inputState.x = "99";
  inputState.y = "10";
  let target = action.computePointerDestination(act, inputState);
  equal(act.x, target.x);
  equal(act.y, target.y);

  run_next_test();
});

add_test(function test_computePointerDestinationPointer() {
  let act = {type: "pointerMove", x: 100, y: 200, origin: "pointer"};
  let inputState = new action.InputState.Pointer(action.PointerType.Mouse);
  inputState.x = 10;
  inputState.y = 99;
  let target = action.computePointerDestination(act, inputState);
  equal(act.x + inputState.x, target.x);
  equal(act.y + inputState.y, target.y);


  run_next_test();
});

add_test(function test_computePointerDestinationElement() {
  // origin represents a web element
  // using an object literal instead to test default case in computePointerDestination
  let act = {type: "pointerMove", x: 100, y: 200, origin: {}};
  let inputState = new action.InputState.Pointer(action.PointerType.Mouse);
  let elementCenter = {x: 10, y: 99};
  let target = action.computePointerDestination(act, inputState, elementCenter);
  equal(act.x + elementCenter.x, target.x);
  equal(act.y + elementCenter.y, target.y);

  Assert.throws(
      () => action.computePointerDestination(act, inputState, {a: 1}),
      InvalidArgumentError,
      "Invalid element center coordinates.");

  Assert.throws(
      () => action.computePointerDestination(act, inputState, undefined),
      InvalidArgumentError,
      "Undefined element center coordinates.");

  run_next_test();
});

add_test(function test_processPointerAction() {
  let actionSequence = {
    type: "pointer",
    id: "some_id",
    parameters: {
      pointerType: "mouse", // TODO "touch"
    },
  };
  let actionItems = [
    {
      duration: 2000,
      type: "pause",
    },
    {
      type: "pointerMove",
      duration: 2000,
    },
    {
      type: "pointerUp",
      button: 1,
    },
  ];
  for (let expected of actionItems) {
    let actual = action.Action.fromJSON(actionSequence, expected);
    equal(actual.type, actionSequence.type);
    equal(actual.subtype, expected.type);
    equal(actual.id, actionSequence.id);
    if (expected.type === "pointerUp") {
      equal(actual.button, expected.button);
    } else {
      equal(actual.duration, expected.duration);
    }
    if (expected.type !== "pause") {
      equal(actual.pointerType, actionSequence.parameters.pointerType);
    }
  }

  run_next_test();
});

add_test(function test_processPauseAction() {
  let actionItem = {type: "pause", duration: 5000};
  let actionSequence = {id: "some_id"};
  for (let type of ["none", "key", "pointer"]) {
    actionSequence.type = type;
    let act = action.Action.fromJSON(actionSequence, actionItem);
    ok(act instanceof action.Action);
    equal(act.type, type);
    equal(act.subtype, actionItem.type);
    equal(act.id, actionSequence.id);
    equal(act.duration, actionItem.duration);
  }
  actionItem.duration = undefined;
  let act = action.Action.fromJSON(actionSequence, actionItem);
  equal(act.duration, actionItem.duration);

  run_next_test();
});

add_test(function test_processActionSubtypeValidation() {
  let actionItem = {type: "dancing"};
  let actionSequence = {id: "some_id"};
  let check = function(regex) {
    let message = `type: ${actionSequence.type}, subtype: ${actionItem.type}`;
    checkErrors(regex, action.Action.fromJSON, [actionSequence, actionItem], message);
  };
  for (let type of ["none", "key", "pointer"]) {
    actionSequence.type = type;
    check(new RegExp(`Unknown subtype for ${type} action`));
  }
  run_next_test();
});

add_test(function test_processKeyActionUpDown() {
  let actionSequence = {type: "key", id: "some_id"};
  let actionItem = {type: "keyDown"};

  for (let v of [-1, undefined, [], ["a"], {length: 1}, null]) {
    actionItem.value = v;
    let message = `actionItem.value: (${getTypeString(v)})`;
    Assert.throws(() => action.Action.fromJSON(actionSequence, actionItem),
        InvalidArgumentError, message);
    Assert.throws(() => action.Action.fromJSON(actionSequence, actionItem),
        /Expected 'value' to be a string that represents single code point/, message);
  }

  actionItem.value = "\uE004";
  let act = action.Action.fromJSON(actionSequence, actionItem);
  ok(act instanceof action.Action);
  equal(act.type, actionSequence.type);
  equal(act.subtype, actionItem.type);
  equal(act.id, actionSequence.id);
  equal(act.value, actionItem.value);

  run_next_test();
});

add_test(function test_processInputSourceActionSequenceValidation() {
  let actionSequence = {type: "swim", id: "some id"};
  let check = (message, regex) => checkErrors(
      regex, action.Sequence.fromJSON, [actionSequence], message);
  check(`actionSequence.type: ${actionSequence.type}`, /Unknown action type/);
  action.inputStateMap.clear();

  actionSequence.type = "none";
  actionSequence.id = -1;
  check(`actionSequence.id: ${getTypeString(actionSequence.id)}`,
      /Expected 'id' to be a string/);
  action.inputStateMap.clear();

  actionSequence.id = undefined;
  check(`actionSequence.id: ${getTypeString(actionSequence.id)}`,
      /Expected 'id' to be defined/);
  action.inputStateMap.clear();

  actionSequence.id = "some_id";
  actionSequence.actions = -1;
  check(`actionSequence.actions: ${getTypeString(actionSequence.actions)}`,
      /Expected 'actionSequence.actions' to be an array/);
  action.inputStateMap.clear();

  run_next_test();
});

add_test(function test_processInputSourceActionSequence() {
  let actionItem = {type: "pause", duration: 5};
  let actionSequence = {
    type: "none",
    id: "some id",
    actions: [actionItem],
  };
  let expectedAction = new action.Action(actionSequence.id, "none", actionItem.type);
  expectedAction.duration = actionItem.duration;
  let actions = action.Sequence.fromJSON(actionSequence);
  equal(actions.length, 1);
  deepEqual(actions[0], expectedAction);
  action.inputStateMap.clear();
  run_next_test();
});

add_test(function test_processInputSourceActionSequencePointer() {
  let actionItem = {type: "pointerDown", button: 1};
  let actionSequence = {
    type: "pointer",
    id: "9",
    actions: [actionItem],
    parameters: {
      pointerType: "mouse", // TODO "pen"
    },
  };
  let expectedAction = new action.Action(
      actionSequence.id, actionSequence.type, actionItem.type);
  expectedAction.pointerType = actionSequence.parameters.pointerType;
  expectedAction.button = actionItem.button;
  let actions = action.Sequence.fromJSON(actionSequence);
  equal(actions.length, 1);
  deepEqual(actions[0], expectedAction);
  action.inputStateMap.clear();
  run_next_test();
});

add_test(function test_processInputSourceActionSequenceKey() {
  let actionItem = {type: "keyUp", value: "a"};
  let actionSequence = {
    type: "key",
    id: "9",
    actions: [actionItem],
  };
  let expectedAction = new action.Action(
      actionSequence.id, actionSequence.type, actionItem.type);
  expectedAction.value = actionItem.value;
  let actions = action.Sequence.fromJSON(actionSequence);
  equal(actions.length, 1);
  deepEqual(actions[0], expectedAction);
  action.inputStateMap.clear();
  run_next_test();
});


add_test(function test_processInputSourceActionSequenceInputStateMap() {
  let id = "1";
  let actionItem = {type: "pause", duration: 5000};
  let actionSequence = {
    type: "key",
    id,
    actions: [actionItem],
  };
  let wrongInputState = new action.InputState.Null();
  action.inputStateMap.set(actionSequence.id, wrongInputState);
  checkErrors(/to be mapped to/, action.Sequence.fromJSON, [actionSequence],
      `${actionSequence.type} using ${wrongInputState}`);
  action.inputStateMap.clear();
  let rightInputState = new action.InputState.Key();
  action.inputStateMap.set(id, rightInputState);
  let acts = action.Sequence.fromJSON(actionSequence);
  equal(acts.length, 1);
  action.inputStateMap.clear();
  run_next_test();
});

add_test(function test_processPointerActionInputStateMap() {
  let actionItem = {type: "pointerDown"};
  let id = "1";
  let parameters = {pointerType: "mouse"};
  let a = new action.Action(id, "pointer", actionItem.type);
  let wrongInputState = new action.InputState.Key();
  action.inputStateMap.set(id, wrongInputState);
  checkErrors(
      /to be mapped to InputState whose type is/, action.processPointerAction,
      [id, parameters, a],
      `type "pointer" with ${wrongInputState.type} in inputState`);
  action.inputStateMap.clear();

  // TODO - uncomment once pen is supported
  // wrongInputState = new action.InputState.Pointer("pen");
  // action.inputStateMap.set(id, wrongInputState);
  // checkErrors(
  //    /to be mapped to InputState whose subtype is/, action.processPointerAction,
  //    [id, parameters, a],
  //    `subtype ${parameters.pointerType} with ${wrongInputState.subtype} in inputState`);
  // action.inputStateMap.clear();

  let rightInputState = new action.InputState.Pointer("mouse");
  action.inputStateMap.set(id, rightInputState);
  action.processPointerAction(id, parameters, a);
  action.inputStateMap.clear();
  run_next_test();
});

add_test(function test_createInputState() {
  for (let kind in action.InputState) {
    let state;
    if (kind == "Pointer") {
      state = new action.InputState[kind]("mouse");
    } else {
      state = new action.InputState[kind]();
    }
    ok(state);
    if (kind === "Null") {
      equal(state.type, "none");
    } else {
      equal(state.type, kind.toLowerCase());
    }
  }
  Assert.throws(() => new action.InputState.Pointer(), InvalidArgumentError,
      "Missing InputState.Pointer constructor arg");
  Assert.throws(() => new action.InputState.Pointer("foo"), InvalidArgumentError,
      "Invalid InputState.Pointer constructor arg");
  run_next_test();
});

add_test(function test_extractActionChainValidation() {
  for (let actions of [-1, "a", undefined, null]) {
    let message = `actions: ${getTypeString(actions)}`;
    Assert.throws(() => action.Chain.fromJSON(actions),
        InvalidArgumentError, message);
    Assert.throws(() => action.Chain.fromJSON(actions),
        /Expected 'actions' to be an array/, message);
  }
  run_next_test();
});

add_test(function test_extractActionChainEmpty() {
  deepEqual(action.Chain.fromJSON([]), []);
  run_next_test();
});

add_test(function test_extractActionChain_oneTickOneInput() {
  let actionItem = {type: "pause", duration: 5000};
  let actionSequence = {
    type: "none",
    id: "some id",
    actions: [actionItem],
  };
  let expectedAction = new action.Action(actionSequence.id, "none", actionItem.type);
  expectedAction.duration = actionItem.duration;
  let actionsByTick = action.Chain.fromJSON([actionSequence]);
  equal(1, actionsByTick.length);
  equal(1, actionsByTick[0].length);
  deepEqual(actionsByTick, [[expectedAction]]);
  action.inputStateMap.clear();
  run_next_test();
});

add_test(function test_extractActionChain_twoAndThreeTicks() {
  let mouseActionItems = [
    {
      type: "pointerDown",
      button: 2,
    },
    {
      type: "pointerUp",
      button: 2,
    },
  ];
  let mouseActionSequence = {
    type: "pointer",
    id: "7",
    actions: mouseActionItems,
    parameters: {
      pointerType: "mouse", // TODO "touch"
    },
  };
  let keyActionItems = [
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
  let actionsByTick = action.Chain.fromJSON([keyActionSequence, mouseActionSequence]);
  // number of ticks is same as longest action sequence
  equal(keyActionItems.length, actionsByTick.length);
  equal(2, actionsByTick[0].length);
  equal(2, actionsByTick[1].length);
  equal(1, actionsByTick[2].length);
  let expectedAction = new action.Action(keyActionSequence.id, "key", keyActionItems[2].type);
  expectedAction.value = keyActionItems[2].value;
  deepEqual(actionsByTick[2][0], expectedAction);
  action.inputStateMap.clear();

  // one empty action sequence
  actionsByTick = action.Chain.fromJSON(
      [keyActionSequence, {type: "none", id: "some", actions: []}]);
  equal(keyActionItems.length, actionsByTick.length);
  equal(1, actionsByTick[0].length);
  action.inputStateMap.clear();
  run_next_test();
});

add_test(function test_computeTickDuration() {
  let expected = 8000;
  let tickActions = [
    {type: "none", subtype: "pause", duration: 5000},
    {type: "key", subtype: "pause", duration: 1000},
    {type: "pointer", subtype: "pointerMove", duration: 6000},
    // invalid because keyDown should not have duration, so duration should be ignored.
    {type: "key", subtype: "keyDown", duration: 100000},
    {type: "pointer", subtype: "pause", duration: expected},
    {type: "pointer", subtype: "pointerUp"},
  ];
  equal(expected, action.computeTickDuration(tickActions));
  run_next_test();
});

add_test(function test_computeTickDuration_empty() {
  equal(0, action.computeTickDuration([]));
  run_next_test();
});

add_test(function test_computeTickDuration_noDurations() {
  let tickActions = [
    // invalid because keyDown should not have duration, so duration should be ignored.
    {type: "key", subtype: "keyDown", duration: 100000},
    // undefined duration permitted
    {type: "none", subtype: "pause"},
    {type: "pointer", subtype: "pointerMove"},
    {type: "pointer", subtype: "pointerDown"},
    {type: "key", subtype: "keyUp"},
  ];

  equal(0, action.computeTickDuration(tickActions));
  run_next_test();
});


// helpers
function getTypeString(obj) {
  return Object.prototype.toString.call(obj);
}

function checkErrors(regex, func, args, message) {
  if (typeof message == "undefined") {
    message = `actionFunc: ${func.name}; args: ${args}`;
  }
  Assert.throws(() => func.apply(this, args), InvalidArgumentError, message);
  Assert.throws(() => func.apply(this, args), regex, message);
}
