#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "applefile.h"
#include "applefile_int.h"
#include "af-graphics.h"

/* Icon resource types handled by this file:
 *
 * ICON   32x32x1 data                  no mask
 * ICN#   32x32x1 data + mask
 * icl4   32x32x4 data                  mask from corresponding ICN#
 * icl8   32x32x8 data                  mask from corresponding ICN#
 * ics#   16x16x1 data + mask
 * ics4   16x16.4 data                  mask from corresponding ics#
 * ics8   16x16x8 data                  mask from corresponding ics#
 * icm#   12x16x1 data + mask
 * icm4   12x16x4 data                  mask from corresponding icm#
 * icm8   12x16x8 data                  mask from corresponding icm#
 * icns   icon family, has subresources of types
 *    ich#, ich4, ich8   48x48x[1,4,8]  data+mask, data, data
 *    ICN#, icl4, icl8   32x32x[1,4,8]  data+mask, data, data
 *    ics#, ics4, icl4   16x16x[1,4,8]  data+mask, data, data
 *    icm#, icm4, icm8   12x16x[1,4,8]  data+mask, data, data
 *    ih32, h8mk         48x48x24       data, alpha
 *    il32, l8mk         32x32x24       data, alpha
 *    is32, s8mk         16x16x24       data, alpha
 *    im32, m8mk         12x16x24       data, alpha
 *
 *
 * Icon resource types NOT handled by this file:
 * cicn   XxYxZ data + mask             generalized color icon type
 * SICN   12x16x1 data  (ever used?)
 *
 * My god...$10 sez Mac OS X will have Yet Another Icon Format
 */

#define SIZE(c)  (c == 'm' ? 0 : (c == 's' ? 1 : (c == 'l' ? 2 : 3)))
#define DEPTH(c) (c == '#' ? 0 : (c == '4' ? 1 : (c == '8' ? 2 : (c == 'a' ? 4 : 3))))

static struct 
{
    char tag;
    int rows;
    int cols;
} sizes[] = {
    { 'm', 12, 16 }, /* mini */
    { 's', 16, 16 }, /* small */
    { 'l', 32, 32 }, /* large */
    { 'h', 48, 48 }, /* huge */
};
#define SIZES 4

/* Most of this info is only valid/necessary for the indexed/masked depths */
static struct
{
    char tag;
    int bpp; /* bits per pixel */
    int ppB; /* pixels per Byte */
    af_clut_t * clut;
} depths[] = {
    { '#', 1, 8, &af_system_clut1, },
    { '4', 4, 2, &af_system_clut4, },
    { '8', 8, 1, &af_system_clut8, },
    { '3', /* '32' */ 24, 0, NULL },
    { 'a', /* 8mk */ 8, 1, NULL }
};
#define DEPTHS 5

struct icns 
{
    unsigned char * data[DEPTHS][SIZES];
};

static af_indexed_pixmap_t *
trad_indexed(unsigned char * data, unsigned char * bw_mask, int depth, int size)
{
    af_indexed_pixmap_t * ind;

    ind = af_int_pmap_indexed(sizes[size].rows,
                              sizes[size].cols,
                              sizes[size].cols/depths[depth].ppB,
                              sizes[size].cols/8,
                              depths[depth].bpp,
                              1,
                              data,
                              bw_mask + (sizes[size].rows*sizes[size].cols/8),
                              depths[depth].clut);
    return ind;
}

static char *
icon_tag(int depth, int size)
{
    static char r[4];
    
    switch (depth) {
	case 0:
	    if (size == 2)
		return "ICN#";
	    /* fallthrough */
	case 1:
	case 2:
	    r[0] = 'i';
	    r[1] = 'c';
	    r[2] = sizes[size].tag;
	    r[3] = depths[depth].tag;
	    return r;
	case 3:
	    r[0] = 'i';
	    r[1] = sizes[size].tag;
	    r[2] = '3';
	    r[3] = '2';
	    return r;
	case 4:
	    r[0] = sizes[size].tag;
	    r[1] = '8';
	    r[2] = 'm';
	    r[3] = 'k';
	    return r;
	default:
	    abort();
    }
}

#define u32(p) af_read_u32(p); p += 4;
#define u16(p) af_read_u16(p); p += 2;

