template<typename T, unsigned long N>
class Array {
    T inner[N];
};

class C {
    Array<int, 3> arr;
};
