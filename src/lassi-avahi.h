#ifndef foolassiavahihfoo
#define foolassiavahihfoo

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-client/lookup.h>
#include <avahi-glib/glib-watch.h>

typedef struct LassiAvahiInfo LassiAvahiInfo;
struct LassiServer;

struct LassiAvahiInfo {
    struct LassiServer *server;

    AvahiGLibPoll *poll;
    AvahiClient *client;

    AvahiEntryGroup *group;
    char *service_name;

    AvahiServiceBrowser *browser;
};

#include "lassi-server.h"

int lassi_avahi_init(LassiAvahiInfo *i, LassiServer *server);
void lassi_avahi_done(LassiAvahiInfo *i);

#define LASSI_SERVICE_TYPE "_mango-lassi._tcp"

#endif
