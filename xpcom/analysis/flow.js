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

/* This implements the control flows-through analysis in bug 432917 */
var Zero_NonZero = {}
include('unstable/zero_nonzero.js', Zero_NonZero);

MapFactory.use_injective = true;

// Print a trace for each function analyzed
let TRACE_FUNCTIONS = 0;
// Trace operation of the ESP analysis, use 2 or 3 for more detail
let TRACE_ESP = 0;
// Print time-taken stats
let TRACE_PERF = 0;

function process_tree(fndecl) {
  // At this point we have a function we want to analyze
  if (TRACE_FUNCTIONS) {
    print('* function ' + decl_name(fndecl));
    print('    ' + loc_string(location_of(fndecl)));
  }
  if (TRACE_PERF) timer_start(fstring);

  let cfg = function_decl_cfg(fndecl);

  try {
    let trace = TRACE_ESP;
    let a = new FlowCheck(cfg, trace);
    a.run();
  } catch (e if e == "skip") {
    return
  }
  print("checked " + decl_name(fndecl))
  if (cfg.x_exit_block_ptr.stateIn.substates)
    for each (let substate in cfg.x_exit_block_ptr.stateIn.substates.getValues()) {
      for each (let v in substate.getVariables()) {
        let var_state= substate.get (v)
        let blame = substate.getBlame(v)
        if (var_state != ESP.TOP && typeof var_state == 'string')
          error(decl_name(fndecl) + ": Control did not flow through " +var_state, location_of(blame))
      }
    }
  
  if (TRACE_PERF) timer_stop(fstring);
}

let track_return_loc = 0;
const FLOW_THROUGH = "MUST_FLOW_THROUGH"

function FlowCheck(cfg, trace) {
  let found = create_decl_set(); // ones we already found
  for (let bb in cfg_bb_iterator(cfg)) {
    for (let isn in bb_isn_iterator(bb)) {
      switch (isn.tree_code()) {
      case GIMPLE_CALL: {
        let fn = gimple_call_fndecl(isn)
        if (!fn || decl_name(fn) != FLOW_THROUGH)
          continue;
        this.must_flow_fn = fn
        break
      }
      case GIMPLE_RETURN:
        let ret_expr = return_expr(isn);
        if (track_return_loc && ret_expr) {
          TREE_CHECK(ret_expr, VAR_DECL, RESULT_DECL);
          this.rval = ret_expr;
        }
        break;
      }
    }
  }
  if (!this.must_flow_fn)
    throw "skip"

  let psvar_list = [new ESP.PropVarSpec(this.must_flow_fn, true)]
  
  if (this.rval)
    psvar_list.push(new ESP.PropVarSpec(this.rval))
 
  this.zeroNonzero = new Zero_NonZero.Zero_NonZero()
  ESP.Analysis.call(this, cfg, psvar_list, Zero_NonZero.meet, trace);
}

FlowCheck.prototype = new ESP.Analysis;

function char_star_arg2string(tree) {
  return TREE_STRING_POINTER(tree.tree_check(ADDR_EXPR).operands()[0].tree_check(ARRAY_REF).operands()[0])
}

// State transition function. Mostly, we delegate everything to
// another function as either an assignment or a call.
FlowCheck.prototype.flowState = function(isn, state) {
  switch (TREE_CODE(isn)) {
  case GIMPLE_CALL: {
    let fn = gimple_call_fndecl(isn)
    if (fn == this.must_flow_fn)
      state.assignValue(fn, char_star_arg2string(gimple_call_arg(isn, 0)), isn)
    break
  }
  case GIMPLE_LABEL: {
    let label = decl_name(gimple_op(isn, 0))
    for ([value, blame] in state.yieldPreconditions(this.must_flow_fn)) {
      if (label != value) continue
      // reached the goto label we wanted =D
      state.assignValue(this.must_flow_fn, ESP.TOP, isn)
    }
    break
  }
  case GIMPLE_RETURN:
    for ([value, blame] in state.yieldPreconditions(this.must_flow_fn)) {
      if (typeof value != 'string') continue
      let loc;
      if (this.rval)
        for ([value, blame] in state.yieldPreconditions(this.rval)) {
          loc = value
          break
        }
      error("return without going through label "+ value, loc);
      return  
    }
    break
  case GIMPLE_ASSIGN:
    if (track_return_loc && gimple_op(isn, 0) == this.rval) {
      state.assignValue(this.rval, location_of(isn), isn)
      break
    }
  default:
    this.zeroNonzero.flowState(isn, state)
  }
}

// State transition function to apply branch filters. This is kind
// of boilerplate--we're just handling some stuff that GCC generates.
FlowCheck.prototype.flowStateCond = function(isn, truth, state) {
  this.zeroNonzero.flowStateCond (isn, truth, state)
}
