/** Liveness analysis.. */

function LivenessAnalysis() {
  BackwardAnalysis.apply(this, arguments);
}

LivenessAnalysis.prototype = new BackwardAnalysis;

LivenessAnalysis.prototype.flowState = function(isn, state) {
  switch (TREE_CODE(isn)) {
  case RETURN_EXPR:
    let gms = TREE_OPERAND(isn, 0);
    if (gms) {
      // gms is usually a GIMPLE_MODIFY_STMT but can be a RESULT_DECL
      if (TREE_CODE(gms) == GIMPLE_MODIFY_STMT) {
        let v = GIMPLE_STMT_OPERAND(gms, 1);
        state.add(v);
      } else if (TREE_CODE(gms) == RESULT_DECL) {
        // TODO figure out what really happens here
        // Presumably we already saw the assignment to it.
      }
    }
    break;
  case GIMPLE_MODIFY_STMT:
  case COND_EXPR:
  case SWITCH_EXPR:
  case CALL_EXPR:
    for (let e in isn_defs(isn, 'strong')) {
      if (DECL_P(e)) {
        state.remove(e);
      }
    }
    for (let e in isn_uses(isn)) {
      if (DECL_P(e)) {
        state.add(e);
      }
    }
    break;
  default:
    break;
  }
};
