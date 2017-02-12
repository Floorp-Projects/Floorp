class C {
    mutable int m_member;
    int m_other;
};

class NonCopiable {
    mutable int m_member;

    ~NonCopiable() {};
};

class NonCopiableWithNonCopiableMutableMember {
    mutable NonCopiable m_member;
};
