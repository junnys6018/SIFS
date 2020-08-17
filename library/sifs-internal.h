#include "../sifs.h"
#include "md5.h"

//  CONCRETE STRUCTURES AND CONSTANTS USED THROUGHOUT THE SIFS LIBRARY.
//  DO NOT CHANGE ANYTHING IN THIS FILE.

#define	SIFS_MIN_BLOCKSIZE	1024

#define	SIFS_MAX_NAME_LENGTH	32	// including the NULL byte
#define	SIFS_MAX_ENTRIES	24	// for both directory and file entries

#define SIFS_UNUSED		'u'
#define SIFS_DIR		'd'
#define SIFS_FILE		'f'
#define SIFS_DATABLOCK		'b'

#define SIFS_ROOTDIR_BLOCKID	0

typedef struct {
    size_t		blocksize;
    uint32_t		nblocks;
} SIFS_VOLUME_HEADER;

typedef char		SIFS_BIT;	// SIFS_UNUSED, SIFS_DIR, ...
typedef uint32_t	SIFS_BLOCKID;

//  DEFINITION OF EACH DIRECTORY BLOCK - MUST FIT INSIDE A SINGLE BLOCK
typedef struct {
    char		name[SIFS_MAX_NAME_LENGTH];
    time_t		modtime;	// time last modified <- time()

    uint32_t		nentries;
    struct {
	SIFS_BLOCKID	blockID;	// of the entry's subdirectory or file
	uint32_t	fileindex;	// into a SIFS_FILEBLOCK's filenames[]
    } entries[SIFS_MAX_ENTRIES];
} SIFS_DIRBLOCK;

//  DEFINITION OF EACH FILE BLOCK - MUST FIT INSIDE A SINGLE BLOCK
typedef struct {
    time_t		modtime;	// time first file added <- time()
    size_t		length;		// length of files' contents in bytes

    unsigned char	md5[MD5_BYTELEN];
    SIFS_BLOCKID	firstblockID;

    uint32_t		nfiles;		// n files with identical contents
    char		filenames[SIFS_MAX_ENTRIES][SIFS_MAX_NAME_LENGTH];
} SIFS_FILEBLOCK;

