#include "sifsutils.h"

// remove an existing directory from an existing volume
int SIFS_rmdir(const char* volumename, const char* dirname)
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

	// Find SIFS_BLOCKID of dirpath
	int err = SIFS_EOK;
	SIFS_BLOCKID childID = find_dir(header, bitmap, vol, SIFS_ROOTDIR_BLOCKID, dirname, &err);
	if (err != SIFS_EOK)
	{
		SIFS_errno = err;
		free(bitmap);
		fclose(vol);
		return 1;
	}

	SIFS_DIRBLOCK childblock = get_dirblock(header, vol, childID);

	// Check if childblock has any entries
	if (childblock.nentries != 0)
	{
		SIFS_errno = SIFS_ENOTEMPTY;
		free(bitmap);
		fclose(vol);
		return 1;
	}

	// Split dirname into its path and name
	char* parentPath, * name;
	if (!split_filepath(dirname, &parentPath, &name))
	{
		SIFS_errno = SIFS_ENOMEM;
		free(bitmap);
		fclose(vol);
		return 1;
	}

	// If parentPath is NULL the parent directory is ROOT
	SIFS_BLOCKID parentID = parentPath ? find_dir(header, bitmap, vol, SIFS_ROOTDIR_BLOCKID, parentPath, &err) :
		SIFS_ROOTDIR_BLOCKID;
	if (err != SIFS_EOK)
	{
		SIFS_errno = err;
		free(bitmap);
		if (parentPath)
			free(parentPath);
		free(name);
		fclose(vol);
		return 1;
	}
	SIFS_DIRBLOCK parentBlock = get_dirblock(header, vol, parentID);

	// Remove child directory entry
	for (uint32_t i = 0; i < parentBlock.nentries; i++)
	{
		SIFS_BLOCKID entryID = parentBlock.entries[i].blockID;
		if (entryID == childID)
		{
			for (uint32_t j = i; j < parentBlock.nentries - 1; j++)
			{
				parentBlock.entries[j].blockID = parentBlock.entries[j + 1].blockID;
				parentBlock.entries[j].fileindex = parentBlock.entries[j + 1].fileindex;
			}
			parentBlock.nentries--;
			break;
		}
	}
	// Update modificationtime
	parentBlock.modtime = time(NULL);

	// Write parentBlock to volume
	fseek(vol, sizeof(SIFS_VOLUME_HEADER) + header.nblocks + parentID * header.blocksize, SEEK_SET);
	fwrite(&parentBlock, sizeof(SIFS_DIRBLOCK), 1, vol);

	// Clear bitmap bit
	fseek(vol, sizeof(SIFS_VOLUME_HEADER) + sizeof(SIFS_BIT) * childID, SEEK_SET);
	SIFS_BIT unused = SIFS_UNUSED;
	fwrite(&unused, sizeof(SIFS_BIT), 1, vol);

	// Clear child block
	char oneblock[header.blocksize];
	memset(oneblock, 0, header.blocksize);
	fseek(vol, sizeof(SIFS_VOLUME_HEADER) + header.nblocks + childID * header.blocksize, SEEK_SET);
	fwrite(oneblock, 1, header.blocksize, vol);
	
	free(bitmap);
	if (parentPath)
		free(parentPath);
	free(name);
	fclose(vol);

	return 0;
}
