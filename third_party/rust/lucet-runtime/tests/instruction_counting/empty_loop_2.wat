(module
  (func $main (export "test_function") (local $i i32)
    block $a
      loop $inner
      end
    end
  )
  (func $instruction_count (export "instruction_count") (result i64)
    i64.const 0
  )
)
