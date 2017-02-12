
#define NSID_LENGTH 10
class nsID {
public:
  virtual void ToProvidedString(char (&aDest)[NSID_LENGTH]) = 0;
};
