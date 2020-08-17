#include <stdlib.h>
#include <stdint.h>
#include <time.h>

//  THE SINGLE INSTANCE FILE SYSTEM (SIFS) APPLICATION PROGRAMMING INTERFACE (API)

//  DO NOT CHANGE ANYTHING IN THIS FILE -
//  YOU MAY ADD THINGS, BUT DON'T CHANGE ANYTHING (else your project can't be tested)


//  MAKE A NEW VOLUME
extern	int SIFS_mkvolume(const char *volumename, size_t blocksize, uint32_t nblocks);

//  MAKE A NEW DIRECTORY WITHIN AN EXISTING VOLUME
extern	int SIFS_mkdir(const char *volumename, const char *dirname);

//  REMOVE AN EXISTING DIRECTORY FROM AN EXISTING VOLUME
extern	int SIFS_rmdir(const char *volumename, const char *dirname);

//  ADD A COPY OF A NEW FILE TO AN EXISTING VOLUME
extern	int SIFS_writefile(const char *volumename, const char *pathname,
			   void *data, size_t nbytes);

//  READ THE CONTENTS OF AN EXISTING FILE FROM AN EXISTING VOLUME
extern	int SIFS_readfile(const char *volumename, const char *pathname,
			  void **data, size_t *nbytes);

//  REMOVE AN EXISTING FILE FROM AN EXISTING VOLUME
extern	int SIFS_rmfile(const char *volumename, const char *pathname);

//  GET INFORMATION ABOUT A REQUESTED DIRECTORY
extern	int SIFS_dirinfo(const char *volumename, const char *pathname,
			 char ***entrynames, uint32_t *nentries, time_t *modtime);

//  GET INFORMATION ABOUT A REQUESTED FILE
extern	int SIFS_fileinfo(const char *volumename, const char *pathname,
			  size_t *length, time_t *modtime);

//  DEFRAGMENT THE VOLUME
extern  int SIFS_defrag(const char* volumename);

//  ON SUCCESS, EACH OF THE ABOVE FUNCTIONS RETURN 0.
//  ON FAILURE, EACH FUNCTION RETURNS 1 AND SETS SIFS_errno
extern	int		SIFS_errno;

#define	SIFS_EOK	0
#define	SIFS_EINVAL	1	// Invalid argument
#define	SIFS_ECREATE	2	// Cannot create volume
#define	SIFS_ENOVOL	3	// No such volume
#define	SIFS_ENOENT	4	// No such file or directory entry
#define	SIFS_EEXIST	5	// Volume, file or directory already exists
#define	SIFS_ENOTVOL	6	// Not a volume
#define	SIFS_ENOTDIR	7	// Not a directory
#define	SIFS_ENOTFILE	8	// Not a file
#define	SIFS_EMAXENTRY	9	// Too many directory or file entries
#define	SIFS_ENOSPC	10	// No space left on volume
#define	SIFS_ENOMEM	11	// Memory allocation failed
#define	SIFS_ENOTYET	12	// Not yet implemented
#define	SIFS_ENOTEMPTY	13	// Directory is not empty


//  THE FUNCTION SIFS_perror() PRODUCES A MESSAGE ON THE STANDARD ERROR OUTPUT,
//  DESCRIBING THE LAST ERROR ENCOUNTERED.
//  IF PROVIDED WITH A NON-NULL PREFIX, IT IS PRINTED BEFORE THE MESSAGE
extern	void		SIFS_perror(const char *prefix);

