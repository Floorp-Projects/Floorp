#include "nsXULAppAPI.h"
#include "nsBuildID.h"

static const nsXREAppData kAppData =
{
  "Mozilla",
  "Xulrunner",
  APP_VERSION,
  BUILD_ID,
  "Copyright (c) 2004 mozilla.org",
  PR_FALSE
};

int main(int argc, char* argv[])
{
  return xre_main(argc, argv, &kAppData);
}
                                                                                                                                                
#if defined( XP_WIN ) && defined( WIN32 ) && !defined(__GNUC__)
// We need WinMain in order to not be a console app.  This function is
// unused if we are a console application.
int WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR args, int )
{
    // Do the real work.
    return main( __argc, __argv );
}
#endif
