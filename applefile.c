#include <string.h>

#include "applefile_int.h"
#include "resource.h"

af_file_t *
af_open(char * filename)
{
    af_file_t * paf;
    void * pResource;
    size_t cResource;
    
    paf = NEW(af_file_t);
    memset(paf, 0, sizeof *paf);
    
    do {
	/* FIXJTL: if there's any more of these, there should be a general
	   way of registering them instead of hardcoding them here */
	if (open_hexbin_file(filename, paf) == 0)
	    break;
	if (open_macbinary_file(filename, paf) == 0)
	    break;
	if (open_applesingle_file(filename, paf) == 0)
	    break;
	if (open_appledouble_file(filename, paf) == 0)
	    break;
    } while (0);

    if (paf->pafvt == NULL) {
	FREE(paf);
	return NULL;
    }

    paf->pafvt->resourceFork(paf, &pResource, &cResource);
    if (cResource) {
	paf->prf = af_rf_load(pResource);
    } else {
	paf->prf = NULL;
    }
    
    return paf;
}

void
af_close(af_file_t * paf)
{
    if (paf->prf)
	af_rf_unload(paf->prf);
    paf->pafvt->close(paf);
    FREE(paf);
}

af_res_t *
af_res(af_file_t * paf, char type[4], int id)
{
    if (paf->prf)
	return af_rf_res(paf->prf, type, id);
    else
	return NULL;
}

void *
af_data(af_file_t * paf, size_t * plen)
{
    void * pData;
    size_t cData;
    
    paf->pafvt->dataFork(paf, &pData, &cData);
    if (plen)
        *plen = cData;
    
    return pData;
}

char *
af_restype_iter(af_file_t * paf, void ** pcookie)
{
    if (pcookie == NULL)
        return NULL;

    if (paf->prf)
        return af_rf_restype_iter(paf->prf, pcookie);
    else
        return NULL;
}

af_res_t *
af_res_iter(af_file_t * paf, char type[4], void ** pcookie)
{
    if (pcookie == NULL)
        return NULL;

    if (paf->prf)
        return af_rf_res_iter(paf->prf, type, pcookie);
    else
        return NULL;
}

