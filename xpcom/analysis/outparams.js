/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

require({ version: '1.8' });
require({ after_gcc_pass: 'cfg' });

include('treehydra.js');

include('util.js');
include('gcc_util.js');
include('gcc_print.js');
include('unstable/adts.js');
include('unstable/analysis.js');
include('unstable/esp.js');
let Zero_NonZero = {};
include('unstable/zero_nonzero.js', Zero_NonZero);

include('xpcom/analysis/mayreturn.js');

function safe_location_of(t) {
  if (t === undefined)
    return UNKNOWN_LOCATION;
  
  return location_of(t);
}

MapFactory.use_injective = true;

// Print a trace for each function analyzed
let TRACE_FUNCTIONS = 0;
// Trace operation of the ESP analysis, use 2 or 3 for more detail
let TRACE_ESP = 0;
// Trace determination of function call parameter semantics, 2 for detail
let TRACE_CALL_SEM = 0;
// Print time-taken stats 
let TRACE_PERF = 0;
// Log analysis results in a special format
let LOG_RESULTS = false;

const WARN_ON_SET_NULL = false;
const WARN_ON_SET_FAILURE = false;

// Filter functions to process per CLI
let func_filter;
if (this.arg == undefined || this.arg == '') {
  func_filter = function(fd) true;
} else {
  func_filter = function(fd) function_decl_name(fd) == this.arg;
}

function process_tree(func_decl) {
  if (!func_filter(func_decl)) return;

  // Determine outparams and return if function not relevant
  if (DECL_CONSTRUCTOR_P(func_decl)) return;
  let psem = OutparamCheck.prototype.func_param_semantics(func_decl);
  if (!psem.some(function(x) x.check)) return;
  let decl = rectify_function_decl(func_decl);
  if (decl.resultType != 'nsresult' && decl.resultType != 'PRBool' &&
      decl.resultType != 'void') {
    warning("Cannot analyze outparam usage for function with return type '" +
            decl.resultType + "'", location_of(func_decl));
    return;
  }

  let params = [ v for (v in flatten_chain(DECL_ARGUMENTS(func_decl))) ];
  let outparam_list = [];
  let psem_list = [];
  for (let i = 0; i < psem.length; ++i) {
    if (psem[i].check) {
      outparam_list.push(params[i]);
      psem_list.push(psem[i]);
    }
  }
  if (outparam_list.length == 0) return;

  // At this point we have a function we want to analyze
  let fstring = rfunc_string(decl);
  if (TRACE_FUNCTIONS) {
    print('* function ' + fstring);
    print('    ' + loc_string(location_of(func_decl)));
  }
  if (TRACE_PERF) timer_start(fstring);
  for (let i = 0; i < outparam_list.length; ++i) {
    let p = outparam_list[i];
    if (TRACE_FUNCTIONS) {
      print("  outparam " + expr_display(p) + " " + DECL_UID(p) + ' ' + 
            psem_list[i].label);
    }
  }

  let cfg = function_decl_cfg(func_decl);

  let [retvar, retvars] = function() {
    let trace = 0;
    let a = new MayReturnAnalysis(cfg, trace);
    a.run();
    return [a.retvar, a.vbls];
  }();
  if (retvar == undefined && decl.resultType != 'void') throw new Error("assert");

  {
    let trace = TRACE_ESP;
    for (let i = 0; i < outparam_list.length; ++i) {
      let psem = [ psem_list[i] ];
      let outparam = [ outparam_list[i] ];
      let a = new OutparamCheck(cfg, psem, outparam, retvar, retvars, trace);
      // This is annoying, but this field is only used for logging anyway.
      a.fndecl = func_decl;
      a.run();
      a.check(decl.resultType == 'void', func_decl);
    }
  }
  
  if (TRACE_PERF) timer_stop(fstring);
}

