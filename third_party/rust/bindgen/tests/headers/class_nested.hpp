class A {
public:
    int member_a;
    class B {
        int member_b;
    };
};

A::B var;

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
