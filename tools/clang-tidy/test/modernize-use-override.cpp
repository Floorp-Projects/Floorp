class Base {
public:
  virtual void foo() = 0;
};

class Deriv : public Base {
  void foo();
};