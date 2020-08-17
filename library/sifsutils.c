//  CITS2002 Project 2 2019
//  Name(s):             Ethan Lim
//  Student number(s):   22701593
#include "sifsutils.h"

// Returns volume header of a valid FILE* volume
SIFS_VOLUME_HEADER get_volumeheader(FILE* vol)
{
	// Seek to beggining
	fseek(vol, 0, SEEK_SET);

	// Read header
	SIFS_VOLUME_HEADER header;
	fread(&header, sizeof(SIFS_VOLUME_HEADER), 1, vol);

	return header;
}

// Stores volume bitmap of a valid FILE* volume
void get_volumebitmap(FILE* vol, SIFS_BIT** bitmap)
{
	SIFS_VOLUME_HEADER header = get_volumeheader(vol);
	*bitmap = malloc(header.nblocks);
	// vol file pointer already points to beginning of bitmap
	fread(*bitmap, sizeof(SIFS_BIT), header.nblocks, vol);
}

// Returns SIFS_DIRBLOCK of directory block pointed to by dir
SIFS_DIRBLOCK get_dirblock(SIFS_VOLUME_HEADER header, FILE* vol, SIFS_BLOCKID dir)
{
	// Seek to beggining of blocks
	int offset = sizeof(SIFS_VOLUME_HEADER) + header.nblocks * sizeof(SIFS_BIT);
	fseek(vol, offset, SEEK_SET);

	// Seek to pointed block
	offset = dir * header.blocksize;
	fseek(vol, offset, SEEK_CUR);

	// Read block
	SIFS_DIRBLOCK block;
	fread(&block, sizeof(SIFS_DIRBLOCK), 1, vol);

	return block;
}

// Returns SIFS_FILEBLOCK of file block pointed to by file
SIFS_FILEBLOCK get_fileblock(SIFS_VOLUME_HEADER header, FILE* vol, SIFS_BLOCKID file)
{
	// Seek to beggining of blocks
	int offset = sizeof(SIFS_VOLUME_HEADER) + header.nblocks * sizeof(SIFS_BIT);
	fseek(vol, offset, SEEK_SET);

	// Seek to pointed block
	offset = file * header.blocksize;
	fseek(vol, offset, SEEK_CUR);

	// Read block
	SIFS_FILEBLOCK block;
	fread(&block, sizeof(SIFS_FILEBLOCK), 1, vol);

	return block;
}

// Returns true if bitmap is valid, false otherwise
bool validate_bitmap(SIFS_BIT* bitmap, uint32_t nblocks)
{
	for (uint32_t i = 0; i < nblocks; i++)
	{
		SIFS_BIT b = bitmap[i];
		if (b != SIFS_DIR && b != SIFS_FILE && b != SIFS_UNUSED && b != SIFS_DATABLOCK)
			return false;
	}
	return true;
}