// Outparam check analysis
function OutparamCheck(cfg, psem_list, outparam_list, retvar, retvar_set, 
                       trace) {
  // We need to save the retvars so we can detect assignments through
  // their addresses passed as arguments.
  this.retvar_set = retvar_set;
  this.retvar = retvar;

  // We need both an ordered set and a lookup structure
  this.outparam_list = outparam_list
  this.outparams = create_decl_set(outparam_list);
  this.psem_list = psem_list;

  // Set up property state vars for ESP
  let psvar_list = [];
  for each (let v in outparam_list) {
    psvar_list.push(new ESP.PropVarSpec(v, true, av.NOT_WRITTEN));
  }
  for (let v in retvar_set.items()) {
    psvar_list.push(new ESP.PropVarSpec(v, v == this.retvar, ESP.TOP));
  }
  if (trace) {
    print("PS vars");
    for each (let v in this.psvar_list) {
      print("    " + expr_display(v.vbl));
    }
  }
  this.zeroNonzero = new Zero_NonZero.Zero_NonZero();
  ESP.Analysis.call(this, cfg, psvar_list, av.meet, trace);
}

// Abstract values for outparam check
function AbstractValue(name, ch) {
  this.name = name;
  this.ch = ch;
}

AbstractValue.prototype.equals = function(v) {
  return this === v;
}

AbstractValue.prototype.toString = function() {
  return this.name + ' (' + this.ch + ')';
}

AbstractValue.prototype.toShortString = function() {
  return this.ch;
}

let avspec = [
  // Abstract values for outparam contents write status
  [ 'NULL',          'x' ],   // is a null pointer
  [ 'NOT_WRITTEN',   '-' ],   // not written
  [ 'WROTE_NULL',    '/' ],   // had NULL written to
  [ 'WRITTEN',       '+' ],   // had anything written to
  // MAYBE_WRITTEN is special. "Officially", it means the same thing as
  // NOT_WRITTEN. What it really means is that an outparam was passed
  // to another function as a possible outparam (outparam type, but not
  // in last position), so if there is an error with it not being written,
  // we can give a hint about the possible outparam in the warning.
  [ 'MAYBE_WRITTEN', '?' ],   // written if possible outparam is one
];

let av = {};
for each (let [name, ch] in avspec) {
  av[name] = new AbstractValue(name, ch);
}

av.ZERO = Zero_NonZero.Lattice.ZERO;
av.NONZERO = Zero_NonZero.Lattice.NONZERO;

/*
av.ZERO.negation = av.NONZERO;
av.NONZERO.negation = av.ZERO;

// Abstract values for int constants. We use these to figure out feasible
// paths in the presence of GCC finally_tmp-controlled switches.
function makeIntAV(v) {
  let key = 'int_' + v;
  if (cachedAVs.hasOwnProperty(key)) return cachedAVs[key];

  let s = "" + v;
  let ans = cachedAVs[key] = new AbstractValue(s, s);
  ans.int_val = v;
  return ans;
}
*/

let cachedAVs = {};

// Abstract values for pointers that contain a copy of an outparam 
// pointer. We use these to figure out writes to a casted copy of 
// an outparam passed to another method.
function makeOutparamAV(v) {
  let key = 'outparam_' + DECL_UID(v);
  if (key in cachedAVs) return cachedAVs[key];

  let ans = cachedAVs[key] = 
    new AbstractValue('OUTPARAM:' + expr_display(v), 'P');
  ans.outparam = v;
  return ans;
}

/** Return the integer value if this is an integer av, otherwise undefined. */
av.intVal = function(v) {
  if (v.hasOwnProperty('int_val'))
    return v.int_val;
  return undefined;
}

/** Meet function for our abstract values. */
av.meet = function(v1, v2) {
  // At this point we know v1 != v2.
  let values = [v1,v2]
  if (values.indexOf(av.LOCKED) != -1
      || values.indexOf(av.UNLOCKED) != -1)
    return ESP.NOT_REACHED;

  return Zero_NonZero.meet(v1, v2)
};

// Outparam check analysis
OutparamCheck.prototype = new ESP.Analysis;

