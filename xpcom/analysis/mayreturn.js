/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* May-return analysis.
 * This makes sense only for functions that return a value. The analysis
 * determines the set of variables that may transitively reach the return
 * statement. */

function MayReturnAnalysis() {
  BackwardAnalysis.apply(this, arguments);
  // May-return variables. We collect them all here.
  this.vbls = create_decl_set();
  // The return value variable itself
  this.retvar = undefined;
}

MayReturnAnalysis.prototype = new BackwardAnalysis;

MayReturnAnalysis.prototype.flowState = function(isn, state) {
  if (TREE_CODE(isn) == GIMPLE_RETURN) {
    let v = return_expr(isn);
    if (!v)
      return;
    if (v.tree_code() == RESULT_DECL) // only an issue with 4.3
      throw new Error("Weird case hit");
    this.vbls.add(v);
    state.add(v);
    this.retvar = v;
  } else if (TREE_CODE(isn) == GIMPLE_ASSIGN) {
    let lhs = gimple_op(isn, 0);
    let rhs = gimple_op(isn, 1);
    if (DECL_P(rhs) && DECL_P(lhs) && state.has(lhs)) {
      this.vbls.add(rhs);
      state.add(rhs);
    }

    for (let e in isn_defs(isn, 'strong')) {
      if (DECL_P(e)) {
        state.remove(e);
      }
    }
  }
};
