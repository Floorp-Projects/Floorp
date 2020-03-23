(assert_malformed (module quote
  "(func $foo)"
  "(func $foo)")
  "duplicate identifier")
(assert_malformed (module quote
  "(import \"\" \"\" (func $foo))"
  "(func $foo)")
  "duplicate identifier")
(assert_malformed (module quote
  "(import \"\" \"\" (func $foo))"
  "(import \"\" \"\" (func $foo))")
  "duplicate identifier")

(assert_malformed (module quote
  "(global $foo i32 (i32.const 0))"
  "(global $foo i32 (i32.const 0))")
  "duplicate identifier")
(assert_malformed (module quote
  "(import \"\" \"\" (global $foo i32))"
  "(global $foo i32 (i32.const 0))")
  "duplicate identifier")
(assert_malformed (module quote
  "(import \"\" \"\" (global $foo i32))"
  "(import \"\" \"\" (global $foo i32))")
  "duplicate identifier")

(assert_malformed (module quote
  "(memory $foo 1)"
  "(memory $foo 1)")
  "duplicate identifier")
(assert_malformed (module quote
  "(import \"\" \"\" (memory $foo 1))"
  "(memory $foo 1)")
  "duplicate identifier")
(assert_malformed (module quote
  "(import \"\" \"\" (memory $foo 1))"
  "(import \"\" \"\" (memory $foo 1))")
  "duplicate identifier")

(assert_malformed (module quote
  "(table $foo 1 funcref)"
  "(table $foo 1 funcref)")
  "duplicate identifier")
(assert_malformed (module quote
  "(import \"\" \"\" (table $foo 1 funcref))"
  "(table $foo 1 funcref)")
  "duplicate identifier")
(assert_malformed (module quote
  "(import \"\" \"\" (table $foo 1 funcref))"
  "(import \"\" \"\" (table $foo 1 funcref))")
  "duplicate identifier")

(assert_malformed (module quote "(func (param $foo i32) (param $foo i32))")
  "duplicate identifier")
(assert_malformed (module quote "(func (param $foo i32) (local $foo i32))")
  "duplicate identifier")
(assert_malformed (module quote "(func (local $foo i32) (local $foo i32))")
  "duplicate identifier")
