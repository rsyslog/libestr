DEBUG = -g
LIBESTR_CFLAGS=-I$(top_srcdir)/include

# Uncomment for debugging TODO: configure option
#CFLAGS += $(DEBUG)

lib_LTLIBRARIES = libestr.la

libestr_la_SOURCES = \
	libestr.c \
	string.c

libestr_la_CPPFLAGS = $(LIBESTR_CFLAGS)
libestr_la_LIBADD = 
libestr_la_LDFLAGS = -version-info 0:0:0

include_HEADERS = 
