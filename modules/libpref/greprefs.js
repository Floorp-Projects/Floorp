#include init/all.js
#include ../../devtools/shared/preferences/devtools-shared.js
#ifdef MOZ_DATA_REPORTING
#include ../../toolkit/components/telemetry/datareporting-prefs.js
#endif
#ifdef MOZ_SERVICES_HEALTHREPORT
#if MOZ_WIDGET_TOOLKIT != android
#include ../../toolkit/components/telemetry/healthreport-prefs.js
#endif
#endif
