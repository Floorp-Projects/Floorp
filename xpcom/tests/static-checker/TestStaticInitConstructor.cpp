struct Blah {
  public:
    Blah() { }
   ~Blah() { } // raises call to __cxa_atexit
};

Blah b;
