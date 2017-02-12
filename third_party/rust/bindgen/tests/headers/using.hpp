// bindgen-flags: -- -std=c++11

template<typename T>
class Point {
    T x;
    T y;
};

typedef Point<int> IntPoint2D;

using IntVec2D = Point<int>;
