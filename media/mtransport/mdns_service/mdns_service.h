#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <new>

struct MDNSService;

extern "C" {

void mdns_service_register_hostname(MDNSService* serv, const char* hostname,
                                    const char* addr);

MDNSService* mdns_service_start(const char* ifaddr);

void mdns_service_stop(MDNSService* serv);

void mdns_service_unregister_hostname(MDNSService* serv, const char* hostname);

const char* mdns_service_generate_uuid();

void mdns_service_free_uuid(const char* uuid);

}  // extern "C"
