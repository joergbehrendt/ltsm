SUBDIRS = lib test
AUTOMAKE_OPTIONS = subdir-objects

ltsmc_CFLAGS = -m64 -DLINUX_CLIENT -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -I$(top_srcdir)/src/lib -I@TSM_SRC_DIR@/
bin_PROGRAMS = ltsmc
ltsmc_SOURCES = ltsmc.c
ltsmc_LDADD = $(top_srcdir)/src/lib/libtsmapi.a

ltsmpipe_CFLAGS = -m64 -DLINUX_CLIENT -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -I$(top_srcdir)/src/lib -I@TSM_SRC_DIR@/
bin_PROGRAMS += ltsmpipe
ltsmpipe_SOURCES = ltsmpipe.c
ltsmpipe_LDADD = $(top_srcdir)/src/lib/libtsmapi.a


if HAVE_LUSTRE
    ltsmc_CFLAGS += -I@LUSTRE_SRC_DIR@/lustre/include
    lhsmtool_tsm_CFLAGS = -I@LUSTRE_SRC_DIR@/libcfs/include -I@LUSTRE_SRC_DIR@/lustre/include -I@LUSTRE_SRC_DIR@/lnet/include -I@TSM_SRC_DIR@/ -I$(top_srcdir)/src/lib
    bin_PROGRAMS += lhsmtool_tsm
    lhsmtool_tsm_SOURCES = lhsmtool_tsm.c
    lhsmtool_tsm_LDADD = $(top_srcdir)/src/lib/libtsmapi.a
endif
