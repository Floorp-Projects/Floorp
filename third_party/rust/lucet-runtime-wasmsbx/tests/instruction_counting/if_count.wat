(module
  (func $main (export "test_function")
    i32.const 11
    i32.const 10
    i32.gt_s
    ;; this counts up to 3
    (if ;; this makes 4
      (then
        i64.const 5
        i64.const 10
        i64.mul
        ;; and these were another 3
        drop
        ;; drop is ignored for a total of 7 operations
      )
    )
  )
  (func $instruction_count (export "instruction_count") (result i64)
    i64.const 7
  )
)
