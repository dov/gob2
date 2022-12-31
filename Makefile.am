SUBDIRS = @SUBDIRS@
DIST_SUBDIRS = src doc examples

m4dir=$(datadir)/aclocal
m4_DATA=gob2.m4

EXTRA_DIST = gob2.spec.in $(m4_DATA) COPYING.generated-code

dist-hook: gob2.spec
	cp gob2.spec $(distdir)
