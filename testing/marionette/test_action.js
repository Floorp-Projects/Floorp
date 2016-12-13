/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

Cu.import("chrome://marionette/content/action.js");
Cu.import("chrome://marionette/content/element.js");
Cu.import("chrome://marionette/content/error.js");

action.inputStateMap = new Map();

add_test(function test_createAction() {
  Assert.throws(() => new action.Action(), InvalidArgumentError,
      "Missing Action constructor args");
  Assert.throws(() => new action.Action(1,2), InvalidArgumentError,
      "Missing Action constructor args");
  Assert.throws(
      () => new action.Action(1, 2, "sometype"), /Expected string/, "Non-string arguments.");
  ok(new action.Action("id", "sometype", "sometype"));

  run_next_test();
});

add_test(function test_defaultPointerParameters() {
  let defaultParameters = {pointerType: action.PointerType.Mouse, primary: true};
  deepEqual(action.PointerParameters.fromJson(), defaultParameters);

  run_next_test();
});

add_test(function test_processPointerParameters() {
  let check = (regex, message, arg) => checkErrors(
      regex, action.PointerParameters.fromJson, [arg], message);
  let parametersData = {pointerType: "foo", primary: undefined};
  let message = `parametersData: [pointerType: ${parametersData.pointerType}, ` +
      `primary: ${parametersData.primary}]`;
  check(/Unknown pointerType/, message, parametersData);
  parametersData.pointerType = "pen";
  parametersData.primary = "a";
  check(/Expected \[object String\] "a" to be boolean/, message, parametersData);
  parametersData.primary = false;
  deepEqual(action.PointerParameters.fromJson(parametersData),
      {pointerType: action.PointerType.Pen, primary: false});

  run_next_test();
});

add_test(function test_processPointerUpDownAction() {
  let actionItem = {type: "pointerDown"};
  let actionSequence = {type: "pointer", id: "some_id"};
  for (let d of [-1, "a"]) {
    actionItem.button = d;
    checkErrors(
        /Expected 'button' \(.*\) to be >= 0/, action.Action.fromJson, [actionSequence, actionItem],
        `button: ${actionItem.button}`);
  }
  actionItem.button = 5;
  let act = action.Action.fromJson(actionSequence, actionItem);
  equal(act.button, actionItem.button);

  run_next_test();
});

add_test(function test_validateActionDurationAndCoordinates() {
  let actionItem = {};
  let actionSequence = {id: "some_id"};
  let check = function (type, subtype, message = undefined) {
    message = message || `duration: ${actionItem.duration}, subtype: ${subtype}`;
    actionItem.type = subtype;
    actionSequence.type = type;
    checkErrors(/Expected '.*' \(.*\) to be >= 0/,
        action.Action.fromJson, [actionSequence, actionItem], message);
  };
  for (let d of [-1, "a"]) {
    actionItem.duration = d;
    check("none", "pause");
    check("pointer", "pointerMove");
  }
  actionItem.duration = 5000;
  for (let d of [-1, "a"]) {
    for (let name of ["x", "y"]) {
      actionItem[name] = d;
      check("pointer", "pointerMove", `${name}: ${actionItem[name]}`);
    }
  }
  run_next_test();
});

add_test(function test_processPointerMoveActionElementValidation() {
  let actionSequence = {type: "pointer", id: "some_id"};
  let actionItem = {duration: 5000, type: "pointerMove"};
  for (let d of [-1, "a", {a: "blah"}]) {
    actionItem.element = d;
    checkErrors(/Expected 'actionItem.element' to be a web element reference/,
        action.Action.fromJson,
        [actionSequence, actionItem],
        `actionItem.element: (${getTypeString(d)})`);
  }
  actionItem.element = {[element.Key]: "something"};
  let a = action.Action.fromJson(actionSequence, actionItem);
  deepEqual(a.element, actionItem.element);

  run_next_test();
});

