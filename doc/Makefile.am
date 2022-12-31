man_MANS = gob2.1
SUBDIRS = .

EXTRA_DIST = gob2.1.in makehtml.pl

html: gob2.1
	perl $(top_srcdir)/doc/makehtml.pl < $(top_srcdir)/doc/gob2.1 > $(top_builddir)/doc/gob2.1.html
