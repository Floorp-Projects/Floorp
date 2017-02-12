
class TestOverload {
  // This one shouldnt' be generated.
  TestOverload();
public:
  TestOverload(int);
  TestOverload(double);
};

class TestPublicNoArgs {
public:
  TestPublicNoArgs();
};
