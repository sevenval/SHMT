#include <php.h>
#include <Zend/zend_exceptions.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "shmt.h"

/* ==================================================================================================== */

shmtHead shmtCreateHeader(uint32_t size)
{
	shmtHead sHead;

	memset(&sHead, 0, sizeof(sHead));

	sHead.mark		= SHMT_MARK;
	sHead.version	= SHMT_FILE_VERSION;
	sHead.system	= SIZEOF_SIZE_T;
	sHead.mask		= _shmtGetMapSize(size) - 1;
	sHead.fileSize	= 0;
	sHead.hashSize	= (sHead.mask + 1) * sizeof(shmtHash);
	sHead.mapSize	= sHead.hashSize + ((sHead.mask + 1) * sizeof(shmtItem));

	return sHead;
}

int shmtCreateMapTable(shmtHash **pMap, shmtItem **pTbl, shmtHead sHead)
{
	if ((*pMap = (shmtHash *)emalloc(sHead.mapSize)) == NULL) {
		return 0;
	}

	/* Fill the pMap and pTbl with the empty-index flags */
	memset(((void *)(*pMap)), UINT32_MAX, sHead.mapSize);

	/* Assign the forwarded address */
	*pTbl = (shmtItem *)((void *)*pMap + sHead.hashSize);

	return 1;
}

int shmtWriteItem(shmtCreatorItem *src, shmtItem *shmtItem, FILE *file)
{
	if ((shmtItem->key_pos = ftell(file)) <= 0) {
		return 0;
	}

	shmtItem->key_len = src->key_len;
	shmtItem->val_len = src->val_len;

	if (src->key_len) {
		/* Check for site_t overflow */
		if ((shmtItem->key_pos + shmtItem->key_len) < shmtItem->key_pos) {
			return 0;
		} else if (fwrite(src->key, 1, src->key_len, file) != (size_t)src->key_len) {
			return 0;
		}
	}

	if (src->val_len) {
		/* Check for site_t overflow */
		if ((shmtItem->key_pos + shmtItem->key_len + shmtItem->val_len) <= shmtItem->key_pos) {
			return 0;
		} else if (fwrite(src->val, 1, src->val_len, file) != (size_t)src->val_len) {
			return 0;
		}
	}

	return 1;
}

int shmtReadTable(const char *path, shmtHead **shmt)
{
	struct stat	sizeCheck;
	int			file;

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	if ((file = open(path, O_RDONLY)) < 0) {
		return shmtCleanupReader(-1, "SHMT: Cannot open the file");
	} else if (fstat(file, &sizeCheck) != 0) {
		return shmtCleanupReader(file, "SHMT: Cannot read the file size");
	} else if ((size_t)sizeCheck.st_size > SIZE_MAX) {
		return shmtCleanupReader(file, "SHMT: Given file is out of the supported file size range");
	} else if ((size_t)sizeCheck.st_size < sizeof(shmtHead)) {
		return shmtCleanupReader(file, "SHMT: Cannot read the header");
	} else if ((*shmt = (shmtHead *)mmap(NULL, (size_t)sizeCheck.st_size, PROT_READ, MAP_SHARED, file, 0)) == MAP_FAILED) {
		return shmtCleanupReader(file, "SHMT: Cannot map the file to the memory");
	} else if ((size_t)sizeCheck.st_size != (*shmt)->fileSize) {
		return shmtCleanupReader(file, "SHMT: Corrupt SHMT file detected");
	} else if ((*shmt)->system != SIZEOF_SIZE_T) {
		return shmtCleanupReader(file, "SHMT: Incompatible file given");
	} else if (((*shmt)->mark != SHMT_MARK) || ((*shmt)->version != SHMT_FILE_VERSION)) {
		return shmtCleanupReader(file, "SHMT: Unsupported file given");
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	return shmtCleanupReader(file, NULL);
}

int shmtCleanupReader(int file, const char *message)
{
	if (file >= 0) {
		close(file);
	}

	if (message != NULL) {
		zend_throw_exception(zend_ce_exception, message, 0);
		return 0;
	}

	return 1;
}

void shmtCleanup(shmtHead *sHead, shmtHash *map, FILE *file, const char *path, shmtCreatorItem *it, shmtCreatorList *li, const char *message)
{
	shmtCreatorList	*pLItem;
	shmtCreatorItem	*pCItem;
	size_t			idx, iterator;

	if (map != NULL) {
		efree(map);
	}

	if (file != NULL) {
		fclose(file);
	}

	if (path != NULL) {
		remove(path);
	}

	if (it != NULL) {
		if ((li != NULL) && (sHead != NULL)) {
			for (iterator = 0; iterator <= sHead->mask; iterator++) {
				pLItem = ((shmtCreatorList *)li) + iterator;

				if (pLItem->num <= 0) {
					break; /* No more items available, nothing to free any more */
				}

				idx = pLItem->idx;

				if (pLItem->num > 1) {
					while (idx != UINT32_MAX) {
						pCItem = ((shmtCreatorItem *)it) + idx;

						efree(pCItem->key);
						efree(pCItem->val);

						idx = pCItem->next;
					}
				} else {
					pCItem = ((shmtCreatorItem *)it) + idx;

					efree(pCItem->key);
					efree(pCItem->val);
				}
			}
		}

		efree(it);
	}

	if (li != NULL) {
		efree(li);
	}

	if (message != NULL) {
		zend_throw_exception(zend_ce_exception, message, 0);
	}
}

void shmtFree(shmtHead *shmt)
{
	if (shmt && (shmt != MAP_FAILED)) {
		munmap(shmt, shmt->fileSize);
	}
}

inline uint32_t _shmtGetMapSize(uint32_t size)
{
	if (size < SHMT_MIN_T_SIZE) {
		size = SHMT_MIN_T_SIZE;
	} else if (size > SHMT_MAX_T_SIZE) {
		return 0;
	}

	size -= 1;
	size |= (size >>  1);
	size |= (size >>  2);
	size |= (size >>  4);
	size |= (size >>  8);
	size |= (size >> 16);

	return size + 1;
}

/* ==================================================================================================== */

/**
 * MurmurHash3 implementation
 * --------------------------
 * 
 * MurmurHash3 was written by Austin Appleby, and is placed in the public
 * domain. The author hereby disclaims copyright to this source code.
 */

#define ROT32(x, y) (x << y) | (x >> (32 - y))

uint32_t shmtGetHash(const void *key, size_t len, uint32_t seed)
{
	const uint8_t *data = (const uint8_t *)key;
	const int nblocks = len / 4;
	int i;

	uint32_t hash = seed, c1 = 0xcc9e2d51, c2 = 0x1b873593, k1;
	const uint32_t *blocks = (const uint32_t *)(data + nblocks * 4);

	for(i = -nblocks; i; i++) {
		k1 = blocks[i];

		k1 *= c1;
		k1 = ROT32(k1, 15);
		k1 *= c2;

		hash ^= k1;
		hash = ROT32(hash, 13); 
		hash = hash * 5 + 0xe6546b64;
	}

	const uint8_t *tail = (const uint8_t *)(data + nblocks * 4);
	k1 = 0;

	switch(len & 3)
	{
		case 3: k1 ^= tail[2] << 16;
		case 2: k1 ^= tail[1] << 8;
		case 1: k1 ^= tail[0];
				k1 *= c1; k1 = ROT32(k1, 15); k1 *= c2; hash ^= k1;
	};

	hash ^= len;

	hash ^= hash >> 16;
	hash *= 0x85ebca6b;
	hash ^= hash >> 13;
	hash *= 0xc2b2ae35;
	hash ^= hash >> 16;

	return hash;
}
