(module
  (type (struct))
  (type (struct (field i32)))
  (type (struct (field (mut i32))))
  (type (struct (field i32) (field i32)))
  (type (struct (field i32) (field (mut i32))))
  (type (struct (field (mut i32)) (field (mut i32))))

  (type $a (struct (field f32)))
  (type $b (struct (field f32)))

  (type (struct (field $field_a anyref)))
  (type (struct (field $field_b anyref) (field $field_c funcref)))

  (func
    struct.new $a
    struct.get $a $field_a
    struct.set $b $field_c
  )

  (func
    struct.narrow i32 f32
    struct.narrow anyref funcref
    struct.narrow anyref nullref
  )
)