OutparamCheck.prototype.split = function(vbl, v) {
  // Can't happen for current version of ESP, but could change
  if (v != ESP.TOP) throw new Error("not implemented");
  return [ av.ZERO, av.NONZERO ];
}

OutparamCheck.prototype.updateEdgeState = function(e) {
  e.state.keepOnly(e.dest.keepVars);
}

OutparamCheck.prototype.flowState = function(isn, state) {
  switch (TREE_CODE(isn)) {
  case GIMPLE_ASSIGN:
    this.processAssign(isn, state);
    break;
  case GIMPLE_CALL:
    this.processCall(isn, isn, state);
    break;
  case GIMPLE_SWITCH:
  case GIMPLE_COND:
    // This gets handled by flowStateCond instead, has no exec effect
    break;
  default:
    this.zeroNonzero.flowState(isn, state);
  }
}

OutparamCheck.prototype.flowStateCond = function(isn, truth, state) {
  this.zeroNonzero.flowStateCond(isn, truth, state);
}

// For any outparams-specific semantics, we handle it here and then
// return. Otherwise we delegate to the zero-nonzero analysis.
OutparamCheck.prototype.processAssign = function(isn, state) {
  let lhs = gimple_op(isn, 0);
  let rhs = gimple_op(isn, 1);

  if (DECL_P(lhs)) {
    // Unwrap NOP_EXPR, which is semantically a copy.
    if (TREE_CODE(rhs) == NOP_EXPR) {
      rhs = rhs.operands()[0];
    }

    if (DECL_P(rhs) && this.outparams.has(rhs)) {
        // Copying an outparam pointer. We have to remember this so that
        // if it is assigned thru later, we pick up the write.
        state.assignValue(lhs, makeOutparamAV(rhs), isn);
        return;
    }

    // Cases of this switch that handle something should return from
    // the function. Anything that does not return is picked up afteward.
    switch (TREE_CODE(rhs)) {
    case INTEGER_CST:
      if (this.outparams.has(lhs)) {
        warning("assigning to outparam pointer");
        return;
      }
      break;
    case EQ_EXPR: {
      // We only care about testing outparams for NULL (and then not writing)
      let [op1, op2] = rhs.operands();
      if (DECL_P(op1) && this.outparams.has(op1) && expr_literal_int(op2) == 0) {
        state.update(function(ss) {
          let [s1, s2] = [ss, ss.copy()]; // s1 true, s2 false
          s1.assignValue(lhs, av.NONZERO, isn);
          s1.assignValue(op1, av.NULL, isn);
          s2.assignValue(lhs, av.ZERO, isn);
          return [s1, s2];
        });
        return;
      }
    }
      break;
    case CALL_EXPR:
      /* Embedded CALL_EXPRs are a 4.3 issue */
      this.processCall(rhs, isn, state, lhs);
      return;

    case INDIRECT_REF:
      // If rhs is *outparam and pointer-typed, lhs is NULL iff rhs is
      // WROTE_NULL. Required for testcase onull.cpp.
      let v = rhs.operands()[0];
      if (DECL_P(v) && this.outparams.has(v) && 
          TREE_CODE(TREE_TYPE(v)) == POINTER_TYPE) {
        state.update(function(ss) {
          let val = ss.get(v) == av.WROTE_NULL ? av.ZERO : av.NONZERO;
          ss.assignValue(lhs, val, isn);
          return [ ss ];
        });
        return;
      }
    }

    // Nothing special -- delegate
    this.zeroNonzero.processAssign(isn, state);
    return;
  }

  switch (TREE_CODE(lhs)) {
  case INDIRECT_REF:
    // Writing to an outparam. We want to try to figure out if we're 
    // writing NULL.
    let e = TREE_OPERAND(lhs, 0);
    if (this.outparams.has(e)) {
      if (expr_literal_int(rhs) == 0) {
        state.assignValue(e, av.WROTE_NULL, isn);
      } else if (DECL_P(rhs)) {
        state.update(function(ss) {
          let [s1, s2] = [ss.copy(), ss]; // s1 NULL, s2 non-NULL
          s1.assignValue(e, av.WROTE_NULL, isn);
          s1.assignValue(rhs, av.ZERO, isn);
          s2.assignValue(e, av.WRITTEN, isn);
          s2.assignValue(rhs, av.NONZERO, isn);
          return [s1,s2];
        });
      } else {
        state.assignValue(e, av.WRITTEN, isn);
      }
    } else {
      // unsound -- could be writing to anything through this ptr
    }
    break;
  case COMPONENT_REF: // unsound
  case ARRAY_REF: // unsound
  case EXC_PTR_EXPR:
  case FILTER_EXPR:
    break;
  default:
    print(TREE_CODE(lhs));
    throw new Error("ni");
  }
}

