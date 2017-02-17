// bindgen-flags: -- -std=c++14

template <typename Elem>
class List {
    List<Elem> *next;
};

template <typename GcThing>
class PersistentRooted {
    List<PersistentRooted<GcThing>> root_list;
};
