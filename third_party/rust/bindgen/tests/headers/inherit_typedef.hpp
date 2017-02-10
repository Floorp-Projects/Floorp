struct Foo {};

typedef Foo TypedefedFoo;

struct Bar: public TypedefedFoo {};
