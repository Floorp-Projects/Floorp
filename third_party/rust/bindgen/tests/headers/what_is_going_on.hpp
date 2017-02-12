
struct UnknownUnits {};
typedef float Float;

template<class units, class F = Float>
struct PointTyped {
  F x;
  F y;

  static PointTyped<units, F> FromUnknownPoint(const PointTyped<UnknownUnits, F>& aPoint) {
    return PointTyped<units, F>(aPoint.x, aPoint.y);
  }

  PointTyped<UnknownUnits, F> ToUnknownPoint() const {
    return PointTyped<UnknownUnits, F>(this->x, this->y);
  }
};

typedef PointTyped<UnknownUnits> IntPoint;
