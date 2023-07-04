(component
  (core module $mod (func (export "fun")))
  (core instance $inst (instantiate $mod))
  (alias core export $inst "fun" (core func $fun1))
  (core func $fun2 (alias core export $inst "fun"))
  (core func $fun3 (alias export $inst "fun"))
)
