/**
 * Detects static initializers i.e. functions called during static initialization.
 */

require({ after_gcc_pass: "cfg" });

function process_tree(fn) {
  for each (let attr in translate_attributes(DECL_ATTRIBUTES(fn)))
    if (attr.name == "constructor")
      warning(pretty_func(fn) + " marked with constructor attribute\n");

  if (decl_name_string(fn) != "__static_initialization_and_destruction_0")
    return;

  let cfg = function_decl_cfg(fn);
  for (let isn in cfg_isn_iterator(cfg)) {
    if (isn.tree_code() != GIMPLE_CALL)
      continue;
    let decl = gimple_call_fndecl(isn);
    let lhs = gimple_call_lhs(isn);
    if (lhs) {
      warning(pretty_var(lhs) + " defined by call to " + pretty_func(decl) +
              " during static initialization", location_of(lhs));
    } else {
      let arg = constructorArg(isn);
      if (arg)
        warning(pretty_var(arg) + " defined by call to constructor " + pretty_func(decl) +
                " during static initialization", location_of(arg));
      else
        warning(pretty_func(decl) + " called during static initialization", location_of(decl));
    }
  }
}

function constructorArg(call) {
  let decl = gimple_call_fndecl(call);

  if (!DECL_CONSTRUCTOR_P(decl))
    return null;

  let arg = gimple_call_arg_iterator(call).next();
  if (TYPE_MAIN_VARIANT(TREE_TYPE(TREE_TYPE(arg))) != DECL_CONTEXT(decl))
    throw new Error("malformed constructor call?!");

  return arg.tree_code() == ADDR_EXPR ? TREE_OPERAND(arg, 0) : arg;
}

function pretty_func(fn) {
  return rfunc_string(rectify_function_decl(fn));
}

function pretty_var(v) {
  return type_string(TREE_TYPE(v)) + " " + expr_display(v);
}
