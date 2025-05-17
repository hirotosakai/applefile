#include <stdio.h>

/* compute_crc_slow and init_crc_table and the created af_compute_crc are
   only slightly modified versions of functions in
   ftp://ftp.std.com/obi/Standards/FileTransfer/XMODEM-CRC.NOTE.1 */

static
unsigned short
compute_crc_slow(unsigned short crc,
                 unsigned char * bufptr,
                 size_t len)
{
    short i;

    while (len--) { /* calculate CRC from end to start*/
	crc ^= (unsigned short) (*bufptr++)<<8;
	for (i=0 ;i<8 ;++i) {
	    if (crc & 0x8000)
		crc = (crc << 1) ^ 0x1021;
	    else
		crc <<= 1;
	    crc &= 0xffff;
	}
     }
     return(crc);
}

unsigned short crc_table[256];   /* declaration for the table */

static
void
init_crc_table(void)
{
    int count;
    char zero;
    
    zero = 0;
    for (count = 0; count < 256; count++)
	crc_table[count] = compute_crc_slow(count<<8, &zero, 1);
}

int
main(int argc, char ** argv)
{
    int i;

    if (argc != 1) {
	fprintf(stderr, "Usage: %s\n", argv[0]);
	return 1;
    }
    
    init_crc_table();
    fputs("/* THIS IS A GENERATED FILE */\n"
	  "/* If any modiciations need to be made, modify make_crc_table.c */\n"
	  "/* and rebuild this file. */\n\n"
	  "#include <stdlib.h>\n"
	  "#include \"crc.h\"\n"
	  "static unsigned short crc_table[256] = {", stdout);
    for (i = 0; i < 256; i++) {
	if (i % 8 == 0)
	    fputs("\n    ", stdout);
	printf("0x%4.4x, ", crc_table[i]);
    }
    fputs("};\n"
	  "\n"
	  "unsigned short\n"
	  "af_compute_crc(unsigned short crc, unsigned char * p, size_t len)\n"
	  "{\n"
	  "    while (len--)\n"
	  "        crc = crc_table[crc >> 8^(*p++)] ^ crc<<8;\n"
	  "    return(crc);\n"
	  "}\n", stdout);
    return 0;
}
