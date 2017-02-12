typedef void (*foo)(int);

template<typename T, typename U>
class Foo {
  typedef T Char;
  typedef Char* FooPtrTypedef;
  typedef bool (*nsCOMArrayEnumFunc)(T* aElement, void* aData);
};
