#include "sifsutils.h"

// get information about a requested file
int SIFS_fileinfo(const char *volumename, const char *pathname,
		  size_t *length, time_t *modtime)
{
	// Check arguments
	if (volumename == NULL || pathname == NULL || *volumename == '\0' ||
		*pathname == '\0' || length == NULL || modtime == NULL)
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
	char* pdirpath, * name;
	if (!split_filepath(pathname, &pdirpath, &name))
	{
		SIFS_errno = SIFS_ENOMEM;
		free(bitmap);
		fclose(vol);
		return 1;
	}

	int err = SIFS_EOK;
	// If dirpath is NULL we are working in the root directory
	SIFS_BLOCKID dir = pdirpath == NULL ? SIFS_ROOTDIR_BLOCKID :
		find_dir(header, bitmap, vol, SIFS_ROOTDIR_BLOCKID, pdirpath, &err);
	if (err != SIFS_EOK)
	{
		SIFS_errno = err;
		free(bitmap);
		if (pdirpath)
			free(pdirpath);
		free(name);
		fclose(vol);
		return 1;
	}
	
	SIFS_BLOCKID fileID = find_file(header, bitmap, vol, dir, name, &err);
	if (err != SIFS_EOK)
	{
		SIFS_errno = err;
		free(bitmap);
		if (pdirpath)
			free(pdirpath);
		free(name);
		fclose(vol);
		return 1;
	}

	SIFS_FILEBLOCK fblock = get_fileblock(header, vol, fileID);

	*length = fblock.length;
	*modtime = fblock.modtime;

	free(bitmap);
	if (pdirpath)
		free(pdirpath);
	free(name);
	fclose(vol);
	return 0;
}
