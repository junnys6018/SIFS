#include "sifsutils.h"

// get information about a requested directory
int SIFS_dirinfo(const char *volumename, const char *pathname,
                 char ***entrynames, uint32_t *nentries, time_t *modtime)
{
	// Check arguments
	if (volumename == NULL || pathname == NULL || *volumename == '\0' ||
		entrynames == NULL || nentries == NULL || modtime == NULL)
	{
		SIFS_errno = SIFS_EINVAL;
		return 1;
	}

	// Try to open volume
	FILE* vol = fopen(volumename, "r");
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

	int err = SIFS_EOK;
	// If filepath is '\0' we are working in the root directory
	SIFS_BLOCKID dir = (*pathname == '\0') ? SIFS_ROOTDIR_BLOCKID :
		find_dir(header, bitmap, vol, SIFS_ROOTDIR_BLOCKID, pathname, &err);
	if (err != SIFS_EOK)
	{
		SIFS_errno = err;
		free(bitmap);
		fclose(vol);
		return 1;
	}
	SIFS_DIRBLOCK block = get_dirblock(header, vol, dir);

	// Allocate memory for entrynames
	*entrynames = malloc(sizeof(char*) * block.nentries);
	if (!(*entrynames))
	{
		SIFS_errno = SIFS_ENOMEM;
		free(bitmap);
		fclose(vol);
		return 1;
	}
	for (int i = 0; i < block.nentries; i++)
	{
		(*entrynames)[i] = malloc(sizeof(char) * SIFS_MAX_NAME_LENGTH);
		if (!(*entrynames)[i])
		{
			SIFS_errno = SIFS_ENOMEM;
			// Deallocate memory
			for (int j = 0; j < i; j++)
			{
				free((*entrynames)[j]);
			}
			free(*entrynames);
			free(bitmap);
			fclose(vol);
			return 1;
		}
	}

	// Assign entrynames
	for (int i = 0; i < block.nentries; i++)
	{
		SIFS_BLOCKID id = block.entries[i].blockID;
		if (bitmap[id] == SIFS_DIR)
		{
			SIFS_DIRBLOCK entry = get_dirblock(header, vol, id);
			strcpy((*entrynames)[i], entry.name);
		}
		else if (bitmap[id] == SIFS_FILE)
		{
			SIFS_FILEBLOCK entry = get_fileblock(header, vol, id);
			strcpy((*entrynames)[i], entry.filenames[block.entries[i].fileindex]);
		}
	}

	// Assign other values
	*nentries = block.nentries;
	*modtime = block.modtime;

	free(bitmap);
	fclose(vol);

    return 0;
}