static struct icns *
get_icns(af_file_t * file, int id)
{
    af_res_t *      res;
    struct icns *   icns;
    unsigned char * p;
    unsigned long   len, len_left;
    int             size, depth;
    
    res = af_res(file, "icns", id);
    if (res == NULL) {
        return NULL;
    }

    p = res->pData;
    if (p[0] != 'i' || p[1] != 'c' || p[2] != 'n' || p[3] != 's') {
        return NULL;
    }
    p += 4;

    icns = NEW(struct icns);
    memset(icns, 0, sizeof *icns);
    
    len_left = u32(p);
    len_left -= 8;
    /* chunk tags:
       ic[msh]#		1-bit data and mask [mini, small, huge];
                        data immediately followed by mask
       ICN#             1-bit data and mask, large
                        data immediately followed by mask
       ic[mslh][48]	[4, 8]-bit data, [mini, small, large, huge]
       i[mslh]32        24("32")-bit data, [mini, small, large, huge] - RLE
                        compressed by colorplane
       [smlh]8mk        8-bit alpha-channel ("mask") for use with 24
                        bit icons
    */
    while (len_left) {
	size = -1;
	depth = -1;
	switch (p[0]) {
	    case 'i':
		switch (p[1]) {
		    case 'c':
			/* ic[mslh][#48] */
			size  = SIZE(p[2]);
                        depth = DEPTH(p[3]);
			break;
                    case 'm': case 's': case 'l': case 'h':
			/* i[mslh]32 */
                        size = SIZE(p[1]);
			if (p[2] == '3' && p[3] == '2') {
			    depth = 3;
			}
			break;
		}
		break;
	    case 'I':
		/* ICN# */
		if (p[1] == 'C' && p[2] == 'N' && p[3] == '#') {
		    size = SIZE('l');
		    depth = DEPTH('#');
		}
		break;
	    default:
		/* [sml]8mk */
		size = SIZE(p[0]);
		if (p[1] == '8' && p[2] == 'm' && p[3] == 'k') {
		    depth = DEPTH('a');
		}
		break;
        }
        if (size == -1 || depth == -1) {
            /* log? stderr? */
        } else {
            icns->data[depth][size] = p + 8;
        }
        len       = af_read_u32(p+4);
        len_left -= len;
        p        += len;
    }

    return icns;
}

static af_indexed_pixmap_t *
icns_make_indexed(struct icns * icns, int sizewanted, int maxcolors)
{
    af_indexed_pixmap_t * ret = NULL;
    unsigned char * d, * m;

    m = icns->data[DEPTH('#')][sizewanted];
    if (maxcolors >= 256) {
        d = icns->data[DEPTH('8')][sizewanted];
        if (d && m) {
            ret = trad_indexed(d, m, DEPTH('8'), sizewanted);
            goto end;
        }
    }
    if (maxcolors >= 16) {
        d = icns->data[DEPTH('4')][sizewanted];
        if (d && m) {
            ret = trad_indexed(d, m, DEPTH('4'), sizewanted);
            goto end;
        }
    }
    
    if (m) {
        ret = trad_indexed(m, m, DEPTH('#'), sizewanted);
        goto end;
    }

 end:
    FREE(icns);
    return ret;
}

static unsigned char *
icns_decompress(unsigned char * s, unsigned char * d, size_t bytes)
{
    unsigned char * end;
    unsigned char c;
    unsigned int n, i;

    end = d + bytes;
    while (d < end) {
	if (*s <= 0x7f) {
	    /* A run of literals */
	    n = *s++ + 1;
	    for (i = 0; i < n; i++) {
		*d++ = *s++;
	    }
	} else {
	    /* A solid block */
	    n = *s++ - 125;
	    c = *s++;
	    for (i = 0; i < n; i++) {
		*d++ = c;
	    }
	}
    }

    return s;
}

static af_direct_pixmap_t *
icns_make_direct(struct icns * icns, int sizewanted)
{
    af_direct_pixmap_t * dir;
    unsigned char * a, * d;
    int rows, cols;
    
    a = icns->data[DEPTH('a')][sizewanted];
    d = icns->data[DEPTH('3')][sizewanted];

    if (a && d) {
        rows = sizes[sizewanted].rows;
        cols = sizes[sizewanted].cols;
        
        dir = NEW(af_direct_pixmap_t);
        dir->red   = NEWC(rows * cols, unsigned char);
        dir->green = NEWC(rows * cols, unsigned char);
        dir->blue  = NEWC(rows * cols, unsigned char);
        dir->alpha = NEWC(rows * cols, unsigned char);
        dir->rows = rows;
        dir->cols = cols;

        d = icns_decompress(d, dir->red,   rows * cols);
        d = icns_decompress(d, dir->green, rows * cols);
        d = icns_decompress(d, dir->blue,  rows * cols);

        memcpy(dir->alpha, a, rows * cols);

        return dir;
    }

    return NULL;
}

