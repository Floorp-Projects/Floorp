// bindgen-flags: -- -std=c++11
namespace JS {
template <typename> class PersistentRooted;
}
template <typename> class a { a *b; };
namespace JS {
template <typename c> class PersistentRooted : a<PersistentRooted<c>> {};
}
