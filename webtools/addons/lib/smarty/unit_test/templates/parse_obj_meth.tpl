{$obj->meth($foo, 2.5)}
{$obj->meth(2.5, $foo)}
{$obj->meth(2.5)}
{$obj->meth($obj->val, "foo")}
{$obj->meth("foo", $obj->val)}
{$obj->meth("foo", $foo)}
{$obj->meth($obj->arr.one, 2)}
{$obj->meth($obj->meth("foo", $foo))}
