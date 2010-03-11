# vim:set ft=automake:

applicationsdir=$(datadir)/applications
nodist_applications_DATA=mango-lassi.desktop

@INTLTOOL_DESKTOP_RULE@

mango-lassi.desktop.in: $(srcdir)/data/mango-lassi.desktop.in.in
	$(AM_V_GEN) sed 's*@BINDIR@*$(bindir)*' $< > $@

CLEANFILES+=$(nodist_applications_DATA) $(nodist_applications_DATA).in
EXTRA_DIST+=data/mango-lassi.desktop.in.in
