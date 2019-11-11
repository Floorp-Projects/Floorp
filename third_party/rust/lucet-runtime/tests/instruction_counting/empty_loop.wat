(module
  (func $main (export "test_function") (local $i i32)
    loop $inner
    end
  )
  (func $instruction_count (export "instruction_count") (result i64)
    i64.const 0
  )
)
