#!/bin/sh
#
# I developed this script as I tried to study up on autotools. 
# Whether it is true or not, I believe that only the human
# generated parts of an autotools project should be checked into
# the source code control system. This script automates the process
# of turning a human generated project skeleton into something you
# can configure/make/make install...
# 
# Here is an example project skeleton:
#
#  ./prepare.sh
#  ./Makefile.am
#  ./configure.additional
#  ./README
#  ./ChangeLog
#  ./NEWS
#  ./AUTHORS
#  ./d.dav
#  ./d.dav/mod_dav_orangefs.c
#  ./d.dav/Makefile.am
#  ./d.s3
#  ./d.s3/mod_orangefs_s3.c
#  ./d.s3/Makefile.am
#
# Autostuff gets indignant if a project doesn't include NEWS, README, 
# AUTHORS and ChangeLog files, so they should probably exist and 
# contain meaningful content, but this script will touch them into 
# existence if needed.
#
# Only developers will ever see or use this script, when the developer
# thinks the project is done, "Make dist" will produce a blob.tar.gz
# to distribute.
#
# If you add new human generated files to the project, it probably works 
# to just re-run this script instead of running autoreconf or buildconf. 
# Or the developer could just get a fresh copy of the project skeleton, 
# add the new files to that, and run this script. I generally do the latter,
# it is comforting to see the raw skeleton get processed from scratch into 
# a working build system before you check the new files into the source 
# code control system. However you do it, don't let any of your human
# generated files get lost in the sea of autotools generated files without
# checking them in.
# 
# buildconf (http://buildconf.brlcad.org/) is a much more complex,
# and maybe better and/or more powerful, tool that can be used to 
# process an autotools project skeleton.
# 
touch configure.ac
autoscan
cp configure.scan configure.ac
sed -i 's/^AC_INIT.*$/AC_INIT([orangefs acl tool],[2],[hubcap@clemson.edu])/' configure.ac
sed -i '/^AC_OUTPUT$/d' configure.ac
cat configure.additional >> configure.ac
echo AC_OUTPUT >> configure.ac
libtoolize
aclocal
autoheader
touch NEWS README AUTHORS COPYING ChangeLog
automake -ac
autoconf
