#include <stdio.h>
#include <stdarg.h>

#include "applefile_int.h"
#include "af-graphics.h"


typedef struct growstr
{
    char * data;
    size_t used;
    size_t alloced;
} * growstr;

static
growstr
growstr_new(void)
{
    growstr st;

    st = NEW(struct growstr);
    st->data = malloc(100);
    st->used = 0;
    st->alloced = 100;

    return st;
}

static
void
growstr_sprintf(growstr st, char * format, ...)
{
    va_list ap;
    int r;
    
    va_start(ap, format);
    while (((r = vsnprintf(st->data + st->used,
                           st->alloced - st->used,
                           format, ap)) == -1) ||
           (r >= st->alloced - st->used - 5)) {
	st->data = realloc(st->data, st->alloced * 2 + 100);
	st->alloced = st->alloced * 2 + 100;
    }
    st->used += r;
    va_end(ap);
}

static
char *
growstr_free(growstr st)
{
    char * r;

    r = st->data;
    FREE(st);
    return r;
}

af_direct_pixmap_t *
af_pixmap_indexed_to_direct(af_indexed_pixmap_t * ind)
{
    af_direct_pixmap_t * dir;
    int rows, cols, row, col, index;
    af_color_t * color;
    
    rows       = ind->rows;
    cols       = ind->cols;
    dir        = NEW(af_direct_pixmap_t);
    if (dir == NULL)
        return NULL;
    dir->rows  = rows;
    dir->cols  = cols;
    dir->red   = NEWC(rows * cols, unsigned char);
    dir->green = NEWC(rows * cols, unsigned char);
    dir->blue  = NEWC(rows * cols, unsigned char);
    dir->alpha = NEWC(rows * cols, unsigned char);
    if (dir->red   == NULL ||
        dir->green == NULL ||
        dir->blue  == NULL ||
        dir->alpha == NULL)
        return NULL;
    
    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            index = row * cols + col;
            if (ind->mask[index]) {
                color             = &ind->clut.entries[ind->data[index]];
                dir->red[index]   = (color->red + 0.5)/256.0;
                dir->green[index] = (color->green + 0.5)/256.0;
                dir->blue[index]  = (color->blue + 0.5)/256.0;
                dir->alpha[index] = 0xff;
            } else {
                dir->alpha[index] = 0x00;
            }
        }
    }

    return dir;
}

static
char *
xpm_color_code(int c)
{
    static char ret[3];
    char chars[] =
	"abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"0123456789!@"; /* 64 */
    if (c == -1) {
	ret[0] = ' ';
	ret[1] = ' ';
	ret[2] = 0;
	return ret;
    }
    
    ret[0] = chars[c/64];
    ret[1] = chars[c%64];
    ret[2] = 0;

    return ret;
}

char *
af_indexed_pixmap_to_xpm(af_indexed_pixmap_t * pmap) 
{
    int row, col, index, color;
    growstr st;
    
    st = growstr_new();
    growstr_sprintf(st, "/* XPM */\nstatic char *name[] = {\n"
		    "\"%d %d %d %d\",\n",
		    pmap->cols, pmap->rows, pmap->clut.num_entries+1, 2);
    growstr_sprintf(st, "\"%s c none\",\n", xpm_color_code(-1));

    /* This makes no attempt to only output the colors used; is this a
       problem for anything? */
    for (color = 0; color < pmap->clut.num_entries; color++) {
        growstr_sprintf(st, "\"%s c rgb:%.4x/%.4x/%.4x\",\n",
                        xpm_color_code(color),
                        pmap->clut.entries[color].red,
                        pmap->clut.entries[color].green,
                        pmap->clut.entries[color].blue);
    }
    for (row = 0; row < pmap->rows; row++) {
	growstr_sprintf(st, "\"");
	for (col = 0; col < pmap->cols; col++) {
            index = row * pmap->cols + col;
	    growstr_sprintf(st, "%s",
			    xpm_color_code(pmap->mask[index] ?
                                           pmap->data[index] : -1));
	}
	growstr_sprintf(st, "\",\n");
    }
    growstr_sprintf(st, "};\n");

    return growstr_free(st);
}

static unsigned char mask_bytes_tab[][8] = {
    {},
    {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01},
    {0xc0, 0x30, 0x0c, 0x03},
    {},
    {0xf0, 0x0f},
    {},
    {},
    {},
    {0xff}
};
static unsigned char shift_amount_tab[][8] = {
    {},
    {7, 6, 5, 4, 3, 2, 1, 0},
    {6, 4, 2, 0},
    {},
    {4, 0},
    {},
    {},
    {},
    {0}
};

af_indexed_pixmap_t *
af_int_pmap_indexed(int rows,                   int cols,
                    int data_rowbytes,          int mask_rowbytes,
                    int data_bpp,               int mask_bpp,
                    unsigned char * data,       unsigned char * mask,
                    af_clut_t * clut)
{
    int                   data_ppB          = 8/data_bpp;
    int                   mask_ppB          = 8/mask_bpp;
    unsigned char *       data_mask_bytes   = mask_bytes_tab[data_bpp];
    unsigned char *       mask_mask_bytes   = mask_bytes_tab[mask_bpp];
    unsigned char *       data_shift_amount = shift_amount_tab[data_bpp];
    unsigned char *       mask_shift_amount = shift_amount_tab[mask_bpp];
    af_indexed_pixmap_t * ind;
    int                   row, col;

    ind                   = NEW(af_indexed_pixmap_t);
    ind->rows             = rows;
    ind->cols             = cols;
    ind->data             = NEWC(rows * cols, unsigned char);
    ind->mask             = NEWC(rows * cols, unsigned char);
    ind->clut.num_entries = clut->num_entries;
    ind->clut.entries     = NEWC(ind->clut.num_entries, af_color_t);
    memcpy(ind->clut.entries, clut->entries,
           ind->clut.num_entries * sizeof(af_color_t));

    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            int           data_byte_index;
            int           data_bit_index;
            int           mask_byte_index;
            int           mask_bit_index;
            unsigned char data_byte;
            unsigned char mask_byte;
            
            data_byte_index = row * data_rowbytes + col / data_ppB;
            data_bit_index  = col % data_ppB;
            data_byte       = data[data_byte_index];

            data_byte  &= data_mask_bytes[data_bit_index];
            data_byte >>= data_shift_amount[data_bit_index];
            ind->data[row * cols + col] = data_byte;

            if (mask) {
                mask_byte_index = row * mask_rowbytes + col / mask_ppB;
                mask_bit_index  = col % mask_ppB;
                mask_byte       = mask[mask_byte_index];

                mask_byte  &= mask_mask_bytes[mask_bit_index];
                mask_byte >>= mask_shift_amount[mask_bit_index];
                if (mask_bpp == 1 && mask_byte == 1) {
                    mask_byte = 0xff;
                }
            } else {
                mask_byte = 0xff;
            }
            ind->mask[row * cols + col] = mask_byte;
        }
    }

    return ind;
}

