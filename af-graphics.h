#ifndef _AF_GRAPHICS_H_
#define _AF_GRAPHICS_H_

#include <applefile.h>

typedef struct 
{
    unsigned int red, green, blue; /* 16-bit values */
} af_color_t;

typedef struct
{
    int        num_entries;
    af_color_t *entries;
} af_clut_t;

extern af_clut_t af_system_clut1;
extern af_clut_t af_system_clut2;
extern af_clut_t af_system_clut4;
extern af_clut_t af_system_clut8;

/* These types are my own invention; they don't match any of the
myriad of graphics resource types; they're not intended to be
particularly efficient.  They ARE intended to be as simple to convert
to and from as possible */


/* If pixel(r,c) is on, then data[r*cols + c] == 0xff;
   if pixel(r,c) is off, then data[r*cols + c] == 0x00;

   yes, this uses 8 times as much memory as necessary.  It
   also simplifies indexing.
*/

typedef struct 
{
    int             rows, cols;
    af_clut_t       clut;
    unsigned char * data;
    unsigned char * mask;
} af_indexed_pixmap_t;

/* This type gives a 32bit pixmap in 4 planes; this technically loses
   some color information if converted from the indexed type, as
   apple's cluts use 16 bit color entries; in practice, I can't see
   how that would matter much, and Apple is transitioning to 32bit
   color like everyone else */
typedef struct
{
    int             rows, cols;
    unsigned char * red;
    unsigned char * green;
    unsigned char * blue;
    unsigned char * alpha;
} af_direct_pixmap_t;

/* XXX there should be functions to free the memory for these */
af_direct_pixmap_t * af_pixmap_indexed_to_direct(af_indexed_pixmap_t *);
char *               af_indexed_pixmap_to_xpm(af_indexed_pixmap_t * pmap);

/* type-specific conversion/loading functions */
af_indexed_pixmap_t * af_cicn_to_indexed_pixmap(af_res_t * res);
af_indexed_pixmap_t * af_ppat_to_indexed_pixmap(af_res_t * res) ;

/* Get the best possible icon of size <size> (one of 'm','s','l','h') with
   no more than <color> colors, with icon family id <id> */
/* try_icns == 0  => use traditional resources only;
   try_icns == 1  => use icns, fallback to traditional (usual case)
   try_icns == 2  => use icns only */
af_indexed_pixmap_t * af_get_indexed_icon(af_file_t * file, int id, char size,
                                          int colors, int try_icns);

/* Get the best direct icon of size <size> (one of 'm','s','l','h') with
   icon family id <id>

   mode: 1 = do the best possible; convert from an indexed icon if necessary
         2 = return a 24bit+alpha icon or NULL
*/
af_direct_pixmap_t * af_get_direct_icon(af_file_t * file, int id, char size,
                                        int mode);

/* ugly function for combining the commonality of most of Apple's pixmap
   types; the interface is obviously rather baroque; one of the above functions
   is probably a better choice */
af_indexed_pixmap_t *
af_int_pmap_indexed(int rows,                   int cols,
                    int data_rowbytes,          int mask_rowbytes,
                    int data_bpp,               int mask_bpp,
                    unsigned char * data,       unsigned char * mask,
                    af_clut_t * clut);
#endif
