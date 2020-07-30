(module
  (func $main (export "test_function")
    block $ops
      i32.const 11
      i32.const 10
      i32.add

      br 0 ;; branch to enclosing scope (end of this block)
      ;; at this point we've counted four operations...
    end

    i32.const 14
    i32.const 15
    i32.add
    ;; this puts us at 7 operations
    drop
  )
  (func $instruction_count (export "instruction_count") (result i64)
    i64.const 7
  )
)
