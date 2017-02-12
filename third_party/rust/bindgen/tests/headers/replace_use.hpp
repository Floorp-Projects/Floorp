template<typename T>
struct nsTArray {
  int x;
};
/**
 * <div rustbindgen replaces="nsTArray"></div>
 */
template<typename T>
struct nsTArray_Simple {
  unsigned int y;
};

struct Test {
  nsTArray<long> a;
};