add_test(function test_processPointerMoveAction() {
  let actionSequence = {id: "some_id", type: "pointer"};
  let actionItems = [
    {
      duration: 5000,
      type: "pointerMove",
      element: undefined,
      x: undefined,
      y: undefined,
    },
    {
      duration: undefined,
      type: "pointerMove",
      element: {[element.Key]: "id", [element.LegacyKey]: "id"},
      x: undefined,
      y: undefined,
    },
    {
      duration: 5000,
      type: "pointerMove",
      x: 0,
      y: undefined,
      element: undefined,
    },
    {
      duration: 5000,
      type: "pointerMove",
      x: 1,
      y: 2,
      element: undefined,
    },
  ];
  for (let expected of actionItems) {
    let actual = action.Action.fromJson(actionSequence, expected);
    ok(actual instanceof action.Action);
    equal(actual.duration, expected.duration);
    equal(actual.element, expected.element);
    equal(actual.x, expected.x);
    equal(actual.y, expected.y);
  }
  run_next_test();
});

add_test(function test_processPointerAction() {
  let actionSequence = {
    type: "pointer",
    id: "some_id",
    parameters: {
      pointerType: "touch",
      primary: false,
    },
  }
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
    }
  ];
  for (let expected of actionItems) {
    let actual = action.Action.fromJson(actionSequence, expected);
    equal(actual.type, actionSequence.type);
    equal(actual.subtype, expected.type);
    equal(actual.id, actionSequence.id);
    if (expected.type === "pointerUp") {
      equal(actual.button, expected.button);
    } else {
      equal(actual.duration, expected.duration);
    }
    if (expected.type !== "pause") {
      equal(actual.primary, actionSequence.parameters.primary);
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
    let act = action.Action.fromJson(actionSequence, actionItem);
    ok(act instanceof action.Action);
    equal(act.type, type);
    equal(act.subtype, actionItem.type);
    equal(act.id, actionSequence.id);
    equal(act.duration, actionItem.duration);
  }
  actionItem.duration = undefined;
  let act = action.Action.fromJson(actionSequence, actionItem);
  equal(act.duration, actionItem.duration);

  run_next_test();
});

add_test(function test_processActionSubtypeValidation() {
  let actionItem = {type: "dancing"};
  let actionSequence = {id: "some_id"};
  let check = function (regex) {
    let message = `type: ${actionSequence.type}, subtype: ${actionItem.type}`;
    checkErrors(regex, action.Action.fromJson, [actionSequence, actionItem], message);
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
    Assert.throws(() => action.Action.fromJson(actionSequence, actionItem),
        InvalidArgumentError, message);
    Assert.throws(() => action.Action.fromJson(actionSequence, actionItem),
        /Expected 'value' to be a string that represents single code point/, message);
  }

  actionItem.value = "\uE004";
  let act = action.Action.fromJson(actionSequence, actionItem);
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
      regex, action.Sequence.fromJson, [actionSequence], message);
  check(`actionSequence.type: ${actionSequence.type}`, /Unknown action type/);

  actionSequence.type = "none";
  actionSequence.id = -1;
  check(`actionSequence.id: ${getTypeString(actionSequence.id)}`,
      /Expected 'id' to be a string/);

  actionSequence.id = "some_id";
  actionSequence.actions = -1;
  check(`actionSequence.actions: ${getTypeString(actionSequence.actions)}`,
      /Expected 'actionSequence.actions' to be an Array/);

  run_next_test();
});

add_test(function test_processInputSourceActionSequence() {
  let actionItem = { type: "pause", duration: 5};
  let actionSequence = {
    type: "none",
    id: "some id",
    actions: [actionItem],
  };
  let expectedAction = new action.Action(actionSequence.id, "none", actionItem.type);
  expectedAction.duration = actionItem.duration;
  let actions = action.Sequence.fromJson(actionSequence);
  equal(actions.length, 1);
  deepEqual(actions[0], expectedAction);
  run_next_test();
});

add_test(function test_processInputSourceActionSequencePointer() {
  let actionItem = {type: "pointerDown", button: 1};
  let actionSequence = {
    type: "pointer",
    id: "9",
    actions: [actionItem],
    parameters: {
      pointerType: "pen",
      primary: false,
    },
  };
  let expectedAction = new action.Action(
      actionSequence.id, actionSequence.type, actionItem.type);
  expectedAction.pointerType = actionSequence.parameters.pointerType;
  expectedAction.primary = actionSequence.parameters.primary;
  expectedAction.button = actionItem.button;
  let actions = action.Sequence.fromJson(actionSequence);
  equal(actions.length, 1);
  deepEqual(actions[0], expectedAction);
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
  let actions = action.Sequence.fromJson(actionSequence);
  equal(actions.length, 1);
  deepEqual(actions[0], expectedAction);
  run_next_test();
});