// Handle an assignment x := test(foo) where test is a simple predicate
OutparamCheck.prototype.processTest = function(lhs, call, val, blame, state) {
  let arg = gimple_call_arg(call, 0);
  if (DECL_P(arg)) {
    this.zeroNonzero.predicate(state, lhs, val, arg, blame);
  } else {
    state.assignValue(lhs, ESP.TOP, blame);
  }
};

// The big one: outparam semantics of function calls.
OutparamCheck.prototype.processCall = function(call, blame, state, dest) {
  if (!dest)
    dest = gimple_call_lhs(call);

  let args = gimple_call_args(call);
  let callable = callable_arg_function_decl(gimple_call_fn(call));
  let psem = this.func_param_semantics(callable);

  let name = function_decl_name(callable);
  if (name == 'NS_FAILED') {
    this.processTest(dest, call, av.NONZERO, call, state);
    return;
  } else if (name == 'NS_SUCCEEDED') {
    this.processTest(dest, call, av.ZERO, call, state);
    return;
  } else if (name == '__builtin_expect') {
    // Same as an assign from arg 0 to lhs
    state.assign(dest, args[0], call);
    return;
  }

  if (TRACE_CALL_SEM) {
    print("param semantics:" + psem);
  }

  if (args.length != psem.length) {
    let ct = TREE_TYPE(callable);
    if (TREE_CODE(ct) == POINTER_TYPE) ct = TREE_TYPE(ct);
    if (args.length < psem.length || !stdarg_p(ct)) {
      // TODO Can __builtin_memcpy write to an outparam? Probably not.
      if (name != 'operator new' && name != 'operator delete' &&
          name != 'operator new []' && name != 'operator delete []' &&
          name.substr(0, 5) != '__cxa' &&
          name.substr(0, 9) != '__builtin') {
        throw Error("bad len for '" + name + "': " + args.length + ' args, ' + 
                    psem.length + ' params');
      }
    }
  }

  // Collect variables that are possibly written to on callee success
  let updates = [];
  for (let i = 0; i < psem.length; ++i) {
    let arg = args[i];
    // The arg could be the address of a return-value variable.
    // This means it's really the nsresult code for the call,
    // so we treat it the same as the target of an rv assignment.
    if (TREE_CODE(arg) == ADDR_EXPR) {
      let v = arg.operands()[0];
      if (DECL_P(v) && this.retvar_set.has(v)) {
        dest = v;
      }
    }
    // The arg could be a copy of an outparam. We'll unwrap to the
    // outparam if it is. The following is cheating a bit because
    // we munge states together, but it should be OK in practice.
    arg = unwrap_outparam(arg, state);
    let sem = psem[i];
    if (sem == ps.CONST) continue;
    // At this point, we know the call can write thru this param.
    // Invalidate any vars whose addresses are passed here. This
    // is distinct from the rv handling above.
    if (TREE_CODE(arg) == ADDR_EXPR) {
      let v = arg.operands()[0];
      if (DECL_P(v)) {
        state.remove(v);
      }
    }
    if (!DECL_P(arg) || !this.outparams.has(arg)) continue;
    // At this point, we may be writing to an outparam 
    updates.push([arg, sem]);
  }
  
  if (updates.length) {
    if (dest != undefined && DECL_P(dest)) {
      // Update & stored rv. Do updates predicated on success.
      let [ succ_ret, fail_ret ] = ret_coding(callable);

      state.update(function(ss) {
        let [s1, s2] = [ss.copy(), ss]; // s1 success, s2 fail
        for each (let [vbl, sem] in updates) {
          s1.assignValue(vbl, sem.val, blame);
          s1.assignValue(dest, succ_ret, blame);
        }
        s2.assignValue(dest, fail_ret, blame);
        return [s1,s2];
      });
    } else {
      // Discarded rv. Per spec in the bug, we assume that either success
      // or failure is possible (if not, callee should return void).
      // Exceptions: Methods that return void and string mutators are
      //     considered no-fail.
      state.update(function(ss) {
        for each (let [vbl, sem] in updates) {
          if (sem == ps.OUTNOFAIL || sem == ps.OUTNOFAILNOCHECK) {
            ss.assignValue(vbl, av.WRITTEN, blame);
            return [ss];
          } else {
            let [s1, s2] = [ss.copy(), ss]; // s1 success, s2 fail
            for each (let [vbl, sem] in updates) {
              s1.assignValue(vbl, sem.val, blame);
            }
            return [s1,s2];
          }
        }
      });
    }
  } else {
    // no updates, just kill any destination for the rv
    if (dest != undefined && DECL_P(dest)) {
      state.remove(dest, blame);
    }
  }
};

