lib_LTLIBRARIES         = libapplefile.la
include_HEADERS         = applefile.h af-graphics.h
libapplefile_la_SOURCES = applefile.c resource.c			     \
		          ff-applesingledouble.c ff-macbinary.c ff-hexbin.c  \
                          crc.c						     \
			  graphics.c clut.c				     \
			  rt-cicn.c rt-ppat.c rt-icons.c
libapplefile_la_LDFLAGS = -version-info 1:0:0
noinst_PROGRAMS         = make_crc_table
make_crc_table_SOURCES  = make_crc_table.c

BUILT_SOURCES           = crc.c
noinst_HEADERS          = applefile_int.h crc.h resource.h

# do not distribute the format documentation without making sure the
# licensing is OK
# EXTRA_DIST            = AppleSingle_AppleDouble.pdf IconSvcs.pdf \
#                         XMODEM-CRC.NOTE.1 macbinary.html macbinary_ii.html \
#                         macbinary_iii.html rfc1740.html rfc1741.html

crc.c: make_crc_table
	./make_crc_table > crc.c
