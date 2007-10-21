#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include <avahi-common/error.h>
#include <avahi-common/alternative.h>
#include <avahi-glib/glib-malloc.h>

#include "lassi-avahi.h"

/* FIXME: Error and collision handling is suboptimal */

static void resolve_cb(
        AvahiServiceResolver *r,
        AvahiIfIndex interface,
        AvahiProtocol protocol,
        AvahiResolverEvent event,
        const char *name,
        const char *type,
        const char *domain,
        const char *host_name,
        const AvahiAddress *address,
        uint16_t port,
        AvahiStringList *txt,
        AvahiLookupResultFlags flags,
        void* userdata) {

    LassiAvahiInfo *i = userdata;

    g_assert(r);
    g_assert(i);

     /* Called whenever a service has been resolved successfully or timed out */

     switch (event) {
         case AVAHI_RESOLVER_FOUND: {
             char a[AVAHI_ADDRESS_STR_MAX], *t;

             avahi_address_snprint(a, sizeof(a), address);
             t = g_strdup_printf("tcp:port=%u,host=%s", port, a);
             lassi_server_connect(i->server,  t);
             g_free(t);
             break;
         }

         case AVAHI_RESOLVER_FAILURE:
             g_message("Failed to resolve service '%s' of type '%s' in domain '%s': %s", name, type, domain, avahi_strerror(avahi_client_errno(i->client)));
             break;

     }

     avahi_service_resolver_free(r);
 }

static void browse_cb(
        AvahiServiceBrowser *b,
        AvahiIfIndex interface,
        AvahiProtocol protocol,
        AvahiBrowserEvent event,
        const char *name,
        const char *type,
        const char *domain,
        AvahiLookupResultFlags flags,
        void* userdata) {

    LassiAvahiInfo *i = userdata;

    g_assert(b);
    g_assert(i);

    switch (event) {
         case AVAHI_BROWSER_NEW:

             if (!(flags & AVAHI_LOOKUP_RESULT_OUR_OWN) &&
                 !lassi_server_is_connected(i->server, name) &&
                 lassi_server_is_known(i->server, name))

                 if (!(avahi_service_resolver_new(i->client, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, resolve_cb, i)))
                     g_message("Failed to resolve service '%s': %s", name, avahi_strerror(avahi_client_errno(i->client)));
             break;

         case AVAHI_BROWSER_REMOVE:
         case AVAHI_BROWSER_ALL_FOR_NOW:
         case AVAHI_BROWSER_CACHE_EXHAUSTED:
             break;

        case AVAHI_BROWSER_FAILURE:
            g_message("Browsing failed: %s", avahi_strerror(avahi_client_errno(i->client)));
            gtk_main_quit();
            break;
    }
}


static void create_service(LassiAvahiInfo *i);

static void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, void *userdata) {
    LassiAvahiInfo *i = userdata;

    g_assert(g);
    g_assert(i);

    i->group = g;

    switch (state) {
        case AVAHI_ENTRY_GROUP_ESTABLISHED :
            g_message("Service '%s' successfully established.", i->service_name);
            break;

        case AVAHI_ENTRY_GROUP_COLLISION : {
            char *n;

            n = avahi_alternative_service_name(i->service_name);
            avahi_free(i->service_name);
            i->service_name = n;

            g_message("Service name collision, renaming service to '%s'", n);

            /* And recreate the services */
            create_service(i);
            break;
        }

        case AVAHI_ENTRY_GROUP_FAILURE :
            g_message("Entry group failure: %s", avahi_strerror(avahi_client_errno(i->client)));
            gtk_main_quit();
            break;

        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
            ;
    }
}

static void create_service(LassiAvahiInfo *i) {
    g_assert(i);

    if (!i->group)
        if (!(i->group = avahi_entry_group_new(i->client, entry_group_callback, i))) {
            g_message("avahi_entry_group_new() failed: %s", avahi_strerror(avahi_client_errno(i->client)));
            gtk_main_quit();
            return;
        }

    if (avahi_entry_group_is_empty(i->group)) {
        int ret;

        for (;;) {

            if (!i->service_name)
                i->service_name = g_strdup(i->server->id);

            if ((ret = avahi_entry_group_add_service(i->group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, i->service_name, LASSI_SERVICE_TYPE, NULL, NULL, i->server->port, NULL)) < 0) {

                if (ret == AVAHI_ERR_COLLISION) {
                    char *n = avahi_alternative_service_name(i->service_name);
                    avahi_free(i->service_name);
                    i->service_name = n;
                    continue;
                }

                g_message("Failed to add service: %s", avahi_strerror(ret));
                gtk_main_quit();
                return;
            }

            break;
        }

        if (strcmp(i->service_name, i->server->id)) {
            g_free(i->server->id);
            i->server->id = g_strdup(i->service_name);
        }

        if ((ret = avahi_entry_group_commit(i->group)) < 0) {
            g_message("Failed to commit entry group: %s", avahi_strerror(ret));
            gtk_main_quit();
            return;
        }
    }
}

static void client_cb(AvahiClient *client, AvahiClientState state, void *userdata) {
    LassiAvahiInfo *i = userdata;

    i->client = client;

    switch (state) {
         case AVAHI_CLIENT_S_RUNNING:
             if (!i->group)
                 create_service(i);
             break;

         case AVAHI_CLIENT_FAILURE:
             g_message("Client failure: %s", avahi_strerror(avahi_client_errno(client)));
             gtk_main_quit();
             break;

         case AVAHI_CLIENT_S_COLLISION:
         case AVAHI_CLIENT_S_REGISTERING:
             if (i->group)
                 avahi_entry_group_reset(i->group);

             break;

         case AVAHI_CLIENT_CONNECTING:
             ;
    }
}

int lassi_avahi_init(LassiAvahiInfo *i, LassiServer *server) {
    int error;

    g_assert(i);
    g_assert(server);

    memset(i, 0, sizeof(*i));
    i->server = server;

    avahi_set_allocator(avahi_glib_allocator());

    if (!(i->poll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT))) {
        g_message("avahi_glib_poll_new() failed.");
        goto fail;
    }

    if (!(i->client = avahi_client_new(avahi_glib_poll_get(i->poll), 0, client_cb, i, &error))) {
        g_message("avahi_client_new() failed: %s", avahi_strerror(error));
        goto fail;
    }

    if (!(i->browser = avahi_service_browser_new(i->client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, LASSI_SERVICE_TYPE, NULL, 0, browse_cb, i))) {
        g_message("avahi_service_browser_new(): %s", avahi_strerror(avahi_client_errno(i->client)));
        goto fail;
    }

    return 0;

fail:
    lassi_avahi_done(i);
    return -1;
}

void lassi_avahi_done(LassiAvahiInfo *i) {
    g_assert(i);

    if (i->client)
        avahi_client_free(i->client);

    if (i->poll)
        avahi_glib_poll_free(i->poll);

    g_free(i->service_name);

    memset(i, 0, sizeof(*i));
}