/** Return the return value coding of the given function. This is a pair
 *  [ succ, fail ] giving the abstract values of the return value under
 *  success and failure conditions. */
function ret_coding(callable) {
  let type = TREE_TYPE(callable);
  if (TREE_CODE(type) == POINTER_TYPE) type = TREE_TYPE(type);

  let rtname = TYPE_NAME(TREE_TYPE(type));
  if (rtname && IDENTIFIER_POINTER(DECL_NAME(rtname)) == 'PRBool') {
    return [ av.NONZERO, av.ZERO ];
  } else {
    return [ av.ZERO, av.NONZERO ];
  }
}

function unwrap_outparam(arg, state) {
  if (!DECL_P(arg) || state.factory.outparams.has(arg)) return arg;

  let outparam;
  for (let ss in state.substates.getValues()) {
    let val = ss.get(arg);
    if (val != undefined && val.hasOwnProperty('outparam')) {
      outparam = val.outparam;
    }
  }
  if (outparam) return outparam;
  return arg;
}

// Check for errors. Must .run() analysis before calling this.
OutparamCheck.prototype.check = function(isvoid, fndecl) {
  let state = this.cfg.x_exit_block_ptr.stateOut;
  for (let substate in state.substates.getValues()) {
    this.checkSubstate(isvoid, fndecl, substate);
  }
}

OutparamCheck.prototype.checkSubstate = function(isvoid, fndecl, ss) {
  if (isvoid) {
    this.checkSubstateSuccess(ss);
  } else {
    let [succ, fail] = ret_coding(fndecl);
    let rv = ss.get(this.retvar);
    // We want to check if the abstract value of the rv is entirely
    // contained in the success or failure condition.
    if (av.meet(rv, succ) == rv) {
      this.checkSubstateSuccess(ss);
    } else if (av.meet(rv, fail) == rv) {
      this.checkSubstateFailure(ss);
    } else {
      // This condition indicates a bug in outparams.js. We'll just
      // warn so we don't break static analysis builds.
      warning("Outparams checker cannot determine rv success/failure",
              location_of(fndecl));
      this.checkSubstateSuccess(ss);
      this.checkSubstateFailure(ss);
    }
  }
}

/* @return     The return statement in the function
 *             that writes the return value in the given substate.
 *             If the function returns void, then the substate doesn't
 *             matter and we just look for the return. */
