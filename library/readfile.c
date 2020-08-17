#include "sifsutils.h"

// read the contents of an existing file from an existing volume
int SIFS_readfile(const char *volumename, const char *pathname,
		  void **data, size_t *nbytes)
{
	// Check arguments
	if (volumename == NULL || pathname == NULL || *volumename == '\0' ||
		*pathname == '\0' || data == NULL || nbytes == NULL)
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

	// Split pathname
	char* dirpath, * name;
	if (!split_filepath(pathname, &dirpath, &name))
	{
		SIFS_errno = SIFS_ENOMEM;
		free(bitmap);
		fclose(vol);
		return 1;
	}

	int err = SIFS_EOK;
	// If dirpath is NULL we are working in the root directory
	SIFS_BLOCKID dir = dirpath == NULL ? SIFS_ROOTDIR_BLOCKID :
		find_dir(header, bitmap, vol, SIFS_ROOTDIR_BLOCKID, dirpath, &err);
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
	
	SIFS_BLOCKID fileID = find_file(header, bitmap, vol, dir, name, &err);
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

	SIFS_FILEBLOCK fblock = get_fileblock(header, vol, fileID);
	*nbytes = fblock.length;
	*data = malloc(*nbytes);

	// Read file into data
	long offset = sizeof(SIFS_VOLUME_HEADER) + header.nblocks + fblock.firstblockID * header.blocksize;
	fseek(vol, offset, SEEK_SET);
	fread(*data, 1, *nbytes, vol);

	free(bitmap);
	if (dirpath)
		free(dirpath);
	free(name);
	fclose(vol);
	return 0;
}
