CFLAGS=-Wall -Wextra -W -O0 -g -pipe -Wno-unused-parameter `pkg-config --cflags dbus-glib-1 glib-2.0 gtk+-2.0 xtst avahi-glib avahi-client avahi-ui libnotify libglade-2.0`
LIBS=`pkg-config --libs dbus-glib-1 glib-2.0 gtk+-2.0 xtst avahi-glib avahi-client avahi-ui libnotify libglade-2.0`

mango-lassi: lassi-server.o lassi-grab.o lassi-osd.o lassi-order.o lassi-clipboard.o lassi-avahi.o lassi-tray.o lassi-prefs.o *.h
	$(CC) $^ -o $@ $(LIBS) $(CFLAGS)

clean:
	rm -f *.o mango-lassi
