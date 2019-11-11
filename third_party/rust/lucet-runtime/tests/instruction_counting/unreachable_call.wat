(module
  (func $main (export "test_function") (result i64)
    ;; counting the const
    i64.const 1
    ;; return is counted by the caller, so we count 1 so far
    return

    ;; we had a bug where calls in unreachable code would still add
    ;; one to the instruction counter to track a matching return,
    ;; but the call would never be made, so the return would never
    ;; occur, and the count was in error. 
    call $main
  )
  (func $instruction_count (export "instruction_count") (result i64)
    i64.const 1
  )
)
