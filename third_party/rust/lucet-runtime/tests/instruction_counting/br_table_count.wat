(module
  (func $main (export "test_function")
    block $a
      i64.const 1
      drop

      block $b
        i64.const 2
        drop

        block $c
          i64.const 3
          drop

          ;; 3 for above consts, one for i32.const below, 2 for br_table
          ;; totalling to an expected count of 6
          (br_table 0 1 2 3 (i32.const 4))
        end
      end
    end
  )
  (func $instruction_count (export "instruction_count") (result i64)
    i64.const 6
  )
)
