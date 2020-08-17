#include "sifsutils.h"

// remove an existing file from an existing volume
int SIFS_rmfile(const char *volumename, const char *pathname)
{
	// Check arguments
	if (volumename == NULL || pathname == NULL || *volumename == '\0' || *pathname == '\0')
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

	// Attempt to find file entry specified
	SIFS_BLOCKID fileID;
	uint32_t fileindex;
	bool success = false;
	for (uint32_t i = 0; i < dblock.nentries; i++)
	{
		SIFS_BLOCKID entryID = dblock.entries[i].blockID;
		// If the entry is a file block
		if (bitmap[entryID] == SIFS_FILE)
		{
			SIFS_FILEBLOCK entryfile = get_fileblock(header, vol, entryID);
			// If the entry name is the same as our given name
			if (strcmp(entryfile.filenames[dblock.entries[i].fileindex], name) == 0)
			{
				fileID = entryID;
				fileindex = dblock.entries[i].fileindex;
				success = true;

				// Delete this entry from dblock
				for (uint32_t j = i; j < dblock.nentries - 1; j++)
				{
					dblock.entries[j].blockID = dblock.entries[j + 1].blockID;
					dblock.entries[j].fileindex = dblock.entries[j + 1].fileindex;
				}
				dblock.nentries--;

				dblock.modtime = time(NULL);

				// Write dblock to volume
				fseek(vol, sizeof(SIFS_VOLUME_HEADER) + header.nblocks + dblockID * header.blocksize, SEEK_SET);
				fwrite(&dblock, sizeof(SIFS_DIRBLOCK), 1, vol);
				break;
			}
		}
		else if (bitmap[entryID] == SIFS_DIR)
		{
			SIFS_DIRBLOCK entrydir = get_dirblock(header, vol, entryID);
			if (strcmp(entrydir.name, name) == 0)
			{
				SIFS_errno = SIFS_ENOTFILE;
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
	if (!success)
	{
		SIFS_errno = SIFS_ENOENT;
		free(bitmap);
		if (dirpath)
			free(dirpath);
		free(name);
		fclose(vol);
		return 1;
	}

	SIFS_FILEBLOCK fblock = get_fileblock(header, vol, fileID);
	if (fblock.nfiles == 1)
	{
		// Only this directory references the specifed file. We can safely delete it

		// Update bitmap
		bitmap[fileID] = SIFS_UNUSED;
		uint32_t nblocks = (fblock.length + header.blocksize - 1) / header.blocksize; // Round up
		for (SIFS_BLOCKID id = fblock.firstblockID; id < fblock.firstblockID + nblocks; id++)
		{
			bitmap[id] = SIFS_UNUSED;
		}

		// Write bitmap to volume
		fseek(vol, sizeof(SIFS_VOLUME_HEADER), SEEK_SET);
		fwrite(bitmap, sizeof(SIFS_BIT), header.nblocks, vol);

		// Clear fileblock from volume (is this nessesary?)
		//unsigned char* clearblock = malloc(header.blocksize);
		//fseek(vol,sizeof(SIFS_VOLUME_HEADER)+

		//free(clearblock);
	}
	else
	{
		// Update fblock filenames
		for (uint32_t i = fileindex; i < fblock.nfiles - 1; i++)
		{
			strcpy(fblock.filenames[i], fblock.filenames[i + 1]);
		}
		fblock.nfiles--;

		// Write fblock to volume
		fseek(vol, sizeof(SIFS_VOLUME_HEADER) + header.nblocks + fileID * header.blocksize, SEEK_SET);
		fwrite(&fblock, sizeof(SIFS_FILEBLOCK), 1, vol);

		// More than one directory points to fblock. We need to update thier entries accordingly
		uint32_t dirs_processed = 0;
		for (uint32_t i = 0; i < header.nblocks && dirs_processed < fblock.nfiles; i++)
		{
			// If the block is a directory
			if (bitmap[i] == SIFS_DIR)
			{
				SIFS_DIRBLOCK d = get_dirblock(header, vol, i);
				for (uint32_t entry = 0; entry < d.nentries; entry++)
				{
					// If d points to fileID
					if (d.entries[entry].blockID == fileID)
					{
						dirs_processed++;
						// ... and if its fileindex into filenames occured after the deleted filename
						if (d.entries[entry].fileindex > fileindex)
						{
							// .. adjust it accordingly
							d.entries[entry].fileindex--;

							// And write d to volume
							fseek(vol, sizeof(SIFS_VOLUME_HEADER) + header.nblocks + i * header.blocksize, SEEK_SET);
							fwrite(&d, sizeof(SIFS_DIRBLOCK), 1, vol);
						}
					}
				}
			}
		}
	}
	
	free(bitmap);
	if (dirpath)
		free(dirpath);
	free(name);
	fclose(vol);
	return 0;
}
