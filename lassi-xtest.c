#include <gdk/gdkx.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/xf86dga.h>


int xtest_init(void) {
    int event_base;
    int error_base;
    int major_version;
    int minor_version;

    if (!XTestQueryExtension(GDK_DISPLAY(), &event_base, &error_base, &major_version, &minor_version)) {
        g_warning("XTest extension not supported.");
        return -1;
    }

    g_debug("XTest %u.%u supported.", major_version, minor_version);

    //if (!XDGAQueryExtension(GDK_DI
    
    
    return 0;
}
