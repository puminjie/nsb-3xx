include $(top_srcdir)/Make_global.am

AUTOMAKE_OPTIONS = foreign dist-bzip2 1.6

ACLOCAL_AMFLAGS = -I m4

SUBDIRS	= include src utils qa

man_MANS = #nfnetlink_conntrack.3 nfnetlink_conntrack.7

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libnetfilter_conntrack.pc

EXTRA_DIST = Make_global.am

$(OBJECTS): libtool
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
