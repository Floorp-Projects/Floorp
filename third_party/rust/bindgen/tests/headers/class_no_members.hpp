// bindgen-flags: -- -std=c++11

class whatever {
};

class whatever_child: public whatever {
};

class whatever_child_with_member: public whatever {
public:
    int m_member;
};

static_assert(sizeof(whatever) == 1, "Testing!");
static_assert(sizeof(whatever_child) == 1, "Testing!");
static_assert(sizeof(whatever_child_with_member) == 4, "Testing!");
