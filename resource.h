#ifndef _RESOURCE_H_
#define _RESOURCE_H_

#include "applefile.h"

typedef struct af_resfile_t af_resfile_t;

/* Load a resource file from a buffer

   FIXJTL: Right now, little data validation is done.  Don't call this
   on a buffer that you aren't sure is a resource file.  Don't be
   surprised if corrupt resource files cause crashes. */
af_resfile_t * af_rf_load(char * pFileBuffer);

/* Unload a loaded resource file */
void af_rf_unload(af_resfile_t * prf);

/* Get a resource by a type and ID; the returned structure should NOT
   be modified.  The structure and the data it points to will be valid
   until af_rf_unload() is called on this file; if the data is
   needed longer than that, it should be copied by the caller.  No
   interpretation of the resource data is done, so any necessary byte
   swapping will have to be done by the caller.  The af_read_u32 and
   af_read_u16 macros may be helpful for this. Returns NULL if no such
   resource is found. */
af_res_t * af_rf_res(af_resfile_t * prf, char *type, int id);

char * af_rf_restype_iter(af_resfile_t * prf, void ** pcookie);
af_res_t * af_rf_res_iter(af_resfile_t * prf, char *type, void ** pcookie);

#endif
