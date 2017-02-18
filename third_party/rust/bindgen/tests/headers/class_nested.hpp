class A {
public:
    int member_a;
    class B {
        int member_b;
    };

    class C;

    template<typename T>
    class D {
      T foo;
    };
};

class A::C {
  int baz;
};

A::B var;
A::D<int> baz;

class D {
    A::B member;
};

template<typename T>
class Templated {
    T member;

    class Templated_inner {
    public:
        T* member_ptr;
        void get() {}
    };
};
