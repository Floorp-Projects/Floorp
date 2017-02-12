struct Foo_empty {};
struct Foo;

struct Bar {
    Foo *f;
};

void baz_struct(Foo* f);

union Union;

void baz_union(Union* u);

class Quux;

void baz_class(Quux* q);
