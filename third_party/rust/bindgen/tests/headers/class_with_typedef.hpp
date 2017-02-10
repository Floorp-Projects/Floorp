typedef int AnotherInt;

class C {
public:
    typedef int MyInt;
    typedef const char* Lookup;
    MyInt c;
    MyInt* ptr;
    MyInt arr[10];
    AnotherInt d;
    AnotherInt* other_ptr;

    void method(MyInt c);
    void methodRef(MyInt& c);
    void complexMethodRef(Lookup& c);
    void anotherMethod(AnotherInt c);
};

class D: public C {
public:
    MyInt* ptr;
};
