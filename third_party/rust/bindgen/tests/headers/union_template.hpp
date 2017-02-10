template<typename T>
struct NastyStruct {
  bool mIsSome;
  union {
    void* mFoo;
    unsigned long mDummy;
  } mStorage;

  union {
    short wat;
    int* wut;
  };
};

template<typename T>
union Whatever {
  void* mTPtr;
  int mInt;
};
