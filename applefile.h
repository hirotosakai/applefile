/* General external interfaces */

#ifndef _APPLEFILE_H_
#define _APPLEFILE_H_

#include <stdlib.h>

typedef struct af_file_t af_file_t;

typedef struct
{
    int             id;         /* resource ID */
    int             cName;      /* number of characters in name */
    char *          psName;     /* name string, not necessarily NUL terminated */
    int             cData;      /* number of bytes of data */
    unsigned char * pData;      /* pointer to data */
} af_res_t;

/* Open an apple file; returns a pointer to the file structure, or NULL on error */
af_file_t * af_open(char * filename);

/* Close an apple file; all af_res_t references to it are no longer valid */
void af_close(af_file_t * paf);

/* Look up a resource in an open apple file by type and ID; returns NULL if not found */
af_res_t * af_res(af_file_t * paf, char type[4], int id);

/* Look up the data fork for an open apple file; returns NULL if not found.
   if plen != NULL, fills in the length of the data
*/
void * af_data(af_file_t * paf, size_t * plen);

/* iterate over the resource types; return value is a pointer to
   a (not NUL-terminated) type ID; returns NULL when the end of the
   list is reached; pcookie should point to a void *, initialized to NULL */

char * af_restype_iter(af_file_t * paf, void ** pcookie);

/* iterate over the resources of a given type; returns NULL when the
   end of the list is reached; pcookie should point to a void *,
   initialized to NULL */
af_res_t * af_res_iter(af_file_t * paf, char type[4], void ** pcookie);
   
/* These are all unsafe to call with args with side effects.

   Whether they're horrible inefficient or more efficient than real
   functions is an open issue; it depends on how smart the compiler is,
   and I haven't checked */

#define af_read_u32(rc) ((rc)[0] <<24) + ((rc)[1] << 16) + ((rc)[2] << 8) + (rc)[3]
#define af_read_u24(rc) ((rc)[0] <<16) + ((rc)[1] << 8)  + (rc)[2]
#define af_read_u16(rc) ((rc)[0] << 8) + (rc)[1]
#define af_read_s16(rc) (af_read_u16(rc) > 32768? af_read_u16(rc) - 65536: af_read_u16(rc))

#endif