add_test(function test_processInputSourceActionSequenceGenerateID() {
  let actionItems = [
    {
      type: "pause",
      duration: 5000,
    },
  ];
  let actionSequence = {
    type: "key",
    actions: actionItems,
  };
  let actions = action.Sequence.fromJson(actionSequence);
  equal(typeof actions[0].id, "string");
  ok(actions[0].id.match(/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i));
  run_next_test();
});

add_test(function test_processInputSourceActionSequenceInputStateMap() {
  let id = "1";
  let actionItem = {type: "pause", duration: 5000};
  let actionSequence = {
    type: "key",
    id: id,
    actions: [actionItem],
  };
  let wrongInputState = new action.InputState.Null();
  action.inputStateMap.set(actionSequence.id, wrongInputState);
  checkErrors(/to be mapped to/, action.Sequence.fromJson, [actionSequence],
      `${actionSequence.type} using ${wrongInputState}`);
  action.inputStateMap.clear();
  let rightInputState = new action.InputState.Key();
  action.inputStateMap.set(id, rightInputState);
  let acts = action.Sequence.fromJson(actionSequence);
  equal(acts.length, 1);
  action.inputStateMap.clear();
  run_next_test();
});

add_test(function test_processPointerActionInputStateMap() {
  let actionItem = {type: "pointerDown"};
  let id = "1";
  let parameters = {pointerType: "mouse", primary: false};
  let a = new action.Action(id, "pointer", actionItem.type);
  let wrongInputState = new action.InputState.Pointer("pause", true);
  action.inputStateMap.set(id, wrongInputState)
  checkErrors(
      /to be mapped to InputState whose subtype is/, action.processPointerAction,
      [id, parameters, a],
      `$subtype ${actionItem.type} with ${wrongInputState.subtype} in inputState`);
  action.inputStateMap.clear();
  let rightInputState = new action.InputState.Pointer("pointerDown", false);
  action.inputStateMap.set(id, rightInputState);
  action.processPointerAction(id, parameters, a);
  equal(a.primary, parameters.primary);
  action.inputStateMap.clear();
  run_next_test();
});

add_test(function test_extractActionChainValidation() {
  for (let actions of [-1, "a", undefined, null]) {
    let message = `actions: ${getTypeString(actions)}`
    Assert.throws(() => action.Chain.fromJson(actions),
        InvalidArgumentError, message);
    Assert.throws(() => action.Chain.fromJson(actions),
        /Expected 'actions' to be an Array/, message);
  }
  run_next_test();
});

add_test(function test_extractActionChainEmpty() {
  deepEqual(action.Chain.fromJson([]), []);
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
  let actionsByTick = action.Chain.fromJson([actionSequence]);
  equal(1, actionsByTick.length);
  equal(1, actionsByTick[0].length);
  deepEqual(actionsByTick, [[expectedAction]]);
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
      pointerType: "touch",
      primary: false,
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
  let actionsByTick = action.Chain.fromJson([keyActionSequence, mouseActionSequence]);
  // number of ticks is same as longest action sequence
  equal(keyActionItems.length, actionsByTick.length);
  equal(2, actionsByTick[0].length);
  equal(2, actionsByTick[1].length);
  equal(1, actionsByTick[2].length);
  let expectedAction = new action.Action(keyActionSequence.id, "key", keyActionItems[2].type);
  expectedAction.value = keyActionItems[2].value;
  deepEqual(actionsByTick[2][0], expectedAction);

  // one empty action sequence
  actionsByTick = action.Chain.fromJson([keyActionSequence, {type: "none", actions: []}]);
  equal(keyActionItems.length, actionsByTick.length);
  equal(1, actionsByTick[0].length);
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
};

function checkErrors(regex, func, args, message) {
  if (typeof message == "undefined") {
    message = `actionFunc: ${func.name}; args: ${args}`;
  }
  Assert.throws(() => func.apply(this, args), InvalidArgumentError, message);
  Assert.throws(() => func.apply(this, args), regex, message);
};