static af_indexed_pixmap_t *
get_indexed_icns(af_file_t * file, int id, int sizewanted, int maxcolors)
{
    struct icns * icns;
    
    icns = get_icns(file, id);
    if (icns) {
        return icns_make_indexed(icns, sizewanted, maxcolors);
    }
    return NULL;
}

static af_direct_pixmap_t *
get_direct_icns(af_file_t * file, int id, int sizewanted, int mode)
{
    struct icns * icns;
    af_direct_pixmap_t * dir;
    
    icns = get_icns(file, id);

    dir = icns_make_direct(icns, sizewanted);
    if (dir == NULL && mode == 1) {
        af_indexed_pixmap_t * ind;

        ind = icns_make_indexed(icns, sizewanted, 256);
        dir = af_pixmap_indexed_to_direct(ind);
        FREEC(ind->data);
        FREEC(ind->mask);
        FREE(ind);
    }

    FREE(icns);

    return dir;
}

static af_indexed_pixmap_t *
ICON_indexed(af_file_t * file, int id)
{
    af_res_t * res;

    res = af_res(file, "ICON", id);
    if (res == NULL) {
        return NULL;
    }

    return af_int_pmap_indexed(32, 32, 4, 0, 1, 1, res->pData, NULL,
                               &af_system_clut1);
}

/* Get the best possible icon of size <size> (mslh) with
   no more than <color> colors, with icon family id <id> */
af_indexed_pixmap_t *
af_get_indexed_icon(af_file_t * file, int id, char sizetag, int maxcolors,
                    int try_icns)
{
    int size;
    af_indexed_pixmap_t * ind;
    af_res_t * mask_res, * data_res;

    size = SIZE(sizetag);

    if (try_icns) {
        ind = get_indexed_icns(file, id, size, maxcolors);
        if (ind) {
            return ind;
        }
    }
    if (try_icns == 2) {
        return NULL;
    }

    if (size > 2) {
        /* There's no huge (ich[#48]) resource type, only icns subtypes */
        return NULL;
    }
    
    mask_res = af_res(file, icon_tag(DEPTH('#'), size), id);
    if (mask_res == NULL) {
        /* With no mask, the only possibility is an old old ICON resource */
        if (sizetag == 'l') {
            return ICON_indexed(file, id);
        }
        return NULL;
    }

    if (maxcolors >= 256) {
        data_res = af_res(file, icon_tag(DEPTH('8'), size), id);
        if (data_res) {
            return trad_indexed(data_res->pData, mask_res->pData,
                                DEPTH('8'), size);
        }
    }

    if (maxcolors >= 16) {
        data_res = af_res(file, icon_tag(DEPTH('4'), size), id);
        if (data_res) {
            return trad_indexed(data_res->pData, mask_res->pData,
                                DEPTH('4'), size);
        }
    }
    
    return trad_indexed(mask_res->pData, mask_res->pData, DEPTH('#'), size);
}

/* Get the best possible icon of size <size> (mslh) with
   icon family id <id> */
af_direct_pixmap_t *
af_get_direct_icon(af_file_t * file, int id, char sizetag, int mode)
{
    int size;
    af_indexed_pixmap_t * ind;
    af_direct_pixmap_t * dir;

    size = SIZE(sizetag);

    dir = get_direct_icns(file, id, size, mode);
    if (dir) {
        return dir;
    }

    if (mode == 2) {
        return NULL;
    }
    
    if (size > 2) {
        /* There's no huge (ich[#48]) resource type, only icns subtypes */
        return NULL;
    }

    ind = af_get_indexed_icon(file, id, sizetag, 256, 0);
    if (ind) {
        dir = af_pixmap_indexed_to_direct(ind);
        FREEC(ind->data);
        FREEC(ind->mask);
        FREE(ind);
    }

    return dir;
}

