#include "sifsutils.h"
#include <assert.h>

void shift_dir(SIFS_VOLUME_HEADER header, SIFS_BIT* bitmap, FILE* vol, SIFS_BLOCKID dir, uint32_t npos)
{
	assert(bitmap[dir] == SIFS_DIR);
	assert(dir > npos);

	// Find parent directory
	for (SIFS_BLOCKID id = 0; id < header.nblocks; id++)
	{
		if (bitmap[id] == SIFS_DIR)
		{
			SIFS_DIRBLOCK block = get_dirblock(header, vol, id);
			for (uint32_t entry = 0; entry < block.nentries; entry++)
			{
				if (block.entries[entry].blockID == dir)
				{
					// Update child entry
					block.entries[entry].blockID -= npos;
					// Write to volume
					fseek(vol, sizeof(SIFS_VOLUME_HEADER) + header.nblocks + id * header.blocksize, SEEK_SET);
					fwrite(&block, sizeof(SIFS_DIRBLOCK), 1, vol);
					goto BREAK_LOOP;
				}
			}
		}
	}
BREAK_LOOP:
	// Update and write bitmap
	bitmap[dir] = SIFS_UNUSED;
	bitmap[dir - npos] = SIFS_DIR;

	fseek(vol, sizeof(SIFS_VOLUME_HEADER), SEEK_SET);
	fwrite(bitmap, sizeof(SIFS_BIT), header.nblocks, vol);

	// Move directory block
	SIFS_DIRBLOCK child = get_dirblock(header, vol, dir);
	fseek(vol, sizeof(SIFS_VOLUME_HEADER) + header.nblocks + (dir - npos) * header.blocksize, SEEK_SET);
	fwrite(&child, sizeof(SIFS_DIRBLOCK), 1, vol);
}

void shift_file(SIFS_VOLUME_HEADER header, SIFS_BIT* bitmap, FILE* vol, SIFS_BLOCKID file, uint32_t npos)
{
	assert(bitmap[file] == SIFS_FILE);
	assert(file > npos);

	SIFS_FILEBLOCK fblock = get_fileblock(header, vol, file);

	// Update all directory entries that point to this file
	uint32_t ndirs_processed = 0;
	for (SIFS_BLOCKID id = 0; id < header.nblocks && ndirs_processed < fblock.nfiles; id++)
	{
		if (bitmap[id] == SIFS_DIR)
		{
			SIFS_DIRBLOCK dblock = get_dirblock(header, vol, id);
			for (uint32_t entry = 0; entry < dblock.nentries; entry++)
			{
				if (dblock.entries[entry].blockID == file)
				{
					ndirs_processed++;
					dblock.entries[entry].blockID -= npos;
				}
			}
			// Write to volume
			fseek(vol, sizeof(SIFS_VOLUME_HEADER) + header.nblocks + id * header.blocksize, SEEK_SET);
			fwrite(&dblock, sizeof(SIFS_DIRBLOCK), 1, vol);
		}
	}

	// Update and write bitmap
	bitmap[file] = SIFS_UNUSED;
	bitmap[file - npos] = SIFS_FILE;

	fseek(vol, sizeof(SIFS_VOLUME_HEADER), SEEK_SET);
	fwrite(bitmap, sizeof(SIFS_BIT), header.nblocks, vol);

	// Move file block
	fseek(vol, sizeof(SIFS_VOLUME_HEADER) + header.nblocks + (file - npos) * header.blocksize, SEEK_SET);
	fwrite(&fblock, sizeof(SIFS_FILEBLOCK), 1, vol);
}

void shift_data(SIFS_VOLUME_HEADER header, SIFS_BIT* bitmap, FILE* vol, SIFS_BLOCKID data, uint32_t npos)
{
	assert(bitmap[data] == SIFS_DATABLOCK);
	assert(data > npos);

	// Update and write bitmap
	bitmap[data] = SIFS_UNUSED;
	bitmap[data - npos] = SIFS_DATABLOCK;

	fseek(vol, sizeof(SIFS_VOLUME_HEADER), SEEK_SET);
	fwrite(bitmap, sizeof(SIFS_BIT), header.nblocks, vol);

	// Move datablock
	char* block = malloc(header.blocksize);
	
	fseek(vol, sizeof(SIFS_VOLUME_HEADER) + header.nblocks + data * header.blocksize, SEEK_SET);
	fread(block, 1, header.blocksize, vol);
	
	fseek(vol, sizeof(SIFS_VOLUME_HEADER) + header.nblocks + (data - npos) * header.blocksize, SEEK_SET);
	fwrite(block, 1, header.blocksize, vol);
}


// Defragments the volume
int SIFS_defrag(const char* volumename)
{
	// Check arguments
	if (volumename == NULL || *volumename == '\0')
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

	SIFS_BLOCKID maxIndex = 0;
	for (SIFS_BLOCKID i = 1; i < header.nblocks; i++)
	{
		if (bitmap[i] != SIFS_UNUSED)
			maxIndex = i;
	}

	// We scan through the volume and coalesce used blocks. Keeping track of the number of blocks we need to shift
	uint32_t consecutiveUnsued = 0;
	for (SIFS_BLOCKID i = 0; i <= maxIndex; i++)
	{
		if (bitmap[i] == SIFS_UNUSED)
			consecutiveUnsued++;
		else if (bitmap[i] != SIFS_UNUSED && consecutiveUnsued > 0)
		{
			if (bitmap[i] == SIFS_DIR)
			{
				shift_dir(header, bitmap, vol, i, consecutiveUnsued);
			}
			else if (bitmap[i] == SIFS_FILE)
			{
				shift_file(header, bitmap, vol, i, consecutiveUnsued);
			}
			else if (bitmap[i] == SIFS_DATABLOCK)
			{
				// We need to update fileblock if we are shifting the first datablock
				for (SIFS_BLOCKID id = 0; id < header.nblocks; id++)
				{
					if (bitmap[id] == SIFS_FILE)
					{
						SIFS_FILEBLOCK fblock = get_fileblock(header, vol, id);
						if (fblock.firstblockID == i) // If our datablock is the first block
						{
							fblock.firstblockID -= consecutiveUnsued;

							// Write to volume
							fseek(vol, sizeof(SIFS_VOLUME_HEADER) + header.nblocks + id * header.blocksize, SEEK_SET);
							fwrite(&fblock, sizeof(SIFS_FILEBLOCK), 1, vol);

							break;
						}
					}
				}

				shift_data(header, bitmap, vol, i, consecutiveUnsued);
			}
		}
	}

	free(bitmap);
	fclose(vol);
	return 0;
}
