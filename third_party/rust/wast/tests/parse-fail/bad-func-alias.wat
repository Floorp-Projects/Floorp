(component
    (component $comp)
    (instance $inst (instantiate $comp))
    (func $alias1 (alias export $inst "fun"))
    (alias export $inst "fun" (func $alias2))
    (alias export $inst "fun" (core func $alias3))
)
