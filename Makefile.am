#
# -fgnu89-inline
#	quiets "warning: inline function foo declared but never defined"
#
AM_CPPFLAGS = `/opt/trunk/bin/pvfs2-config --cflags` -fgnu89-inline
AM_LDFLAGS = `/opt/trunk/bin/pvfs2-config --libs`
bin_PROGRAMS = srv
srv_SOURCES = srv.c
