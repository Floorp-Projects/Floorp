// bindgen-flags: -- -std=c++11
template<typename T>
class RandomTemplate;

template<int i>
class Wat;

template<int i>
class Wat3;

template<>
class Wat3<3>;

/** <div rustbindgen opaque></div> */
typedef RandomTemplate<int> ShouldBeOpaque;

typedef RandomTemplate<float> ShouldNotBeOpaque;
