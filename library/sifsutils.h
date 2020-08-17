#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "sifs-internal.h"

// Returns volume header of a valid FILE* volume
extern SIFS_VOLUME_HEADER get_volumeheader(FILE* vol);

// Stores volume bitmap of a valid FILE* volume
extern void get_volumebitmap(FILE* vol, SIFS_BIT** bitmap);

// Returns SIFS_DIRBLOCK of directory block pointed to by dir
extern SIFS_DIRBLOCK get_dirblock(SIFS_VOLUME_HEADER header, FILE* vol, SIFS_BLOCKID dir);

// Returns SIFS_FILEBLOCK of file block pointed to by file
extern SIFS_FILEBLOCK get_fileblock(SIFS_VOLUME_HEADER header, FILE* vol, SIFS_BLOCKID file);

// Returns true if bitmap is valid, false otherwise
extern bool validate_bitmap(SIFS_BIT* bitmap, uint32_t nblocks);

// Returns the SIFS_BLOCKID of the directory pointed to by filepath. Note filepath is relative to dir
extern SIFS_BLOCKID find_dir(SIFS_VOLUME_HEADER header, SIFS_BIT* bitmap, FILE* vol, SIFS_BLOCKID dir, const char* filepath, int* err);

// Returns the SIFS_BLOCKID of the fileblock pointed to by dir with name filename
extern SIFS_BLOCKID find_file(SIFS_VOLUME_HEADER header, SIFS_BIT* bitmap, FILE* vol, SIFS_BLOCKID dir, const char* filename, int* err);

// Splits src by the last occurence of '/' character. If no '/' character was found,
// src is copied into name and dirpath is set to NULL. Returns true if action was successful
extern bool split_filepath(const char* src, char** dirpath, char** name);
