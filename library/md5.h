//  From code at:    https://rosettacode.org/wiki/MD5#C
//  Refactored by Chris.McDonald@uwa.edu.au
//  simple enough that I can almost understand it!

#include <stdlib.h>		// defines  size_t

#define MD5_BYTELEN     16
#define MD5_STRLEN      32

//  CALCULATE THE MD5 DIGEST OF input BUFFER, LEAVE RESULT IN md5_result
extern  void    *MD5_buffer(const char *input, size_t len, void *md5_result);

//  RETURNS A 'HUMAN-READABLE' FORMATTED STRING OF AN MD5 DIGEST
extern  char    *MD5_format(const void *md5_result);

//  RETURNS A 'HUMAN-READABLE' FORMATTED STRING OF DIGEST OF A STRING
extern  char    *MD5_str(const char *str);

//  RETURNS A 'HUMAN-READABLE' FORMATTED STRING OF DIGEST OF A FILE'S CONTENTS
extern  char    *MD5_file(const char *filenm);

#if	defined(WANT_TESTING)
extern	void	MD5_TESTALL(void);
#endif
