#include <stdio.h>
#include "../sifs.h"

int	SIFS_errno = SIFS_EOK;

char* SIFS_errlist[] = {
	"OK",						// SIFS_EOK
	"Invalid argument",				// SIFS_EINVAL
	"Cannot create volume",				// SIFS_ECREATE
	"No such volume",				// SIFS_ENOVOL
	"No such file or directory entry",		// SIFS_ENOENT
	"Volume, file or directory already exists",	// SIFS_EEXIST
	"Not a volume",					// SIFS_ENOTVOL
	"Not a directory",				// SIFS_ENOTDIR
	"Not a file",					// SIFS_ENOTFILE
	"Too many directory or file entries",		// SIFS_EMAXENTRY
	"No space left on volume",			// SIFS_ENOSPC
	"Memory allocation failed",			// SIFS_ENOMEM
		"Not yet implemented",                          // SIFS_ENOTYET
	"Directory is not empty",			// SIFS_ENOTEMPTY
};

#define	SIFS_NERRS	(sizeof(SIFS_errlist) / sizeof(SIFS_errlist[0]))

//  THE FUNCTION SIFS_perror() PRODUCES A MESSAGE ON THE STANDARD ERROR OUTPUT,
//  DESCRIBING THE LAST ERROR ENCOUNTERED.
//  IF PROVIDED WITH A NON-NULL PREFIX, IT IS PRINTED BEFORE THE MESSAGE

void SIFS_perror(const char* prefix)
{
	if (SIFS_errno >= SIFS_EOK && SIFS_errno < SIFS_NERRS) {
		if (prefix != NULL && *prefix != '\0') {
			fprintf(stderr, "%s: ", prefix);
		}
		fprintf(stderr, "%s\n", SIFS_errlist[SIFS_errno]);
	}
}
