(module
  (func $main (export "test_function") (local $i i32)
    i32.const 0
    set_local $i
    block $outer
      loop $inner
        ;; each loop iteration is:
        ;; * 4 operations to increment i
        ;; * 3 operations to test i == 10
        ;; * 1 branch to break (untaken)
        ;; * 1 branch to loop
        get_local $i
        i32.const 1
        i32.add
        set_local $i
        get_local $i
        i32.const 10
        i32.eq
        br_if $outer
        br $inner
      end
    end
    ;; iterating i = 0..9, that's 10 * 8 instructions from full executions,
    ;; plus 9 instructions from the last round.
    ;; add two for initializing i and that gives 80 + 9 + 2 = 91 instructions
  )
  (func $instruction_count (export "instruction_count") (result i64)
    i64.const 91
  )
)
