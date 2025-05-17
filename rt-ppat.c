#include <stdio.h>
#include <stdlib.h>

#include "applefile.h"
#include "applefile_int.h"
#include "af-graphics.h"

struct rect {
    int top, left, bottom, right;
};

struct color {
    int value;
    int red, green, blue;
};

struct ppat 
{
    int             pattern_type;       /* 16 */
    int             record_offset;      /* 32 */
    int             data_offset;        /* 32 */
    int             expanded_pixel;     /* 32 */
    int             valid;              /* 16 */
    int             expanded_pattern;   /* 32 */
    unsigned char   old_style[8];       /* 64? */
    int             baseaddr;           /* 32 */
    int             color_qd;           /* 1 */
    int             reserved1;          /* 2 */
    int             rowbytes;           /* 13 */
    struct rect     bounds;             /* 64 */
    int             version;            /* 16 */
    int             packing_format;     /* 16 */
    int             packing_size;       /* 32 */
    int             res_hor;            /* 32 */
    int             res_vert;           /* 32 */
    int             pixel_type;         /* 16 */
    int             pixel_size;         /* 16 */
    int             components;         /* 16 */
    int             comp_size;          /* 16 */
    int             plane_offset;       /* 32 */
    int             clut;               /* 32 */
    int             reserved2;          /* 32 */
    int             datalen;
    unsigned char * data;               /* rows * rowbytes */
    int             seed;               /* 32 */
    int             flags;              /* 16 */
    int             color_count;        /* 16 */
    struct color *  color_data;         /* 64 * (color_count+1) */
};

#define u32(p) af_read_u32(p); p += 4;
#define u16(p) af_read_u16(p); p += 2;

static
void
load_ppat(af_res_t * res, struct ppat * ppat)
{
    unsigned char * p;
    unsigned int tmp;
    int i;
    
    p = res->pData;

    ppat->pattern_type            = u16(p);
    ppat->record_offset           = u32(p);
    ppat->data_offset             = u32(p);
    ppat->expanded_pixel          = u32(p);
    ppat->valid                   = u16(p);
    ppat->expanded_pattern        = u32(p);
    memcpy(ppat->old_style, p, 8); p += 8;
    ppat->baseaddr                = u32(p);
    tmp                           = u16(p);
    ppat->color_qd                = tmp & 0x8000;
    ppat->reserved1               = tmp & 0x6000;
    ppat->rowbytes                = tmp & 0x1fff;
    ppat->bounds.top              = u16(p);
    ppat->bounds.left             = u16(p);
    ppat->bounds.bottom           = u16(p);
    ppat->bounds.right            = u16(p);
    ppat->version                 = u16(p);
    ppat->packing_format          = u16(p);
    ppat->packing_size            = u32(p);
    ppat->res_hor                 = u32(p);
    ppat->res_vert                = u32(p);
    ppat->pixel_type              = u16(p);
    ppat->pixel_size              = u16(p);
    ppat->components              = u16(p);
    ppat->comp_size               = u16(p);
    ppat->plane_offset            = u32(p);
    ppat->clut                    = u32(p);
    ppat->reserved2               = u32(p);
    ppat->datalen                 = ppat->rowbytes *
        (ppat->bounds.bottom - ppat->bounds.top);
    ppat->data                    = NEWC(ppat->datalen, unsigned char);
    memcpy(ppat->data, p, ppat->datalen);
    p += ppat->datalen;
    ppat->seed                    = u32(p);
    ppat->flags                   = u16(p);
    ppat->color_count             = u16(p);
    ppat->color_data              = NEWC(ppat->color_count + 1, struct color);
    for (i = 0; i <= ppat->color_count; i++) {
        ppat->color_data[i].value = u16(p);
        ppat->color_data[i].red   = u16(p);
        ppat->color_data[i].green = u16(p);
        ppat->color_data[i].blue  = u16(p);
    }

    return;
}

static
void
unload_ppat(struct ppat * ppat)
{
    FREEC(ppat->data);
}

/* this is almost identical to af_cicn_to_indexed pixmap; the
   structures have different names (and need a (slightly) different
   load function), and there's no mask.  surely there's a reasonable
   way of reducing code size */
af_indexed_pixmap_t *
af_ppat_to_indexed_pixmap(af_res_t * res) 
{
    int                   color;
    struct ppat           ppat;
    af_indexed_pixmap_t * ind;
    af_clut_t             clut;
    
    if (res == NULL)
        return NULL;
    
    load_ppat(res, &ppat);

    clut.num_entries = 1 << ppat.pixel_size;
    clut.entries = NEWC(clut.num_entries, af_color_t);

    if (ppat.pixel_size > 8) {
        /* error report? */
        /* XXX unimplemented */
        unload_ppat(&ppat);
        return NULL;
    }
    
    for (color = 0; color < ppat.color_count; color++) {
        af_color_t *   dest;
        struct color * src;

        src         = &ppat.color_data[color];
        dest        = &clut.entries[src->value];
        dest->red   = src->red;
        dest->green = src->green;
        dest->blue  = src->blue;
    }

    ind = af_int_pmap_indexed(ppat.bounds.bottom - ppat.bounds.top,
                              ppat.bounds.right - ppat.bounds.left,
                              ppat.rowbytes,    0,
                              ppat.pixel_size,  1,
                              ppat.data,        NULL,
                              &clut);
    FREEC(clut.entries);
    unload_ppat(&ppat);
    return ind;
}
