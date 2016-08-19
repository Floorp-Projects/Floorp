"use strict";

Components.utils.import("resource://gre/modules/Schemas.jsm");

function generateNsWithPropValue(val) {
  let count = 0;
  return {
    get count() { return count; },
    get prop1() {
      ++count;
      return val;
    },
  };
}

let pathObjApiJson = [
  {
    namespace: "ret_false",
    properties: {
      // Nb. Somehow a string is a valid value for the "object" type.
      prop1: {type: "object", "value": "value of ret-false"},
    },
  },
  {
    namespace: "ret_false_noval",
    properties: {
      prop1: {type: "object"},
    },
  },
  {
    namespace: "dot.obj",
    properties: {
      prop1: {type: "object"},
    },
  },
  {
    namespace: "ret_obj",
    properties: {
      prop1: {type: "object"},
    },
  },
  {
    namespace: "ret_true",
    properties: {
      prop1: {type: "object", value: "value of ret-true"},
    },
  },
  {
    namespace: "ret_true_noval",
    properties: {
      prop1: {type: "object"},
    },
  },
  {
    namespace: "with_submodule",
    types: [{
      id: "subtype",
      type: "object",
      // Properties in submodules are not supported (yet), so we use functions.
      functions: [{
        name: "retObj",
        type: "function",
        parameters: [],
        returns: {type: "string"},
      }, {
        name: "retFalse",
        type: "function",
        parameters: [],
        returns: {type: "string"},
      }, {
        name: "retTrue",
        type: "function",
        parameters: [],
        returns: {type: "string"},
      }],
    }],
    properties: {
      mySub: {$ref: "subtype"},
    },
  },
];
add_task(function* testPathObj() {
  let url = "data:," + JSON.stringify(pathObjApiJson);
  yield Schemas.load(url);

  let localApi = generateNsWithPropValue("localApi value");
  let dotApi = generateNsWithPropValue("dotApi value");
  let submoduleApi = {
    _count: 0,
    retObj() {
      return "submoduleApi value " + (++submoduleApi._count);
    },
  };
  let localWrapper = {
    shouldInject(ns, name) {
      if (ns == "ret_obj") {
        return localApi;
      } else if (ns == "dot.obj") {
        return dotApi;
      } else if (ns == "ret_true" || ns == "ret_true_noval") {
        return true;
      } else if (ns == "ret_false" || ns == "ret_false_noval") {
        return false;
      } else if (ns == "with_submodule") {
        if (name == "subtype" || name == "mySub") {
          return true;
        }
      } else if (ns == "with_submodule.mySub") {
        if (name == "retTrue") {
          return true;
        } else if (name == "retFalse") {
          return false;
        } else if (name == "retObj") {
          return submoduleApi;
        }
      }
      throw new Error(`Unexpected shouldInject call: ${ns} ${name}`);
    },
    getProperty(pathObj, path, name) {
      if (pathObj) {
        return pathObj[name];
      }
      return "fallback,pathObj=" + pathObj;
    },
    callFunction(pathObj, path, name, args) {
      if (pathObj) {
        return pathObj[name](...args);
      }
      return "fallback call,pathObj=" + pathObj;
    },
  };

  let root = {};
  Schemas.inject(root, localWrapper);

  do_check_eq(0, localApi.count);
  do_check_eq(0, dotApi.count);
  do_check_eq(Object.keys(root).sort().join(),
      "dot,ret_obj,ret_true,ret_true_noval,with_submodule");

  do_check_eq("fallback,pathObj=null", root.ret_true_noval.prop1);
  do_check_eq("value of ret-true", root.ret_true.prop1);
  do_check_eq("localApi value", root.ret_obj.prop1);
  do_check_eq(1, localApi.count);
  do_check_eq("dotApi value", root.dot.obj.prop1);
  do_check_eq(1, dotApi.count);

  do_check_eq(Object.keys(root.with_submodule).sort().join(), "mySub");
  let mySub = root.with_submodule.mySub;
  do_check_eq(Object.keys(mySub).sort().join(), "retObj,retTrue");
  do_check_eq("fallback call,pathObj=null", mySub.retTrue());
  do_check_eq("submoduleApi value 1", mySub.retObj());
});
