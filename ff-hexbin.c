#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "applefile_int.h"

/* The notional hexbin header, when it's in binary (not ascii) format */
/* This struct can't be usefully declared in C at all, but is here for
   reference */
#if 0
struct HBBinHeader 
{
    unsigned char u8_fname_length[1]; /* length of filename */
    unsigned char u8_fname[u8_fname_length];
    unsigned char u8_version[1];
    unsigned char ost_type[4];
    unsigned char ost_creator[4];
    unsigned char u16_flags[2];
    unsigned char u32_data_length[4];
    unsigned char u32_resource_length[4];
    unsigned char u16_crc_1[2];	/* CRC on what? Why do they write
				   specs that don't specify anything? */
    unsigned char u8_data[u32_data_length];
    unsigned char u16_crc_2[2];
    unsigned char u8_resource[u32_resource_length];
    unsigned char u16_crc_2[2];
}
#endif

/* The information about an open hexbin file needed for the various
   af_file_t operations */
struct HBData 
{
    size_t cData;
    size_t cResource;
    void * pBase;
    void * pData;
    void * pResource;
};

static int close_file(af_file_t *);
static int resource_fork(af_file_t *, void **, size_t *);
static int data_fork(af_file_t *, void **, size_t *);

af_ff_vtab afvtHexBin = {
    close_file,
    resource_fork,
    data_fork,
};

static char szSignature[] = "(This file must be converted with BinHex 4.0)";
static size_t cSignature = sizeof(szSignature)-1;

static
unsigned char *
decodeAscii(unsigned char * pStart, size_t cBytes)
{
    unsigned char * pBin, * pBinEnd, * pExpand;
    unsigned char * s;
    unsigned char * d;
    unsigned long tmp;
    size_t cAlloced, cSizeExpand;
    int p;
    /* ARGGGH.  The list in RFC1741 is *wrong*.  It includes an erroneous
       space */  
    static char decode_chars[] = "!\"#$%&'()*+,-012345689@ABCDEFGHIJKLMNPQRSTUVXYZ[`abcdefhijklmpqr";
    static char decode_table[256];
    static int decode_initialized = 0;

    if (!decode_initialized) {
	unsigned char * pc;
	int i;

	memset(decode_table, -1, sizeof decode_table);
	for (pc = decode_chars, i = 0; *pc; pc++, i++)
	    decode_table[*pc] = i;
	decode_table[0] = -2;
	decode_initialized = 1;
    }
    
    cAlloced = ((cBytes+3)*3)/4 + 2;
    pBin = NEWC(cAlloced, unsigned char);
    d = pBin;
    s = pStart;

    /* This is okay, because a valid HexBin file will have at least
       one ':' after the data */ 
    pStart[cBytes-1] = 0;
    
    while (1) {
	tmp = 0;
	while ((p = decode_table[*s]) == -1)
	    s++;
	if (p == -2)
	    break;
	tmp |= p;
	s++;

	while ((p = decode_table[*s]) == -1)
	    s++;
	if (p == -2) /* error */
	    break;
	tmp <<= 6;
	tmp |= p;
	s++;

	while ((p = decode_table[*s]) == -1)
	    s++;
	if (p == -2) {
	    tmp >>= 4;
	    *d++ = (tmp & 0xff) >> 16;
	    break;
	}
	tmp <<= 6;
	tmp |= p;
	s++;

	while ((p = decode_table[*s]) == -1)
	    s++;
	if (p == -2) {
	    tmp >>= 2;
	    *d++ = (tmp & 0xff00) >> 8;
	    *d++ = (tmp & 0x00ff) >> 16;
	    break;
	}
	tmp <<= 6;
	tmp |= p;
	s++;

	*d++ = (tmp & 0xff0000) >> 16;
	*d++ = (tmp & 0x00ff00) >> 8;
	*d++ = (tmp & 0x0000ff);
    }
    
    pBinEnd = d+1;
    cSizeExpand = d - pBin + 1;
    for (s = pBin; s < pBinEnd; s++) {
	if (*s == 0x90) {
	    s++;
	    if (*s) {
		cSizeExpand += *s - 1;
	    } else {
		cSizeExpand--;
	    }
	}
    }

    pExpand = NEWC(cSizeExpand, unsigned char);
    d = pExpand;
    for (s = pBin; s < pBinEnd; s++) {
	if (*s == 0x90) {
	    s++; /* Skip the marker */
	    if (*s) {
		memset(d, d[-1], (*s)-1);
		d += (*s)-1;
	    } else {
		*d++ = 0x90;
	    }
	} else {
	    *d++ = *s;
	}
    }

    FREEC(pBin);
    return pExpand;
}

