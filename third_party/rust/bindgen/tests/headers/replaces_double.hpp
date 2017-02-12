// bindgen-flags: --blacklist-type Wrapper -- --std=c++11

template<typename T>
struct Wrapper {
    struct Wrapped {
        T t;
    };
    using Type = Wrapped;
};

template<typename T>
class Rooted {
    using MaybeWrapped = typename Wrapper<T>::Type;
    MaybeWrapped ptr;

    /**
     * <div rustbindgen replaces="Rooted_MaybeWrapped"></div>
     */
    using MaybeWrapped_simple = T;
};