OutparamCheck.prototype.findReturnStmt = function(ss) {
  if (this.retvar != undefined)
    return ss.getBlame(this.retvar);

  if (this.cfg._cached_return)
    return this.cfg._cached_return;
  
  for (let bb in cfg_bb_iterator(this.cfg)) {
    for (let isn in bb_isn_iterator(bb)) {
      if (isn.tree_code() == GIMPLE_RETURN) {
        return this.cfg._cached_return = isn;
      }
    }
  }

  return undefined;
}

OutparamCheck.prototype.checkSubstateSuccess = function(ss) {
  for (let i = 0; i < this.psem_list.length; ++i) {
    let [v, psem] = [ this.outparam_list[i], this.psem_list[i] ];
    if (psem == ps.INOUT) continue;
    let val = ss.get(v);
    if (val == av.NOT_WRITTEN) {
      this.logResult('succ', 'not_written', 'error');
      this.warn([this.findReturnStmt(ss), "outparam '" + expr_display(v) + "' not written on NS_SUCCEEDED(return value)"],
                [v, "outparam declared here"]);
    } else if (val == av.MAYBE_WRITTEN) {
      this.logResult('succ', 'maybe_written', 'error');

      let blameStmt = ss.getBlame(v);
      let callMsg;
      let callName = "";
      try {
        let call = TREE_CHECK(blameStmt, GIMPLE_CALL, GIMPLE_MODIFY_STMT);
        let callDecl = callable_arg_function_decl(gimple_call_fn(call));
        
        callMsg = [callDecl, "declared here"];
        callName = " '" + decl_name(callDecl) + "'";
      }
      catch (e if e.TreeCheckError) { }
      
      this.warn([this.findReturnStmt(ss), "outparam '" + expr_display(v) + "' not written on NS_SUCCEEDED(return value)"],
                [v, "outparam declared here"],
                [blameStmt, "possibly written by unannotated function call" + callName],
                callMsg);
    } else {
      this.logResult('succ', '', 'ok');
    }
  }    
}

OutparamCheck.prototype.checkSubstateFailure = function(ss) {
  for (let i = 0; i < this.psem_list.length; ++i) {
    let [v, ps] = [ this.outparam_list[i], this.psem_list[i] ];
    let val = ss.get(v);
    if (val == av.WRITTEN) {
      this.logResult('fail', 'written', 'error');
      if (WARN_ON_SET_FAILURE) {
        this.warn([this.findReturnStmt(ss), "outparam '" + expr_display(v) + "' written on NS_FAILED(return value)"],
                  [v, "outparam declared here"],
                  [ss.getBlame(v), "written here"]);
      }
    } else if (val == av.WROTE_NULL) {
      this.logResult('fail', 'wrote_null', 'warning');
      if (WARN_ON_SET_NULL) {
        this.warn([this.findReturnStmt(ss), "NULL written to outparam '" + expr_display(v) + "' on NS_FAILED(return value)"],
                  [v, "outparam declared here"],
                  [ss.getBlame(v), "written here"]);
      }
    } else {
      this.logResult('fail', '', 'ok');
    }
  }    
}

/**
 * Generate a warning from one or more tuples [treeforloc, message]
 */
OutparamCheck.prototype.warn = function(arg0) {
  let loc = safe_location_of(arg0[0]);
  let msg = arg0[1];

  for (let i = 1; i < arguments.length; ++i) {
    if (arguments[i] === undefined) continue;
    let [atree, amsg] = arguments[i];
    msg += "\n" + loc_string(safe_location_of(atree)) + ":   " + amsg;
  }
  warning(msg, loc);
}

OutparamCheck.prototype.logResult = function(rv, msg, kind) {
  if (LOG_RESULTS) {
    let s = [ '"' + x + '"' for each (x in [ loc_string(location_of(this.fndecl)), function_decl_name(this.fndecl), rv, msg, kind ]) ].join(', ');
    print(":LR: (" + s + ")");
  }
}

