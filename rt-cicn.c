#include <stdio.h>
#include <stdlib.h>

#include "applefile.h"
#include "applefile_int.h"
#include "af-graphics.h"

struct rect {
    int top, left, bottom, right;
};

struct bitmap {
    int             baseaddr;
    int             rowbytes;
    struct rect     bounds;
    int             datalen;
    unsigned char * data;
};

struct color {
    int value;
    int red, green, blue;
};

struct cicn 
{
    int             baseaddr;
    int             is_a_pixmap;
    int             reserved1;
    int             rowbytes;
    struct rect     bounds;
    int             version;
    int             packing;
    int             packed_size;
    int             res_hor;
    int             res_vert;
    int             pixel_type;
    int             pixel_size;
    int             components;
    int             comp_size;
    int             plane_offset;
    int             clut;
    int             reserved2;
    struct bitmap   mask;
    struct bitmap   bitmap;
    int             datahandle;
    int             seed;
    int             flags;
    int             color_count;
    struct color *  color_data;
    int             datalen;
    unsigned char * data;
};

#define u32(p) af_read_u32(p); p += 4;
#define u16(p) af_read_u16(p); p += 2;

static
void
load_cicn(af_res_t * res, struct cicn * cicn)
{
    unsigned char * p;
    unsigned int tmp;
    int i;
    
    p =  res->pData;

    cicn->baseaddr                = u32(p);
    tmp                           = u16(p);
    cicn->is_a_pixmap             = tmp & 0x8000;
    cicn->reserved1               = tmp & 0x6000;
    cicn->rowbytes                = tmp & 0x1fff;
    cicn->bounds.top              = u16(p);
    cicn->bounds.left             = u16(p);
    cicn->bounds.bottom           = u16(p);
    cicn->bounds.right            = u16(p);
    cicn->version                 = u16(p);
    cicn->packing                 = u16(p);
    cicn->packed_size             = u32(p);
    cicn->res_hor                 = u32(p);
    cicn->res_vert                = u32(p);
    cicn->pixel_type              = u16(p);
    cicn->pixel_size              = u16(p);
    cicn->components              = u16(p);
    cicn->comp_size               = u16(p);
    cicn->plane_offset            = u32(p);
    cicn->clut                    = u32(p);
    cicn->reserved2               = u32(p);
    cicn->mask.baseaddr           = u32(p);
    cicn->mask.rowbytes           = u16(p);
    cicn->mask.bounds.top         = u16(p);
    cicn->mask.bounds.left        = u16(p);
    cicn->mask.bounds.bottom      = u16(p);
    cicn->mask.bounds.right       = u16(p);
    cicn->bitmap.baseaddr         = u32(p);
    cicn->bitmap.rowbytes         = u16(p);
    cicn->bitmap.bounds.top       = u16(p);
    cicn->bitmap.bounds.left      = u16(p);
    cicn->bitmap.bounds.bottom    = u16(p);
    cicn->bitmap.bounds.right     = u16(p);
    cicn->datahandle              = u32(p);
    cicn->mask.datalen            = cicn->mask.rowbytes *
        (cicn->mask.bounds.bottom - cicn->mask.bounds.top);
    cicn->mask.data               = NEWC(cicn->mask.datalen, unsigned char);
    memcpy(cicn->mask.data, p, cicn->mask.datalen); p += cicn->mask.datalen;
    cicn->bitmap.datalen          = cicn->bitmap.rowbytes *
        (cicn->bitmap.bounds.bottom - cicn->bitmap.bounds.top);
    cicn->bitmap.data             = NEWC(cicn->bitmap.datalen, unsigned char);
    memcpy(cicn->bitmap.data, p, cicn->bitmap.datalen);
    p += cicn->bitmap.datalen;
    cicn->seed                    = u32(p);
    cicn->flags                   = u16(p);
    cicn->color_count             = u16(p);
    cicn->color_data              = NEWC(cicn->color_count + 1, struct color);
    for (i = 0; i <= cicn->color_count; i++) {
        cicn->color_data[i].value = u16(p);
        cicn->color_data[i].red   = u16(p);
        cicn->color_data[i].green = u16(p);
        cicn->color_data[i].blue  = u16(p);
    }
    cicn->datalen = cicn->rowbytes * (cicn->bounds.bottom - cicn->bounds.top);
    cicn->data    = NEWC(cicn->datalen, unsigned char);
    memcpy(cicn->data, p, cicn->datalen);

    return;
}

static
void
unload_cicn(struct cicn * cicn)
{
    FREEC(cicn->mask.data);
    FREEC(cicn->bitmap.data);
    FREEC(cicn->color_data);
    FREEC(cicn->data);
}


af_indexed_pixmap_t *
af_cicn_to_indexed_pixmap(af_res_t * res) 
{
    int                   color;
    struct cicn           cicn;
    af_indexed_pixmap_t * ind;
    af_clut_t             clut;
    
    if (res == NULL)
        return NULL;
    
    load_cicn(res, &cicn);

    clut.num_entries = 1 << cicn.pixel_size;
    clut.entries = NEWC(clut.num_entries, af_color_t);
    
    for (color = 0; color < cicn.color_count; color++) {
        af_color_t *   dest;
        struct color * src;

        src         = &cicn.color_data[color];
        dest        = &clut.entries[src->value];
        dest->red   = src->red;
        dest->green = src->green;
        dest->blue  = src->blue;
    }

    ind = af_int_pmap_indexed(cicn.bounds.bottom - cicn.bounds.top,
                              cicn.bounds.right - cicn.bounds.left,
                              cicn.rowbytes,    cicn.mask.rowbytes,
                              cicn.pixel_size,  1,
                              cicn.data,        cicn.mask.data,
                              &clut);
    FREEC(clut.entries);
    unload_cicn(&cicn);
    return ind;
}
