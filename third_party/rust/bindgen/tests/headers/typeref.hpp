struct nsFoo;

namespace mozilla {

struct FragmentOrURL { bool mIsLocalRef; };
struct Position { };

} // namespace mozilla

class Bar {
  nsFoo* mFoo;
};

namespace mozilla {

template<typename ReferenceBox>
struct StyleShapeSource {
  union {
    Position* mPosition;
    FragmentOrURL* mFragmentOrURL;
  };
};

} // namespace mozilla

struct nsFoo {
  mozilla::StyleShapeSource<int> mBar;
};