// Parameter Semantics values -- indicates whether a parameter is
// an outparam.
//    label    Used for debugging output
//    val      Abstract value (state) that holds on an argument after
//             a call
//    check    True if parameters with this semantics should be
//             checked by this analysis
let ps = {
  OUTNOFAIL: { label: 'out-no-fail', val: av.WRITTEN,  check: true },
  // Special value for receiver of strings methods. Callers should
  // consider this to be an outparam (i.e., it modifies the string),
  // but we don't want to check the method itself.
  OUTNOFAILNOCHECK: { label: 'out-no-fail-no-check' },
  OUT:       { label: 'out',         val: av.WRITTEN,  check: true },
  INOUT:     { label: 'inout',       val: av.WRITTEN,  check: true },
  MAYBE:     { label: 'maybe',       val: av.MAYBE_WRITTEN},  // maybe out
  CONST:     { label: 'const' }   // i.e. not out
};

// Return the param semantics of a FUNCTION_DECL or VAR_DECL representing
// a function pointer. The result is a pair [ ann, sems ].
OutparamCheck.prototype.func_param_semantics = function(callable) {
  let ftype = TREE_TYPE(callable);
  if (TREE_CODE(ftype) == POINTER_TYPE) ftype = TREE_TYPE(ftype);
  // What failure semantics to use for outparams
  let rtype = TREE_TYPE(ftype);
  let nofail = TREE_CODE(rtype) == VOID_TYPE;
  // Whether to guess outparams by type
  let guess = type_string(rtype) == 'nsresult';

  // Set up param lists for analysis
  let params;     // param decls, if available
  let types;      // param types
  let string_mutator = false;
  if (TREE_CODE(callable) == FUNCTION_DECL) {
    params = [ p for (p in function_decl_params(callable)) ];
    types = [ TREE_TYPE(p) for each (p in params) ];
    string_mutator = is_string_mutator(callable);
  } else {
    types = [ p for (p in function_type_args(ftype)) 
                if (TREE_CODE(p) != VOID_TYPE) ];
  }

  // Analyze params
  let ans = [];
  for (let i = 0; i < types.length; ++i) {
    let sem;
    if (i == 0 && string_mutator) {
      // Special case: string mutator receiver is an no-fail outparams
      //               but not checkable
      sem = ps.OUTNOFAILNOCHECK;
    } else {
      if (params) sem = decode_attr(DECL_ATTRIBUTES(params[i]));
      if (TRACE_CALL_SEM >= 2) print("param " + i + ": annotated " + sem);
      if (sem == undefined) {
        sem = decode_attr(TYPE_ATTRIBUTES(types[i]));
        if (TRACE_CALL_SEM >= 2) print("type " + i + ": annotated " + sem);
        if (sem == undefined) {
          if (guess && type_is_outparam(types[i])) {
            // Params other than last are guessed as MAYBE
            sem = i < types.length - 1 ? ps.MAYBE : ps.OUT;
          } else {
            sem = ps.CONST;
          }
        }
      }
      if (sem == ps.OUT && nofail) sem = ps.OUTNOFAIL;
    }
    if (sem == undefined) throw new Error("assert");
    ans.push(sem);
  }
  return ans;
}

/* Decode parameter semantics GCC attributes.
 * @param attrs    GCC attributes of a parameter. E.g., TYPE_ATTRIBUTES
 *                 or DECL_ATTRIBUTES of an item
 * @return         The parameter semantics value defined by the attributes,
 *                 or undefined if no such attributes were present. */
function decode_attr(attrs) {
  // Note: we're not checking for conflicts, we just take the first
  // one we find.
  for each (let attr in rectify_attributes(attrs)) {
    if (attr.name == 'user') {
      for each (let arg in attr.args) {
        if (arg == 'NS_outparam') {
          return ps.OUT;
        } else if (arg == 'NS_inoutparam') {
          return ps.INOUT;
        } else if (arg == 'NS_inparam') {
          return ps.CONST;
        }
      }
    }
  }
  return undefined;
}