// Returns the SIFS_BLOCKID of the directory pointed to by filepath. Note filepath is relative to dir
SIFS_BLOCKID find_dir(SIFS_VOLUME_HEADER header, SIFS_BIT* bitmap, FILE* vol, SIFS_BLOCKID dir, const char* filepath, int* err)
{
	if (bitmap[dir] != SIFS_DIR || filepath == NULL || *filepath == '\0')
	{
		*err = SIFS_EINVAL;
		return 0;
	}

	if (*filepath == '/')
	{
		filepath++;
	}

	// Find first directory pointed to by filepath. e.g. "PATH/TO/FILE" --> "PATH"
	char dirname[SIFS_MAX_NAME_LENGTH] = "";
	const char* pFirstSlash = strchr(filepath, '/');

	// If no slash was found, point pFirstSlash to the null byte
	pFirstSlash = pFirstSlash ? pFirstSlash : filepath + strlen(filepath);
	
	// Check if the directory name is too long
	if (pFirstSlash - filepath > SIFS_MAX_NAME_LENGTH)
	{
		*err = SIFS_EINVAL;
		return 0;
	}
	strncpy(dirname, filepath, pFirstSlash - filepath);

	SIFS_DIRBLOCK block = get_dirblock(header, vol, dir);
	SIFS_BLOCKID newdirID;
	bool success = false;
	// Attempt to find child directory
	for (uint32_t i = 0; i < block.nentries; i++)
	{
		SIFS_BLOCKID entryID = block.entries[i].blockID;
		if (bitmap[entryID] == SIFS_DIR)
		{
			SIFS_DIRBLOCK nextblock = get_dirblock(header, vol, entryID);
			if (strcmp(nextblock.name, dirname) == 0)
			{
				newdirID = entryID;
				success = true;
				break;
			}
		}
		// Check if supplied path was actually to a file
		else if (bitmap[entryID] == SIFS_FILE)
		{
			SIFS_FILEBLOCK fileblock = get_fileblock(header, vol, entryID);
			if (strcmp(fileblock.filenames[block.entries[i].fileindex], dirname) == 0)
			{
				*err = SIFS_ENOTDIR;
				return 0;
			}
		}
	}
	if (!success)
	{
		// No such directory was found
		*err = SIFS_ENOENT;
		return 0;
	}

	// Update filepath
	filepath += strlen(dirname);

	if (*filepath == '\0')
	{
		return newdirID;
		*err = SIFS_EOK;
	}
	else
	{
		return find_dir(header, bitmap, vol, newdirID, filepath, err);
	}
}

// Returns the SIFS_BLOCKID of the fileblock pointed to by dir with name filename
SIFS_BLOCKID find_file(SIFS_VOLUME_HEADER header, SIFS_BIT* bitmap, FILE* vol, SIFS_BLOCKID dir, const char* filename, int* err)
{
	SIFS_DIRBLOCK dblock = get_dirblock(header, vol, dir);
	for (uint32_t entry = 0; entry < dblock.nentries; entry++)
	{
		SIFS_BLOCKID id = dblock.entries[entry].blockID;
		if (bitmap[id] == SIFS_FILE)
		{
			SIFS_FILEBLOCK fblock = get_fileblock(header, vol, id);
			if (strcmp(filename, fblock.filenames[dblock.entries[entry].fileindex]) == 0)
			{
				*err = SIFS_EOK;
				return id;
			}
		}
		if (bitmap[id] == SIFS_DIR)
		{
			SIFS_DIRBLOCK entrydir = get_dirblock(header, vol, id);
			if (strcmp(filename, entrydir.name) == 0)
			{
				*err = SIFS_ENOTFILE;
				return 0;
			}
		}
	}
	// No such file was found
	*err = SIFS_ENOENT;
	return 0;
}

// Splits src by the last occurence of '/' character. If no '/' character was found,
// or if there is only a leading slash (e.g. "/file.txt" ) src is copied into name and *dirpath is set to NULL
bool split_filepath(const char* src, char** dirpath, char** name)
{
	// Skip leading '/' if it exists
	if (*src == '/')
		src++;

	// Find last instance of '/' character
	const char* pLastSlash = strrchr(src, '/');

	// Split dirname into its path and name
	if (pLastSlash)
	{
		size_t pathlen = pLastSlash - src;
		(*dirpath) = malloc(pathlen + 1); // For null byte
		memset(*dirpath, 0, pathlen + 1); // Clear 
		if (!(*dirpath))
		{
			return false;
		}
		(*name) = malloc(strlen(pLastSlash));
		memset(*name, 0, strlen(pLastSlash));
		if (!(*name))
		{
			free(*dirpath);
			return false;
		}

		strcpy(*name, pLastSlash + 1);
		strncpy(*dirpath, src, pathlen);
		(*dirpath)[pathlen + 1] = '\0';
	}
	else
	{
		*dirpath = NULL;
		*name = malloc(strlen(src) + 1);
		memset(*name, 0, strlen(src) + 1);
		if (!(*name))
		{
			return false;
		}

		strcpy(*name, src);
	}

	return true;
}
