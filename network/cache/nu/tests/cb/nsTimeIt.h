/* A class to time excursion events */
/* declare an object of this class within the scope to be timed. */
#include <prtypes.h>
#include <prinrval.h>

class nsTimeIt
{
public:
    nsTimeIt(PRUint32& tmp);
    ~nsTimeIt();
private:
    PRIntervalTime t;
    PRUint32& dest;
};

inline
nsTimeIt::nsTimeIt(PRUint32& tmp):t(PR_IntervalNow()), dest(tmp)
{
}

inline
nsTimeIt::~nsTimeIt()
{
    dest = PR_IntervalToMicroseconds(PR_IntervalNow()-t);
}