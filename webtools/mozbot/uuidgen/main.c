/* copyright?  hah!  it's 10 lines of code! */

#include <stdio.h>
#include "token.h"

int main(int argc, char **argv) {
  uuid_state state;
  uuid_t uuid;
  char output[1024];

  create_uuid_state(&state);
  create_token(&state, &uuid);
  format_token(output, &uuid);

  printf("%s\n", output);
  
}
