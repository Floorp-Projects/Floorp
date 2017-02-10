
class Calc {
    int w;
};

class Test {
public:
    struct Size;
    friend struct Size;
    struct Size {
        struct Dimension : public Calc {
        };
        Dimension mWidth, mHeight;
    };
};
