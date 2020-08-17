#include "sifsutils.h"
#include <stdbool.h>

// make a new directory within an existing volume
int SIFS_mkdir(const char *volumename, const char *dirname)
{
	// Check arguments
	if (volumename == NULL || dirname == NULL || *volumename == '\0' || *dirname == '\0')
	{
		SIFS_errno = SIFS_EINVAL;
		return 1;
	}

	// Try to open volume
	FILE* vol = fopen(volumename, "r+");
	if (!vol)
	{
		SIFS_errno = SIFS_ENOVOL;
		return 1;
	}

	// Read and validate header
	SIFS_VOLUME_HEADER header = get_volumeheader(vol);
	if (header.blocksize < SIFS_MIN_BLOCKSIZE || header.nblocks == 0)
	{
		SIFS_errno = SIFS_ENOTVOL;
		fclose(vol);
		return 1;
	}

	// Read and validate bitmap
	SIFS_BIT* bitmap;
	get_volumebitmap(vol, &bitmap);
	if (!validate_bitmap(bitmap, header.nblocks))
	{
		SIFS_errno = SIFS_ENOTVOL;
		free(bitmap);
		fclose(vol);
		return 1;
	}

	// Split dirname into its path and name
	char* dirpath, *name;
	if (!split_filepath(dirname, &dirpath, &name))
	{
		SIFS_errno = SIFS_ENOMEM;
		free(bitmap);
		fclose(vol);
		return 1;
	}

	// Check if specified name is too long
	if (strlen(name) + 1 > SIFS_MAX_NAME_LENGTH)
	{
		SIFS_errno = SIFS_EINVAL;
		free(bitmap);
		if (dirpath)
			free(dirpath);
		free(name);
		fclose(vol);
		return 1;
	}

	// Find SIFS_BLOCKID of dirpath
	int err = SIFS_EOK;
	// If dirpath is NULL we are working in the root directory
	SIFS_BLOCKID pdirID = (dirpath) ? find_dir(header, bitmap, vol, SIFS_ROOTDIR_BLOCKID, dirpath, &err) : 
		SIFS_ROOTDIR_BLOCKID;
	if (err != SIFS_EOK)
	{
		SIFS_errno = err;
		free(bitmap);
		if (dirpath)
			free(dirpath);
		free(name);
		fclose(vol);
		return 1;
	}

	// Get SIFS_DIRBLOCK of dirpath
	SIFS_DIRBLOCK pdir = get_dirblock(header, vol, pdirID);

	// Check if we can fit another entry
	if (pdir.nentries == SIFS_MAX_ENTRIES)
	{
		SIFS_errno = SIFS_EMAXENTRY;
		free(bitmap);
		if (dirpath)
			free(dirpath);
		free(name);
		fclose(vol);
		return 1;
	}

	// Check if name already exists
	for (uint32_t i = 0; i < pdir.nentries; i++)
	{
		SIFS_BLOCKID entryID = pdir.entries[i].blockID;
		// If the entry is a directory block
		if (bitmap[entryID] == SIFS_DIR)
		{
			SIFS_DIRBLOCK entrydir = get_dirblock(header, vol, entryID);
			// If the entry name is the same as our given name
			if (strcmp(entrydir.name, name) == 0)
			{
				SIFS_errno = SIFS_EEXIST;
				free(bitmap);
				if (dirpath)
					free(dirpath);
				free(name);
				fclose(vol);
				return 1;
			}

		}
		// If the entry is a file block
		else if (bitmap[entryID] == SIFS_FILE)
		{
			SIFS_FILEBLOCK entryfile = get_fileblock(header, vol, entryID);
			if (strcmp(entryfile.filenames[pdir.entries[i].fileindex], name) == 0)
			{
				SIFS_errno = SIFS_EEXIST;
				free(bitmap);
				if (dirpath)
					free(dirpath);
				free(name);
				fclose(vol);
				return 1;
			}
		}
		// directory points to an invalid block. Volume is corrupted
		else
		{
			SIFS_errno = SIFS_ENOTVOL;
			free(bitmap);
			if (dirpath)
				free(dirpath);
			free(name);
			fclose(vol);
			return 1;
		}
	}

	// Find an available block for child dir
	SIFS_BLOCKID cdirID = 0;
	bool success = false;
	for (/*blank*/; cdirID < header.nblocks; cdirID++)
	{
		if (bitmap[cdirID] == SIFS_UNUSED)
		{
			// Write to bitmap
			bitmap[cdirID] = SIFS_DIR;
			fseek(vol, sizeof(SIFS_VOLUME_HEADER), SEEK_SET);
			fwrite(bitmap, sizeof(SIFS_BIT), header.nblocks, vol);
			success = true;
			break;
		}
	}

	if (success == false)
	{
		SIFS_errno = SIFS_ENOSPC;
		free(bitmap);
		if (dirpath)
			free(dirpath);
		free(name);
		fclose(vol);
		return 1;
	}

	// Update parent directory
	pdir.entries[pdir.nentries].blockID = cdirID;
	pdir.nentries++;
	pdir.modtime = time(NULL);
	// Write parent directory to volume
	fseek(vol, sizeof(SIFS_VOLUME_HEADER) + header.nblocks + header.blocksize * pdirID, SEEK_SET);
	fwrite(&pdir, sizeof(SIFS_DIRBLOCK), 1, vol);

	// Allocate block for new directory
	SIFS_DIRBLOCK cdir;
	memset(&cdir, 0, sizeof(SIFS_DIRBLOCK));
	strcpy(cdir.name, name);
	cdir.modtime = time(NULL);
	cdir.nentries = 0;

	// Write dirblock to volume
	fseek(vol, sizeof(SIFS_VOLUME_HEADER) + header.nblocks + header.blocksize * cdirID, SEEK_SET);
	fwrite(&cdir, sizeof(SIFS_DIRBLOCK), 1, vol);

	free(bitmap);
	if (dirpath)
		free(dirpath);
	free(name);
	fclose(vol);
	return 0;
}
