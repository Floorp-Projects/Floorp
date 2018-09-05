// Iterator returning by value.
template <typename T>
struct Iterator {
  void operator++();
  T operator*();
  bool operator!=(const Iterator& other);
};

// The template argument is an iterator type, and a view is an object you can
// run a for loop on.
template <typename T>
struct View {
  T begin();
  T end();
};

// With this class, the implicit conversion is a call to the (implicit)
// constructor of the class.
template <typename T>
class ImplicitWrapper {
 public:
  // Implicit!
  ImplicitWrapper(const T& t);
};

template <typename T>
class OperatorWrapper {
 public:
  OperatorWrapper() = delete;
};

struct SimpleClass {
  int foo;
  operator OperatorWrapper<SimpleClass>();
};

typedef View<Iterator<SimpleClass>> SimpleView;

void ImplicitSimpleClassIterator() {
  for (const ImplicitWrapper<SimpleClass>& foo : SimpleView()) {}
  for (const ImplicitWrapper<SimpleClass> foo : SimpleView()) {}
  for (ImplicitWrapper<SimpleClass> foo : SimpleView()) {}
}
