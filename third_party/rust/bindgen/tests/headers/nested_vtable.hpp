class nsISupports {
public:
  virtual nsISupports* QueryInterface();
};

class nsIRunnable : public nsISupports {};

class Runnable : public nsIRunnable {};
