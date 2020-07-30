(module
  (func $main (export "test_function")
    i32.const 4
    i32.const -5
    i32.add
    drop
  )
  (func $instruction_count (export "instruction_count") (result i64)
    i64.const 3
  )
)
