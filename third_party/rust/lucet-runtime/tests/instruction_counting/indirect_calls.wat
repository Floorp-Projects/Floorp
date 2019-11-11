(module
  (type $mulfn (func (param i64) (result i64)))

  ;; mul2 is 3 operations
  (func $mul2 (param $in i64) (result i64)
    get_local $in
    get_local $in
    i64.add
  )
  ;; mul4 is 2 * |mul2| + 2 call + 3 == 13
  (func $mul4 (param $in i64) (result i64)
    get_local $in
    call $mul2
    get_local $in
    call $mul2
    i64.add
  )
  ;; mul8 is 2 * |mul4| + 2 call + 3 == 33
  (func $mul8 (param $in i64) (result i64)
    get_local $in
    call $mul4
    get_local $in
    call $mul4
    i64.add
  )
  ;; mul16 is 2 * |mul8| + 2 call + 3 == 73
  ;; by entire accident the number of instructions for 
  ;; subsequent similar functions for higher powers is given by
  ;; mul(n^2) == 10 * (2 ^ (n - 1)) - 10 + 3
  (func $mul16 (param $in i64) (result i64)
    get_local $in
    call $mul8
    get_local $in
    call $mul8
    i64.add
  )

  (table anyfunc
    (elem
      $mul2 $mul4 $mul8 $mul16
    )
  )

  (func $main (export "test_function")
    ;; call_indirect here is 2, plus 1 for the const, one for the index, and
    ;; 1 for return
    ;; the called function (mul2) is 3 instructions, for 8 total.
    (call_indirect (type $mulfn) (i64.const 0) (i32.const 0))
    drop

    ;; call_indirect here is 2, plus 1 for the const, one for the index, and
    ;; 1 for return
    ;; the called function (mul4) is 13 instructions, for 18 total.
    (call_indirect (type $mulfn) (i64.const 1) (i32.const 1))
    drop

    ;; call_indirect here is 2, plus 1 for the const, one for the index, and
    ;; 1 for return
    ;; the called function (mul16) is 73 instructions, for 78 total.
    (call_indirect (type $mulfn) (i64.const 2) (i32.const 3))
    drop

    ;; for a total of 8 + 18 + 78 == 104 instructions
  )
  (func $instruction_count (export "instruction_count") (result i64)
    i64.const 104
  )
)
