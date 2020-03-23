(module
 (type $FUNCSIG$i (func (result i32)))
 (type $FUNCSIG$vi (func (param i32)))
 (type $FUNCSIG$vj (func (param i64)))
 (type $FUNCSIG$vf (func (param f32)))
 (type $FUNCSIG$vd (func (param f64)))
 (type $FUNCSIG$j (func (result i64)))
 (import "fuzzing-support" "log-i32" (func $log-i32 (param i32)))
 (import "fuzzing-support" "log-i64" (func $log-i64 (param i64)))
 (import "fuzzing-support" "log-f32" (func $log-f32 (param f32)))
 (import "fuzzing-support" "log-f64" (func $log-f64 (param f64)))
 (memory $0 1 1)
 (data (i32.const 0) "\00\00\00\00")
 (table $0 0 0 funcref)
 (global $global$0 (mut i32) (i32.const -65536))
 (global $global$1 (mut f32) (f32.const 3402823466385288598117041e14))
 (global $global$2 (mut f32) (f32.const -241640626995699031081859e12))
 (global $global$3 (mut f64) (f64.const -nan:0xfffffffffac1b))
 (global $hangLimit (mut i32) (i32.const 10))
 (export "hashMemory" (func $hashMemory))
 (export "memory" (memory $0))
 (export "func_5" (func $func_5))
 (export "hangLimitInitializer" (func $hangLimitInitializer))
 (func $hashMemory (; 4 ;) (type $FUNCSIG$i) (result i32)
  (local $0 i32)
  (local.set $0
   (i32.const 5381)
  )
  (local.set $0
   (i32.xor
    (i32.add
     (i32.shl
      (local.get $0)
      (i32.const 5)
     )
     (local.get $0)
    )
    (i32.load8_u
     (i32.const 0)
    )
   )
  )
  (local.set $0
   (i32.xor
    (i32.add
     (i32.shl
      (local.get $0)
      (i32.const 5)
     )
     (local.get $0)
    )
    (i32.load8_u offset=1
     (i32.const 0)
    )
   )
  )
  (local.set $0
   (i32.xor
    (i32.add
     (i32.shl
      (local.get $0)
      (i32.const 5)
     )
     (local.get $0)
    )
    (i32.load8_u offset=2
     (i32.const 0)
    )
   )
  )
  (local.set $0
   (i32.xor
    (i32.add
     (i32.shl
      (local.get $0)
      (i32.const 5)
     )
     (local.get $0)
    )
    (i32.load8_u offset=3
     (i32.const 0)
    )
   )
  )
  (local.set $0
   (i32.xor
    (i32.add
     (i32.shl
      (local.get $0)
      (i32.const 5)
     )
     (local.get $0)
    )
    (i32.load8_u offset=4
     (i32.const 0)
    )
   )
  )
  (local.set $0
   (i32.xor
    (i32.add
     (i32.shl
      (local.get $0)
      (i32.const 5)
     )
     (local.get $0)
    )
    (i32.load8_u offset=5
     (i32.const 0)
    )
   )
  )
  (local.set $0
   (i32.xor
    (i32.add
     (i32.shl
      (local.get $0)
      (i32.const 5)
     )
     (local.get $0)
    )
    (i32.load8_u offset=6
     (i32.const 0)
    )
   )
  )
  (local.set $0
   (i32.xor
    (i32.add
     (i32.shl
      (local.get $0)
      (i32.const 5)
     )
     (local.get $0)
    )
    (i32.load8_u offset=7
     (i32.const 0)
    )
   )
  )
  (local.set $0
   (i32.xor
    (i32.add
     (i32.shl
      (local.get $0)
      (i32.const 5)
     )
     (local.get $0)
    )
    (i32.load8_u offset=8
     (i32.const 0)
    )
   )
  )
  (local.set $0
   (i32.xor
    (i32.add
     (i32.shl
      (local.get $0)
      (i32.const 5)
     )
     (local.get $0)
    )
    (i32.load8_u offset=9
     (i32.const 0)
    )
   )
  )
  (local.set $0
   (i32.xor
    (i32.add
     (i32.shl
      (local.get $0)
      (i32.const 5)
     )
     (local.get $0)
    )
    (i32.load8_u offset=10
     (i32.const 0)
    )
   )
  )
  (local.set $0
   (i32.xor
    (i32.add
     (i32.shl
      (local.get $0)
      (i32.const 5)
     )
     (local.get $0)
    )
    (i32.load8_u offset=11
     (i32.const 0)
    )
   )
  )
  (local.set $0
   (i32.xor
    (i32.add
     (i32.shl
      (local.get $0)
      (i32.const 5)
     )
     (local.get $0)
    )
    (i32.load8_u offset=12
     (i32.const 0)
    )
   )
  )
  (local.set $0
   (i32.xor
    (i32.add
     (i32.shl
      (local.get $0)
      (i32.const 5)
     )
     (local.get $0)
    )
    (i32.load8_u offset=13
     (i32.const 0)
    )
   )
  )
  (local.set $0
   (i32.xor
    (i32.add
     (i32.shl
      (local.get $0)
      (i32.const 5)
     )
     (local.get $0)
    )
    (i32.load8_u offset=14
     (i32.const 0)
    )
   )
  )
  (local.set $0
   (i32.xor
    (i32.add
     (i32.shl
      (local.get $0)
      (i32.const 5)
     )
     (local.get $0)
    )
    (i32.load8_u offset=15
     (i32.const 0)
    )
   )
  )
  (local.get $0)
 )
 (func $func_5 (; 5 ;) (type $FUNCSIG$j) (result i64)
  (local $0 i32)
  (local $1 i64)
  (block
   (if
    (i32.eqz
     (global.get $hangLimit)
    )
    (return
     (i64.const 4293531749)
    )
   )
   (global.set $hangLimit
    (i32.sub
     (global.get $hangLimit)
     (i32.const 1)
    )
   )
  )
  (block $label$0 (result i64)
   (nop)
   (local.tee $1
    (block $label$1 (result i64)
     (local.set $0
      (local.tee $0
       (if (result i32)
        (i32.const -2147483647)
        (local.get $0)
        (block $label$4 (result i32)
         (br_if $label$4
          (if (result i32)
           (block $label$5 (result i32)
            (call $log-f32
             (if (result f32)
              (i32.eqz
               (loop $label$6 (result i32)
                (block
                 (if
                  (i32.eqz
                   (global.get $hangLimit)
                  )
                  (return
                   (i64.const -32766)
                  )
                 )
                 (global.set $hangLimit
                  (i32.sub
                   (global.get $hangLimit)
                   (i32.const 1)
                  )
                 )
                )
                (block (result i32)
                 (nop)
                 (br_if $label$6
                  (i32.eqz
                   (i32.const 8388608)
                  )
                 )
                 (local.get $0)
                )
               )
              )
              (block $label$7 (result f32)
               (f32.const -nan:0x7fffda)
              )
              (f32.const 4294967296)
             )
            )
            (i32.const -2147483648)
           )
           (i32.const -1073741824)
           (local.get $0)
          )
          (i32.eqz
           (global.get $global$0)
          )
         )
        )
       )
      )
     )
     (i64.const 255)
    )
   )
  )
 )
 (func $hangLimitInitializer (; 6 ;)
  (global.set $hangLimit
   (i32.const 10)
  )
 )
)
