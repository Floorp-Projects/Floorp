(module
  (func $mul2 (param $in i64) (result i64)
    get_local $in
    get_local $in
    i64.mul
  )
  (func $main (export "test_function")
    i64.const 1
    call $mul2 ;; one from the call, three from the called function, one for the return
    drop
    i64.const 2
    call $mul2 ;; and again
    drop
    i64.const 3
    call $mul2 ;; and again
    ;; for a total of 3 * 6 == 18 instructions
    drop
  )
  (func $instruction_count (export "instruction_count") (result i64)
    i64.const 18
  )
)
