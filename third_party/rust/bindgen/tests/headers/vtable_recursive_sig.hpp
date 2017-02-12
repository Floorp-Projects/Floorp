// bindgen-flags: -- -std=c++11

class Derived;
class Base {
public:
  virtual Derived* AsDerived() { return nullptr; }
};

class Derived final : public Base {
  virtual Derived* AsDerived() override { return this; }
};
