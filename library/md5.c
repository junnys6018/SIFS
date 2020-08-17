//  From code at:    https://rosettacode.org/wiki/MD5#C
//  Refactored by Chris.McDonald@uwa.edu.au
//  simple enough that I can almost understand it!

#include "md5.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>

typedef uint32_t (*DgstFctn)(uint32_t a[]);
 
static uint32_t f0(uint32_t abcd[])
{
    return (abcd[1] & abcd[2]) | (~abcd[1] & abcd[3]);
}
 
static uint32_t f1(uint32_t abcd[])
{
    return (abcd[3] & abcd[1]) | (~abcd[3] & abcd[2]);
}
 
static uint32_t f2(uint32_t abcd[])
{
    return  abcd[1] ^ abcd[2] ^ abcd[3];
}
 
static uint32_t f3(uint32_t abcd[])
{
    return abcd[2] ^ (abcd[1] |~ abcd[3]);
}
 
// ROtate v Left by amt bits
static uint32_t ROL(uint32_t v, int amt)
{
    uint32_t  msk1 = (1<<amt) -1;
    return ((v>>(32-amt)) & msk1) | ((v<<amt) & ~msk1);
}
 
//  --------------------------------------------------------------------------

static void *MD5(const char *msg, size_t mlen) 
{
    typedef uint32_t Digest[4];
 
    static Digest result;
    static Digest init	= { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476 };

    memcpy(result, init, sizeof(Digest));		// initialise
 
    static DgstFctn funcs[]	= { &f0, &f1, &f2, &f3 };
    static int16_t M[]		= { 1, 5, 3, 7 };
    static int16_t O[]		= { 0, 1, 5, 0 };
    static int16_t rot0[]	= { 7, 12, 17, 22 };
    static int16_t rot1[]	= { 5, 9, 14, 20 };
    static int16_t rot2[]	= { 4, 11, 16, 23 };
    static int16_t rot3[]	= { 6, 10, 15, 21 };
    static int16_t *rots[]	= { rot0, rot1, rot2, rot3 };
    static uint32_t *k		= NULL;
 
    if(k == NULL) {
	static uint32_t kspace[64];
	k = kspace;

	for(int i=0; i<64; i++) {
	    double s = fabs(sin((double)(1+i)));

#define	TWO32		4294967296UL
	    k[i] = (uint32_t)(s * TWO32);
#undef	TWO32
	}
    }
 
    int	ngroups	= (mlen+8)/64 + 1;
    uint8_t msgcopy[64*ngroups];

    memset(msgcopy, 0, sizeof(msgcopy));
    memcpy(msgcopy, msg, mlen);
    msgcopy[mlen] = (uint8_t)0x80;  

    uint32_t l = 8*mlen;
    memcpy(msgcopy + (64*ngroups - 8), &l, 4);
 
    union {
        uint32_t w[16];
        char     b[64];
    } mm;
 
    int offset = 0;
    for(int group=0 ; group<ngroups ; ++group, offset += 64) {
	Digest abcd;
	memcpy(abcd, result, sizeof(Digest));

        memcpy(mm.b, msgcopy+offset, 64);

        for(int p=0 ; p<4 ; p++) {
	    DgstFctn fctn	= funcs[p];
            int16_t *rotn	= rots[p];
            int m		= M[p];
	    int o		= O[p];

            for(int q=0 ; q<16 ; q++) {
                int g		= (m*q + o) % 16;
		uint32_t f	=
		    abcd[1] + ROL(abcd[0]+ fctn(abcd) + k[q+16*p] + mm.w[g], rotn[q%4]);
 
                abcd[0] = abcd[3];
                abcd[3] = abcd[2];
                abcd[2] = abcd[1];
                abcd[1] = f;
            }
        }
        for(int p=0 ; p<4 ; p++)
            result[p] += abcd[p];
    }
    return result;
}    
 
//  --------------------------------------------------------------------------

//  CALCULATE THE MD5 DIGEST OF input BUFFER, LEAVE RESULT IN md5_result
void *MD5_buffer(const char *buffer, size_t len, void *md5_result)
{
    return memcpy(md5_result, MD5(buffer, len), MD5_BYTELEN);
}

//  RETURNS A 'HUMAN-READABLE' FORMATTED STRING OF AN MD5 DIGEST
char *MD5_format(const void *md5_result)
{
    static char	fmt[MD5_STRLEN+1];

    char *s	= fmt;
    unsigned char *res	= (unsigned char *)md5_result;

    for(int n=0 ; n<MD5_BYTELEN; ++n, s+=2)
	sprintf(s, "%02x", res[n]);
    fmt[MD5_STRLEN]	= '\0';
    return fmt;
}

//  RETURNS A 'HUMAN-READABLE' FORMATTED STRING OF DIGEST OF A FILE'S CONTENTS
char *MD5_file(const char *filenm)
{
    int	fd = open(filenm, O_RDONLY, 0);

    if(fd >= 0) {
	struct stat sbuf;

	if(fstat(fd, &sbuf) == 0) {
	    char bytes[sbuf.st_size];

	    if(read(fd, bytes, sbuf.st_size) == sbuf.st_size) {
		close(fd);
		return MD5_format(MD5(bytes, sbuf.st_size));
	    }
	}
	close(fd);
    }
    return MD5_format(MD5("", 0));
}

//  RETURNS A 'HUMAN-READABLE' FORMATTED STRING OF DIGEST OF A STRING
char *MD5_str(const char *str)
{
    return MD5_format(MD5(str, strlen(str)));
}

//  --------------------------------------------------------------------------

#if	defined(WANT_TESTING)
void MD5_TEST1(const char *expect, const char *msg)
{
    char *str = MD5_str(msg);

    printf("%s\n%s\n%s\n", msg, expect, str);
    printf("%s\n", (strcmp(expect, str) == 0) ? "PASS" : "FAIL");
}

void MD5_TESTALL(void)
{
//  THE FOLLOWING VERIFICATION STRINGS AND HASHES COME FROM RFC1321
    MD5_TEST1("d41d8cd98f00b204e9800998ecf8427e",
	"");
    MD5_TEST1("0cc175b9c0f1b6a831c399e269772661",
	"a");
    MD5_TEST1("900150983cd24fb0d6963f7d28e17f72",
	"abc");
    MD5_TEST1("f96b697d7cb7938d525a2f31aaf161d0",
	"message digest");
    MD5_TEST1("c3fcd3d76192e4007dfb496cca67e13b",
	"abcdefghijklmnopqrstuvwxyz");
    MD5_TEST1("d174ab98d277d9f5a5611c2c9f419d9f",
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    MD5_TEST1("57edf4a22be3c955ac49da2e2107b67a",
	"12345678901234567890123456789012345678901234567890123456789012345678901234567890");
    MD5_TEST1("9e107d9d372bb6826bd81d3542a419d6",
	"The quick brown fox jumps over the lazy dog");

    printf("\n%s\n", MD5_file("Makefile"));
}
#endif

//  vim: ts=8 sw=4
