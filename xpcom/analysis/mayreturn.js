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
  if (TREE_CODE(isn) == RETURN_EXPR) {
    let gms = TREE_OPERAND(isn, 0);
    if (gms) {
      // gms is usually a GIMPLE_MODIFY_STMT but can be a RESULT_DECL
      if (TREE_CODE(gms) == GIMPLE_MODIFY_STMT) {
        let v = GIMPLE_STMT_OPERAND(gms, 1);
        this.vbls.add(v);
        state.add(v);
        this.retvar = v;
      } else if (TREE_CODE(gms) == RESULT_DECL) {
        throw new Error("Weird case hit");
      }
    }
  } else if (TREE_CODE(isn) == GIMPLE_MODIFY_STMT) {
    let lhs = GIMPLE_STMT_OPERAND(isn, 0);
    let rhs = GIMPLE_STMT_OPERAND(isn, 1);
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