int
open_hexbin_file(char * filename, af_file_t * paf)
{
    int fd = -1;
    struct stat stat_buf;
    unsigned char * pBaseAscii = (void *)-1;
    unsigned char * pBaseBin = NULL;
    unsigned char * pStart;
    unsigned char * p;
    unsigned char * pData, * pResource;
    struct HBData * phbd;
    size_t cData, cResource;
    size_t cAscii = 0;
    
    do {
	fd = open(filename, O_RDONLY);
	if (fd < 0 && errno == ENOENT) {
	    char * tmpfilename;

	    tmpfilename = NEWC(strlen(filename)+5, char);
	    sprintf(tmpfilename, "%s.hqx", filename);
	    fd = open(tmpfilename, O_RDONLY);
	    FREEC(tmpfilename);
	}
	if (fd < 0)
	    break;

	if (fstat(fd, &stat_buf) < 0)
	    break;

	cAscii = stat_buf.st_size;
	pBaseAscii = mmap(NULL, cAscii, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
	close(fd); fd = -1;
	if (pBaseAscii == (void *)-1)
	    break;

	for (pStart = pBaseAscii; pStart < pBaseAscii + cAscii; pStart++) {
	    if (*pStart == '(' &&
		strncmp(pStart, szSignature, cSignature) == 0) {
		pStart = pStart + cSignature;
		break;
	    }
	}
	if (pStart == pBaseAscii + cAscii)
	    break;
	
	pBaseBin = decodeAscii(pStart, cAscii - (pStart-pBaseAscii));
	munmap(pBaseAscii, cAscii); pBaseAscii = (void *)-1;
	
	if (pBaseBin == NULL)
	    break;

	p = pBaseBin + *pBaseBin + 1; /* skip over filename */
	p++;			/* skip version */
	p += 4;			/* skip type */
	p += 4;			/* skip creator */
	p += 2;			/* skip flags */
	cData = af_read_u32(p);
	p += 4;
	cResource = af_read_u32(p);
	p += 4;
	/* Verify CRCs - crc on what,exactly, though? */
	p += 2;			/* skip CRC 1 */
	pData = p;
	p += cData;
	p += 2;			/* skip CRC 2 */
	pResource = p;
	p += cResource;
	p += 2;			/* skip CRC 3 */

	phbd = NEW(struct HBData);
	phbd->pBase = pBaseBin;
	phbd->cData = cData;
	phbd->cResource = cResource;
	phbd->pData = pData;
	phbd->pResource = pResource;

	paf->pafvt = &afvtHexBin;
	paf->pData = phbd;

	return 0;
    } while (0);

    if (fd >= 0)
	close(fd);
    if (pBaseAscii != (void *)-1)
	munmap(pBaseAscii, cAscii);
    if (pBaseBin)
	FREE(pBaseBin);
    return -1;
}

static
int
close_file(af_file_t * paf)
{
    struct HBData * phbd;

    phbd = paf->pData;
    
    FREE(phbd->pBase);
    FREE(phbd);

    paf->pafvt = NULL;
    paf->pData = NULL;

    return 0;
}

static
int
resource_fork(af_file_t *paf, void **ppBase, size_t *pcSize)
{
    struct HBData * phbd;

    phbd = paf->pData;

    *ppBase = phbd->pResource;
    *pcSize = phbd->cResource;

    return 0;
}

static
int
data_fork(af_file_t *paf, void **ppBase, size_t *pcSize)
{
    struct HBData * phbd;

    phbd = paf->pData;

    *ppBase = phbd->pData;
    *pcSize = phbd->cData;

    return 0;
}

