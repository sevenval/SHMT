#ifndef SHMT_H
	#define SHMT_H

	#include <arpa/inet.h>

	/* ==================================================================================================== */

	#define SHMT_MARK			htonl('s' << 24 | 'h' << 16 | 'm' <<  8 | 't')
	#define SHMT_FILE_VERSION	((uint16_t)(100)) /* Example: 1023 corresponds to version 10.23 */
	#define SHMT_MIN_T_SIZE		((uint32_t)(2))

	#if SIZEOF_SIZE_T == 4
		#define SHMT_MAX_T_SIZE	((uint32_t)(1 << 26))
	#elif SIZEOF_SIZE_T == 8
		#define SHMT_MAX_T_SIZE	((uint32_t)(1 << 31))
	#else
		#error "Unknown SIZEOF_SIZE_T"
	#endif

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	/* STRUCTURES */

	typedef struct _shmtCreatorItem	shmtCreatorItem;
	typedef struct _shmtCreatorList	shmtCreatorList;
	typedef struct _shmtItem		shmtItem;
	typedef struct _shmtHash		shmtHash;
	typedef struct _shmtHead		shmtHead;

	struct _shmtCreatorItem
	{
		char		*key;
		uint32_t	key_len;
		char		*val;
		uint32_t	val_len;
		uint32_t	hash;
		uint32_t	next;
	};

	struct _shmtCreatorList
	{
		uint32_t	num;		/* The number of items in the current bucket				*/
		uint32_t	idx;		/* The index of the last item in the current bucket			*/
	};

	struct _shmtItem
	{
		size_t		key_pos;	/* The offset to the key string								*/
		uint32_t	key_len;	/* The key string length									*/
		uint32_t	val_len;	/* The value string length									*/
	};

	struct _shmtHash
	{
		uint32_t	seed;		/* The alternative seed										*/
		shmtItem	hit;		/* The direct hit											*/
	};

	struct _shmtHead
	{
		uint32_t	mark;		/* The plausibility check mark								*/
		uint16_t	version;	/* The file compatibility check mark						*/
		uint8_t		system;		/* The system compatibility check mark (32/64 bit)			*/
		uint32_t	mask;		/* The hash map mask										*/
		size_t		mapSize;	/* The total hash map table size							*/
		size_t		hashSize;	/* The size of the "hash" part in the hash map				*/
		size_t		fileSize;	/* The file size in bytes (for "mmap()")					*/
	};

	typedef struct _shmt_object
	{
		shmtHead	*shmt;		/* The "mmap()" pointer to be able to call "munmap()"		*/
		shmtHash	*map;		/* The pointer to the "map" part in the total map table		*/
		shmtItem	*tbl;		/* The pointer to the "tbl" part in the total map table		*/
		zend_object	std;		/* The PHP class object										*/
	} shmt_object;

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	/* FUNCTIONS */

	shmtHead shmtCreateHeader(uint32_t size);

	int shmtCreateMapTable(shmtHash **pMap, shmtItem **pTbl, shmtHead shmtHead);

	int shmtWriteItem(shmtCreatorItem *src, shmtItem *dest, FILE *file);

	int shmtReadTable(const char *path, shmtHead **shmt);

	int shmtCleanupReader(int file, const char *message);

	void shmtCleanup(shmtHead *sHead, shmtHash *map, FILE *file, const char *path, shmtCreatorItem *it, shmtCreatorList *li, const char *message);

	void shmtFree(shmtHead *shmt);

	uint32_t _shmtGetMapSize(uint32_t size);

	uint32_t shmtGetHash(const void *key, size_t len, uint32_t seed);

	static inline int shmtSortCmp(const void *a, const void *b)
	{
		if ((*(shmtCreatorList *)a).num > (*(shmtCreatorList *)b).num) {
			return -1;
		} else if ((*(shmtCreatorList *)a).num < (*(shmtCreatorList *)b).num) {
			return  1;
		} else {
			return  0;
		}
	}

#endif /* SHMT_H */
