// bindgen-flags: -- -std=c++11

class A {
    unsigned c;
    struct Segment { int begin, end; };
    union {
        int f;
    } named_union;
    union {
        int d;
    };
};

class B {
    unsigned d;
    struct Segment { int begin, end; };
};


enum class StepSyntax {
    Keyword,                     // step-start and step-end
    FunctionalWithoutKeyword,    // steps(...)
    FunctionalWithStartKeyword,  // steps(..., start)
    FunctionalWithEndKeyword,    // steps(..., end)
};

class C {
    unsigned d;
    union {
        struct {
            float mX1;
            float mY1;
            float mX2;
            float mY2;
        } mFunc;
        struct {
            StepSyntax mStepSyntax;
            unsigned int mSteps;
        };
    };
    // To ensure it doesn't collide
    struct Segment { int begin, end; };
};
