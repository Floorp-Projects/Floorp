// bindgen-flags: --whitelist-type Rooted -- -std=c++14

template <typename a> using MaybeWrapped = a;
class Rooted {
  MaybeWrapped<int> ptr;
};

/// <div rustbindgen replaces="MaybeWrapped"></div>
template <typename a> using replaces_MaybeWrapped = a;
