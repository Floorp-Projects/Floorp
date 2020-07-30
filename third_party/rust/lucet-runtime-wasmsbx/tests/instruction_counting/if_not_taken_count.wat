(module
  (func $main (export "test_function")
    i32.const 10
    i32.const 11
    i32.gt_s
    ;; this counts up to 3
    (if ;; this makes 4
      ;; but the `then` branch is not taken
      (then
        i64.const 5
        i64.const 10
        i64.mul
        drop
      )
    )
    ;; so we only execute 4 operations
  )
  (func $instruction_count (export "instruction_count") (result i64)
    i64.const 4
  )
)
