// bindgen-flags: --enable-cxx-namespaces

void top_level();

namespace whatever {
    typedef int whatever_int_t;

    void in_whatever();
}

namespace {
    namespace empty {}

    void foo();
    struct A {
        whatever::whatever_int_t b;
    public:
        int lets_hope_this_works();
    };
}

template<typename T>
class C: public A {
    T m_c;
    T* m_c_ptr;
    T m_c_arr[10];
};


template<>
class C<int>;


namespace w {
    typedef unsigned int whatever_int_t;

    template<typename T>
    class D {
        C<T> m_c;
    };

    whatever_int_t heh(); // this should return w::whatever_int_t, and not whatever::whatever_int_t

    C<int> foo();

    C<float> barr(); // <- This is the problematic one
}
