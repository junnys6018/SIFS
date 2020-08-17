#include "sifsutils.h"

// Returns blockID of file with same MD5. If no file is found, function returns SIFS_ROOTDIR_BLOCKID
SIFS_BLOCKID search_MD5(SIFS_VOLUME_HEADER header, SIFS_BIT* bitmap, FILE* vol, const unsigned char* md5_digest)
{
	// For every block
	for (SIFS_BLOCKID id = 0; id < header.nblocks; id++)
	{
		// If the block is a file
		if (bitmap[id] == SIFS_FILE)
		{
			SIFS_FILEBLOCK file = get_fileblock(header, vol, id);
			bool success = true;
			// If the block shares the same md5_digest
			for (int i = 0; i < MD5_BYTELEN; i++)
			{
				if (file.md5[i] != md5_digest[i])
					success = false;
			}

			if (success)
				return id;
		}
	}
	return SIFS_ROOTDIR_BLOCKID;
}

// add a copy of a new file to an existing volume
int SIFS_writefile(const char *volumename, const char *pathname,
		   void *data, size_t nbytes)
{
	// Check arguments
	if (volumename == NULL || pathname == NULL || *volumename == '\0' || *pathname == '\0' || nbytes == 0)
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

	// Split pathname into its path and name
	char* dirpath, * name;
	if (!split_filepath(pathname, &dirpath, &name))
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
	SIFS_BLOCKID dblockID = (dirpath) ? find_dir(header, bitmap, vol, SIFS_ROOTDIR_BLOCKID, dirpath, &err) :
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

	SIFS_DIRBLOCK dblock = get_dirblock(header, vol, dblockID);

	// Check if we can fit another entry
	if (dblock.nentries == SIFS_MAX_ENTRIES)
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
	for (uint32_t i = 0; i < dblock.nentries; i++)
	{
		SIFS_BLOCKID entryID = dblock.entries[i].blockID;
		// If the entry is a directory block
		if (bitmap[entryID] == SIFS_DIR)
		{
			SIFS_DIRBLOCK entrydir = get_dirblock(header, vol, entryID);

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
			// If the entry name is the same as our given name
			if (strcmp(entryfile.filenames[dblock.entries[i].fileindex], name) == 0)
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

	// Calculate md5 digest
	unsigned char md5_digest[MD5_BYTELEN];
	MD5_buffer(data, nbytes, md5_digest);
	
	// Attempt to find fileblockID with same md5 digest
	SIFS_BLOCKID fileID = search_MD5(header, bitmap, vol, md5_digest);
	SIFS_FILEBLOCK fblock;

	// Configure fblock and dblock
	if (fileID == SIFS_ROOTDIR_BLOCKID)
	{
		// No file with the same md5 digest found. Create new file block
		memset(&fblock, 0, sizeof(SIFS_FILEBLOCK));

		// Find a free block
		bool foundblock = false;
		for (SIFS_BLOCKID i = 0; i < header.nblocks; i++)
		{
			if (bitmap[i] == SIFS_UNUSED)
			{
				foundblock = true;
				fileID = i;
				bitmap[fileID] = SIFS_FILE;

				fblock.modtime = time(NULL);
				fblock.length = nbytes;
				memcpy(fblock.md5, md5_digest, MD5_BYTELEN);

				// Find a contigious block of memory for data
				int nblocks = (nbytes + header.blocksize - 1) / header.blocksize; // Round up 
				uint32_t currentlength = 0;
				SIFS_BLOCKID firstblockID;
				bool success = false;
				// Start search from i + 1 th BLOCKID as all previous blocks are in use 
				for (uint32_t j =  i + 1; j < header.nblocks; j++)
				{
					if (bitmap[j] == SIFS_UNUSED)
						currentlength++;
					else
						currentlength = 0;

					if (currentlength == nblocks)
					{
						success = true;
						firstblockID = j - currentlength + 1;
						break;
					}
				}
				if (!success)
				{
					SIFS_errno = SIFS_ENOSPC;
					free(bitmap);
					if (dirpath)
						free(dirpath);
					free(name);
					fclose(vol);
					return 1;
				}
				fblock.firstblockID = firstblockID;

				strcpy(fblock.filenames[fblock.nfiles], name);
				dblock.entries[dblock.nentries].blockID = fileID;
				dblock.entries[dblock.nentries].fileindex = fblock.nfiles;
				dblock.modtime = time(NULL);

				dblock.nentries++;
				fblock.nfiles++;

				// Write bitmap to volume
				for (SIFS_BLOCKID id = firstblockID; id < firstblockID + nblocks; id++)
				{
					bitmap[id] = SIFS_DATABLOCK;
				}
				fseek(vol, sizeof(SIFS_VOLUME_HEADER), SEEK_SET);
				fwrite(bitmap, sizeof(SIFS_BIT), header.nblocks, vol);

				// Write data to volume
				long offset = sizeof(SIFS_VOLUME_HEADER) + header.nblocks + firstblockID * header.blocksize;
				fseek(vol, offset, SEEK_SET);
				fwrite(data, 1, nbytes, vol);

				break;
			}
		}
		if (!foundblock)
		{
			SIFS_errno = SIFS_ENOSPC;
			free(bitmap);
			if (dirpath)
				free(dirpath);
			free(name);
			fclose(vol);
			return 1;
		}
	}
	else
	{
		fblock = get_fileblock(header, vol, fileID);

		if (fblock.nfiles == SIFS_MAX_ENTRIES)
		{
			SIFS_errno = SIFS_EMAXENTRY;
			free(bitmap);
			if (dirpath)
				free(dirpath);
			free(name);
			fclose(vol);
			return 1;
		}

		strcpy(fblock.filenames[fblock.nfiles], name);
		dblock.entries[dblock.nentries].blockID = fileID;
		dblock.entries[dblock.nentries].fileindex = fblock.nfiles;
		dblock.modtime = time(NULL);

		dblock.nentries++;
		fblock.nfiles++;
	}

	// Write dblock and fblock to volume
	fseek(vol, sizeof(SIFS_VOLUME_HEADER) + header.nblocks + header.blocksize * fileID, SEEK_SET);
	fwrite(&fblock, sizeof(SIFS_FILEBLOCK), 1, vol);

	fseek(vol, sizeof(SIFS_VOLUME_HEADER) + header.nblocks + header.blocksize * dblockID, SEEK_SET);
	fwrite(&dblock, sizeof(SIFS_DIRBLOCK), 1, vol);

	free(bitmap);
	if (dirpath)
		free(dirpath);
	free(name);
	fclose(vol);
    return 0;
}
