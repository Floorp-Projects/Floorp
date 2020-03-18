(module
  (type $a (struct))
  (type $b (struct))

  (type $f1 (func (param (ref $a))))
  (type $f2 (func (result (ref $b))))

  (global (ref $a))
  (global (ref $b))

  (func (param (ref $a)))
  (func (result (ref $a)))
  (func (local (ref $a)))

  (func
    struct.narrow (ref $a) (ref $b)
    select (result (ref $a))

    (block (param (ref $a)))
    (block (result (ref $b)))
    (block $f1 (param (ref $a)))
    (block $f2 (result (ref $b)))

    (loop (param (ref $a)))
    (loop (result (ref $b)))
    (loop $f1 (param (ref $a)))
    (loop $f2 (result (ref $b)))

    (if (param (ref $a)) (then) (else))
    (if (result (ref $b)) (then) (else))
    (if $f1 (param (ref $a)) (then) (else))
    (if $f2 (result (ref $b)) (then) (else))
  )
)
