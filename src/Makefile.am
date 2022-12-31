#YACCFLAGS += -d -t
AM_YFLAGS = -d -t
SUBDIRS = .

#GCC specific, should not be in distributed files
#AM_CFLAGS = \
#	-Wall			\
#	-Wpointer-arith		\
#	-Wmissing-prototypes	\
#	-Wmissing-declarations

DEFS = @DEFS@ \
	-DPKGDATADIR=\"$(pkgdatadir)\"

INCLUDES = \
	$(GLIB_CFLAGS)		\
	-I$(top_builddir)	\
	-I$(top_builddir)/src	\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/src	\
	-I$(includedir)

if NOINSTGOB
noinst_PROGRAMS = gob2
else
bin_PROGRAMS = gob2
endif
EXTRA_PROGRAMS = gob2

BUILT_SOURCES = parse.h parse.c lexer.c

gob2_SOURCES = 	\
	main.c		\
	main.h		\
	treefuncs.c	\
	treefuncs.h	\
	out.c		\
	out.h		\
	util.c		\
	util.h		\
	checks.c	\
	checks.h	\
	parse.y		\
	lexer.l

gob2_LDADD = \
	-lm \
	$(GLIB_LIBS) \
	@LEXLIB@

# we might want popt back in the future
#	@POPT_LIB@

CLEANFILES = @CLEANFILES@ $(BUILT_SOURCES) test-object.* test-object-private.h

EXTRA_DIST = $(BUILT_SOURCES) test.gob generate_treefuncs.pl treefuncs.def str.gob str_test.c Makefile.str

# parse.h parse.c: parse.y
# 	$(YACC) $(YFLAGS) $<
# 	test -f y.tab.c && mv -f y.tab.c parse.c
# 	test -f y.tab.h && mv -f y.tab.h parse.h

test: test-object.c test-object.h
	$(CC) -g -O -W -Wall -Wunused -Wuninitialized -Wdeclaration-after-statement -Wshadow -Wwrite-strings -Wunreachable-code -Wbad-function-cast `pkg-config --cflags gobject-2.0 gtk+-2.0` -c test-object.c

test-object.c test-object.h: test.gob gob2
	./gob2 test.gob

# can't depend on the header file as that would break the above rule
# since it's just for testing, who cares
testcpp: test-object.cc
	g++ -g -O -W -Wall -Wunused -Wuninitialized  -Wwrite-strings -Wunreachable-code `pkg-config --cflags gobject-2.0 gtk+-2.0` -c test-object.cc

test-object.cc: test.gob gob2
	./gob2 --for-cpp test.gob
