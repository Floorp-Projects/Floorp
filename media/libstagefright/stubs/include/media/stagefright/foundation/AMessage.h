/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

#ifndef A_MESSAGE_H_
#define A_MESSAGE_H_

#include <utils/RefBase.h>

namespace stagefright {

struct AMessage : public RefBase {
public:
    void post() {}
};

}

#endif