/* @return       true if the given type appears to be an outparam
 *               type based on the type alone (i.e., not considering
 *               attributes. */
function type_is_outparam(type) {
  switch (TREE_CODE(type)) {
  case POINTER_TYPE:
    return pointer_type_is_outparam(TREE_TYPE(type));
  case REFERENCE_TYPE:
    let rt = TREE_TYPE(type);
    return !TYPE_READONLY(rt) && is_string_type(rt);
  default:
    // Note: This is unsound for UNION_TYPE, because the union could
    //       contain a pointer.
    return false;
  }
}

/* Helper for type_is_outparam.
 * @return      true if 'pt *' looks like an outparam type. */
function pointer_type_is_outparam(pt) {
  if (TYPE_READONLY(pt)) return false;

  switch (TREE_CODE(pt)) {
  case POINTER_TYPE:
  case ARRAY_TYPE: {
    // Look for void **, nsIFoo **, char **, PRUnichar **
    let ppt = TREE_TYPE(pt);
    let tname = TYPE_NAME(ppt);
    if (tname == undefined) return false;
    let name = decl_name_string(tname);
    return name == 'void' || name == 'char' || name == 'PRUnichar' ||
      name.substr(0, 3) == 'nsI';
  }
  case INTEGER_TYPE: {
    // char * and PRUnichar * are probably strings, otherwise guess
    // it is an integer outparam.
    let name = decl_name_string(TYPE_NAME(pt));
    return name != 'char' && name != 'PRUnichar';
  }
  case ENUMERAL_TYPE:
  case REAL_TYPE:
  case UNION_TYPE:
  case BOOLEAN_TYPE:
    return true;
  case RECORD_TYPE:
    // TODO: should we consider field writes?
    return false;
  case FUNCTION_TYPE:
  case VOID_TYPE:
    return false;
  default:
    throw new Error("can't guess if a pointer to this type is an outparam: " +
                    TREE_CODE(pt) + ': ' + type_string(pt));
  }
}

// Map type name to boolean as to whether it is a string.
let cached_string_types = MapFactory.create_map(
  function (x, y) x == y,
  function (x) x,
  function (t) t,
  function (t) t);

// Base string types. Others will be found by searching the inheritance
// graph.

cached_string_types.put('nsAString', true);
cached_string_types.put('nsACString', true);
cached_string_types.put('nsAString_internal', true);
cached_string_types.put('nsACString_internal', true);

// Return true if the given type represents a Mozilla string type.
// The binfo arg is the binfo to use for further iteration. This is
// for internal use only, users of this function should pass only
// one arg.
function is_string_type(type, binfo) {
  if (TREE_CODE(type) != RECORD_TYPE) return false;
  //print(">>>IST " + type_string(type));
  let name = decl_name_string(TYPE_NAME(type));
  let ans = cached_string_types.get(name);
  if (ans != undefined) return ans;

  ans = false;
  binfo = binfo != undefined ? binfo : TYPE_BINFO(type);
  if (binfo != undefined) {
    for each (let base in VEC_iterate(BINFO_BASE_BINFOS(binfo))) {
      let parent_ans = is_string_type(BINFO_TYPE(base), base);
      if (parent_ans) {
        ans = true;
        break;
      }
    }
  }
  cached_string_types.put(name, ans);
  //print("<<<IST " + type_string(type) + ' ' + ans);
  return ans;
}

function is_string_ptr_type(type) {
  return TREE_CODE(type) == POINTER_TYPE && is_string_type(TREE_TYPE(type));
}

// Return true if the given function is a mutator method of a Mozilla
// string type.
function is_string_mutator(fndecl) {
  let first_param = function() {
    for (let p in function_decl_params(fndecl)) {
      return p;
    }
    return undefined;
  }();

  return first_param != undefined && 
    decl_name_string(first_param) == 'this' &&
    is_string_ptr_type(TREE_TYPE(first_param)) &&
    !TYPE_READONLY(TREE_TYPE(TREE_TYPE(first_param)));
}

