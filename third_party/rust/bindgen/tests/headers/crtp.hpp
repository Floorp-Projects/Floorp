template<class T>
class Base {};

class Derived : public Base<Derived> {};

template<class T>
class BaseWithDestructor {
  ~BaseWithDestructor();
};

class DerivedFromBaseWithDestructor :
  public BaseWithDestructor<DerivedFromBaseWithDestructor> {};
