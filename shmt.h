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
		uint32_t num;				/* The number of items in the current bucket				*/
		uint32_t idx;				/* The index of the last item in the current bucket			*/
	};

	struct _shmtItem
	{
		size_t		key_pos;		/* The offset to the key string								*/
		uint32_t	key_len;		/* The key string length									*/
		uint32_t	val_len;		/* The value string length									*/
	};

	struct _shmtHash
	{
		uint32_t			seed;	/* The alternative seed										*/
		struct _shmtItem	hit;	/* The direct hit											*/
	};

	struct _shmtHead
	{
		uint32_t	mark;			/* The plausibility check mark								*/
		uint16_t	version;		/* The file compatibility check mark						*/
		uint8_t		system;			/* The system compatibility check mark (32/64 bit)			*/
		uint32_t	mask;			/* The hash map mask										*/
		size_t		mapSize;		/* The total hash map table size							*/
		size_t		hashSize;		/* The size of the "hash" part in the hash map				*/
		size_t		fileSize;		/* The file size in bytes (for "mmap()")					*/
	};

	typedef struct _SHMT_object
	{
		struct _shmtHead	*shmt;	/* The "mmap()" pointer to be able to call "munmap()"		*/
		struct _shmtHash	*map;	/* The pointer to the "map" part in the total map table		*/
		struct _shmtItem	*tbl;	/* The pointer to the "tbl" part in the total map table		*/
		zend_object			std;	/* The PHP class object										*/
	} SHMT_object;

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	/* FUNCTIONS */

	struct _shmtHead shmtCreateHeader(uint32_t size);

	int shmtCreateMapTable(struct _shmtHash **pMap, struct _shmtItem **pTbl, struct _shmtHead shmtHead);

	int shmtWriteItem(struct _shmtCreatorItem *src, struct _shmtItem *dest, FILE *file);

	int shmtReadTable(const char *path, struct _shmtHead **shmt);

	int shmtCleanupReader(int file, const char *message);

	void shmtCleanup(struct _shmtHead *head, struct _shmtHash *map, FILE *file, const char *path, struct _shmtCreatorItem *it, struct _shmtCreatorList *li, const char *message);

	void shmtFree(struct _shmtHead *shmt);

	uint32_t _shmtGetMapSize(uint32_t size);

	uint32_t shmtGetHash(const void *key, size_t len, uint32_t seed);

	static inline int shmtSortCmp(const void *a, const void *b)
	{
		if ((*(struct _shmtCreatorList *)a).num > (*(struct _shmtCreatorList *)b).num) {
			return -1;
		} else if ((*(struct _shmtCreatorList *)a).num < (*(struct _shmtCreatorList *)b).num) {
			return  1;
		} else {
			return  0;
		}
	}

#endif /* SHMT_H */
