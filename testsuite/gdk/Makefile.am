include $(top_srcdir)/Makefile.decl

NULL=

noinst_PROGRAMS = $(TEST_PROGS)

AM_CPPFLAGS = 				\
	$(GDK_DEP_CFLAGS) 		\
	-I$(top_srcdir) 		\
	-I$(top_builddir)/gdk 		\
	$(NULL)

LDADD = 				\
	$(GDK_DEP_LIBS) 		\
	$(top_builddir)/gdk/libgdk-3.la \
	$(NULL)

#TEST_PROGS              += check-gdk-cairo

TEST_PROGS += 				\
	rgba				\
	encoding			\
	display				\
	keysyms				\
	$(NULL)

CLEANFILES = 			\
	cairosurface.png	\
	gdksurface.png		\
	$(NULL)

if BUILDOPT_INSTALL_TESTS
insttestdir=$(pkglibexecdir)/installed-tests
insttest_PROGRAMS = $(TEST_PROGS)

%.test: %$(EXEEXT) Makefile
	$(AM_V_GEN) (echo '[Test]' > $@.tmp; \
	echo 'Type=session' >> $@.tmp; \
	echo 'Exec=$(insttestdir)/$<' >> $@.tmp; \
	mv $@.tmp $@)

test_files = $(TEST_PROGRS:=.test)

testmetadir = $(datadir)/installed-tests/$(PACKAGE)
testmeta_DATA = $(test_files)
endif

-include $(top_srcdir)/git.mk
