// bindgen-flags: --enable-cxx-namespaces --whitelist-type JSAutoCompartment -- -std=c++11

namespace mozilla {
    template <typename> class a {};
    namespace detail {
        class GuardObjectNotifier {};
        struct b;
    }
    class c {
        typedef detail::b d;
    };
}
namespace js {
    class D {
        mozilla::a<mozilla::c> e;
    };
}
struct f {
    js::D g;
};
namespace js {
    struct ContextFriendFields : f {};
}
class JSAutoCompartment {
public:
    JSAutoCompartment(mozilla::detail::GuardObjectNotifier);
};
