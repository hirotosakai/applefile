/* Internal interfaces */

#ifndef _APPLEFILE_INT_H_
#define _APPLEFILE_INT_H_

#include <stdlib.h>
#include "applefile.h"
#include "resource.h"

#define NEW(x) ((x *) malloc(sizeof(x)))
#define NEWC(c,x) ((x *) malloc((c)*sizeof(x)))
#define FREE(x) free(x)
#define FREEC(x) free(x)

typedef struct
{
    int (*close)(af_file_t *);
    int (*resourceFork)(af_file_t *, void **ppStart, size_t * pLen);
    int (*dataFork)(af_file_t *, void **ppStart, size_t * pLen);
} af_ff_vtab;

struct af_file_t
{
    af_ff_vtab * pafvt;
    void * pData;
    af_resfile_t * prf;
};

int open_applesingle_file(char * filename, af_file_t * paf);
int open_appledouble_file(char * filename, af_file_t * paf);
int open_hexbin_file(char * filename, af_file_t * paf);
int open_macbinary_file(char * filename, af_file_t * paf);

#endif
