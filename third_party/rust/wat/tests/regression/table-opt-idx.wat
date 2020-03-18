(; table indices are optional for backwards compatibility ;)
(module
  (table 1 funcref)
  (elem $a $f)
  (func $f)
  (func
    i32.const 0
    table.get
    drop)
  (func
    i32.const 0
    ref.func $f
    table.set)
  (func
    table.size
    drop)
  (func
    ref.func $f
    i32.const 0
    table.grow
    drop)
  (func
    i32.const 0
    ref.func $f
    i32.const 0
    table.fill)
  (func
    i32.const 0
    i32.const 0
    i32.const 0
    table.copy)
  (func
    i32.const 0
    i32.const 0
    i32.const 0
    table.init $a)
)
