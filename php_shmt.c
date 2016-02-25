#include <stdio.h>
#include <stdint.h>

#include "php.h"
#include "zend_exceptions.h"
#include "ext/standard/info.h"

#include "php_shmt.h"
#include "shmt.h"

/* ==================================================================================================== */

zend_class_entry *ceSHMT;
zend_object_handlers SHMT_handlers;

inline SHMT_object *SHMT_fetch(zend_object *object)
{
	return (SHMT_object *)((char *)(object) - XtOffsetOf(SHMT_object, std));
}

zend_object *SHMT_new(zend_class_entry *ce)
{
	SHMT_object *intern = (SHMT_object *)ecalloc(1, sizeof(SHMT_object) + zend_object_properties_size(ce));

	intern->shmt	= NULL;
	intern->map		= NULL;
	intern->tbl		= NULL;

	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &SHMT_handlers;

	return &intern->std;
}

void SHMT_free(zend_object *object)
{
	SHMT_object *intern = SHMT_fetch(object);

	zend_object_std_dtor(&intern->std);

	shmtFree(&intern->shmt);
}

/* ==================================================================================================== */

ZEND_BEGIN_ARG_INFO_EX(arginfo_SHMT__construct, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, filename, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_SHMT_get, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, string, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_SHMT_create, 0, 0, 2)
	ZEND_ARG_TYPE_INFO(0, filename, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, array, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

/* ==================================================================================================== */

PHP_METHOD(SHMT, __construct)
{
	SHMT_object	*object = SHMT_fetch(Z_OBJ_P(getThis()));
	char		*path;
	size_t		pathLen;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "s", &path, &pathLen)) {
		return;
	} else if (!shmtReadTable(path, &object->shmt)) {
		return;
	}

	/* Assign the pointer to the right addresses/offsets */
	object->map = (struct _shmtHash *)((char *)object->shmt + sizeof(struct _shmtHead));
	object->tbl = (struct _shmtItem *)((char *)object->map + object->shmt->hashSize);
}

PHP_METHOD(SHMT, get)
{
	SHMT_object			*object;
	char				*key;
	size_t				keyLen;
	uint32_t			hash;
	zval				result;
	struct _shmtItem	*shmtItem;
	struct _shmtHash	*shmtHash;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "s", &key, &keyLen)) {
		return;
	}

	object = SHMT_fetch(Z_OBJ_P(getThis()));

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	hash = shmtGetHash(key, keyLen, 0);
	shmtHash = &object->map[hash & object->shmt->mask];

	if (shmtHash->seed == UINT32_MAX) {
		/* Direct hit */
		shmtItem = &shmtHash->hit;
	} else {
		/* Resolved item */
		hash = shmtGetHash(key, keyLen, shmtHash->seed);
		shmtItem = &object->tbl[hash & object->shmt->mask];
	}

	if ((shmtItem->key_pos != SIZE_MAX) && (keyLen == shmtItem->key_len)) {
		if ((memcmp((const void *)((char *)object->shmt + shmtItem->key_pos), key, keyLen) == 0)) {
			ZVAL_STRINGL(&result, (char *)object->shmt + shmtItem->key_pos + shmtItem->key_len, shmtItem->val_len);
			RETURN_ZVAL(&result, 0, 1);
		}
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	/* No match found -> return a NULL value */
	ZVAL_NULL(&result);
	RETURN_ZVAL(&result, 0, 1);
}

PHP_METHOD(SHMT, create)
{
	char				*path;
	size_t				pathLen;
	zval				*data, currKey, *currVal;
	HashTable			*htData;
	HashPosition		hpPos;
	uint32_t			iCount, iItemIndex, iterator = 0;
	FILE				*pFile;
	struct _shmtHead	shmtHead;
	struct _shmtHash	*pMap;
	struct _shmtItem	*pTbl, *shmtItem;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "sa", &path, &pathLen, &data)) {
		return;
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	htData = Z_ARRVAL_P(data);
	zend_hash_internal_pointer_reset_ex(htData, &hpPos);

	iCount = zend_hash_num_elements(htData);

	if (iCount < 1) {
		return shmtCleanup(NULL, NULL, NULL, NULL, NULL, "SHMT: Empty data array given");
	} else if ((pFile = fopen(path, "w")) == NULL) {
		return shmtCleanup(NULL, NULL, NULL, NULL, NULL, "SHMT: Cannot open the file");
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	shmtHead = shmtCreateHeader(iCount);

	if (shmtHead.mask == UINT32_MAX) {
		return shmtCleanup(NULL, &pFile, path, NULL, NULL, "SHMT: Number of the given elements is out of range");
	}

	if ((fwrite(&shmtHead, 1, sizeof(shmtHead), pFile)) < sizeof(shmtHead)) {
		return shmtCleanup(NULL, &pFile, path, NULL, NULL, "SHMT: Cannot write the header");
	}

	if (!shmtCreateMapTable(&pMap, &pTbl, shmtHead)) {
		return shmtCleanup(NULL, &pFile, path, NULL, NULL, "SHMT: Cannot create the table");
	}

	if (fseek(pFile, shmtHead.mapSize, SEEK_CUR)) {
		return shmtCleanup(&pMap, &pFile, path, NULL, NULL, "SHMT: Unexpected internal \"seek\" error");
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	uint32_t n, newHashVal, newHashMod, newSeed;
	struct _shmtCreatorItem	*pCItem = NULL, *pCItems = NULL;
	struct _shmtCreatorList	*pLItem = NULL, *pLItems = NULL;

	if ((pCItems = (struct _shmtCreatorItem *)emalloc((shmtHead.mask + 1) * sizeof(struct _shmtCreatorItem))) == NULL) {
		return shmtCleanup(&pMap, &pFile, path, NULL, NULL, "SHMT: Unexpected internal \"malloc\" error");
	}

	if ((pLItems = (struct _shmtCreatorList *)emalloc((shmtHead.mask + 1) * sizeof(struct _shmtCreatorList))) == NULL) {
		return shmtCleanup(&pMap, &pFile, path, &pCItems, NULL, "SHMT: Unexpected internal \"malloc\" error");
	}

	memset(pLItems, 0, (shmtHead.mask + 1) * sizeof(struct _shmtCreatorList));

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	while ((currVal = zend_hash_get_current_data_ex(htData, &hpPos)) != NULL) {
		zend_hash_get_current_key_zval_ex(htData, &currKey, &hpPos);

		convert_to_string(currVal);
		convert_to_string(&currKey);

		iItemIndex		= iterator++;
		pCItem			= ((struct _shmtCreatorItem *)(pCItems)) + iItemIndex;

		pCItem->hash	= shmtGetHash(Z_STRVAL(currKey), Z_STRLEN(currKey), 0);
		pCItem->hash	= (uint32_t)(pCItem->hash & shmtHead.mask);

		zend_string *x	= zval_get_string(&currKey);
		pCItem->key		= x->val;
		pCItem->key_len	= Z_STRLEN(currKey);
		zend_string *y	= zval_get_string(currVal);
		pCItem->val		= y->val;
		pCItem->val_len	= Z_STRLEN_P(currVal);

		pLItem			= ((struct _shmtCreatorList *)(pLItems)) + pCItem->hash;

		if (!pLItem->num) {
			pLItem->idx = UINT32_MAX;
		}

		pCItem->next	= pLItem->idx;
		pLItem->idx		= iItemIndex;
		pLItem->num++;

		zval_dtor(&currKey);
		zend_hash_move_forward_ex(htData, &hpPos);
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	qsort((void *)pLItems, shmtHead.mask + 1, sizeof(struct _shmtCreatorList), shmtSortCmp);

	for (iterator = 0; iterator <= shmtHead.mask; iterator++) {
		pLItem = ((struct _shmtCreatorList *)(pLItems)) + iterator;

		/* Ready, all items are written. */
		if (pLItem->num <= 0) {
			break;
		}

		iItemIndex = pLItem->idx;

		/* Resolve collisions */
		if (pLItem->num > 1) {
			iCount = 0;
			newSeed = 1;

			uint32_t aTempMod[pLItem->num];
			memset(&aTempMod, UINT32_MAX, sizeof(aTempMod));

			while (iItemIndex != UINT32_MAX) {
				pCItem = ((struct _shmtCreatorItem *)(pCItems)) + iItemIndex;

				newHashVal = shmtGetHash(pCItem->key, pCItem->key_len, newSeed);
				newHashMod = newHashVal & shmtHead.mask;

				shmtItem = ((struct _shmtItem *)(pTbl)) + newHashMod;

				if (shmtItem->key_pos != SIZE_MAX) {
					newSeed++;
					memset(&aTempMod, UINT32_MAX, sizeof(aTempMod));
					iItemIndex = pLItem->idx;
					iCount = 0;
				} else {
					for (n = 0; n <= iCount; n++) {
						if (aTempMod[n] == UINT32_MAX) {
							aTempMod[iCount] = newHashMod;
							iItemIndex = pCItem->next;
							iCount++;
							break;
						}

						if (aTempMod[n] == newHashMod) {
							newSeed++;
							memset(&aTempMod, UINT32_MAX, sizeof(aTempMod));
							iItemIndex = pLItem->idx;
							iCount = 0;
							break;
						}
					}
				}
			}

			pMap[pCItem->hash].seed = newSeed;

			iItemIndex = pLItem->idx;
			for (n = 0; n < pLItem->num; n++) {
				pCItem = ((struct _shmtCreatorItem *)(pCItems)) + iItemIndex;
				shmtItem = ((struct _shmtItem *)(pTbl)) + aTempMod[n];

				if (!shmtWriteItem(pCItem, shmtItem, pFile)) {
					return shmtCleanup(&pMap, &pFile, path, &pCItems, &pLItems, "SHMT: Unexpected internal \"write\" error");
				}

				iItemIndex = pCItem->next;
			}
		} else {
			/* Collision free items are written directly into the pMap (not pTbl) array. */
			pCItem = ((struct _shmtCreatorItem *)(pCItems)) + iItemIndex;

			if (!shmtWriteItem(pCItem, &pMap[pCItem->hash].hit, pFile)) {
				return shmtCleanup(&pMap, &pFile, path, &pCItems, &pLItems, "SHMT: Unexpected internal \"write\" error");
			}
		}
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	if (fseek(pFile, sizeof(shmtHead), SEEK_SET) || ((fwrite(pMap, 1, shmtHead.mapSize, pFile)) < shmtHead.mapSize)) {
		return shmtCleanup(&pMap, &pFile, path, &pCItems, &pLItems, "SHMT: Cannot write the table");
	}

	if (
		(fseek(pFile, 0, SEEK_END) != 0) ||
		((shmtHead.fileSize = ftell(pFile)) <= 0) ||
		(fseek(pFile, (char *)&shmtHead.fileSize - (char *)&shmtHead, SEEK_SET) != 0) ||
		((fwrite(&shmtHead.fileSize, 1, sizeof(shmtHead.fileSize), pFile)) < sizeof(shmtHead.fileSize))
	) {
		return shmtCleanup(&pMap, &pFile, path, &pCItems, &pLItems, "SHMT: Unexpected internal \"finalize\" error");
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	shmtCleanup(&pMap, &pFile, NULL, &pCItems, &pLItems, NULL);

	RETURN_BOOL(1);
}

/* ==================================================================================================== */

zend_function_entry SHMT_methods[] = {
	PHP_ME(SHMT, __construct, arginfo_SHMT__construct, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
	PHP_ME(SHMT, get, arginfo_SHMT_get, ZEND_ACC_PUBLIC)
	PHP_ME(SHMT, create, arginfo_SHMT_create, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_FE_END
};

/* ==================================================================================================== */

static PHP_MINIT_FUNCTION(shmt)
{
	zend_class_entry ce;

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

	memcpy(&SHMT_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	SHMT_handlers.offset = XtOffsetOf(SHMT_object, std);
	SHMT_handlers.dtor_obj = zend_objects_destroy_object;
	SHMT_handlers.free_obj = SHMT_free;

	INIT_CLASS_ENTRY(ce, "SHMT", SHMT_methods);
	ce.create_object = SHMT_new;
	ceSHMT = zend_register_internal_class_ex(&ce, NULL);

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

	return SUCCESS;
}

static PHP_MINFO_FUNCTION(shmt)
{
	php_info_print_table_start();
		php_info_print_table_header(2, "SHMT", "enabled");
		php_info_print_table_row(2, "Version", PHP_SHMT_EXTVER);
	php_info_print_table_end();
}

/* ==================================================================================================== */

zend_module_entry shmt_module_entry =
{
	STANDARD_MODULE_HEADER,
	PHP_SHMT_EXTNAME,
	NULL,					/* Functions */
	PHP_MINIT(shmt),
	NULL,					/* MSHUTDOWN */
	NULL,					/* RINIT */
	NULL,					/* RSHUTDOWN */
	PHP_MINFO(shmt),
	PHP_SHMT_EXTVER,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SHMT
	ZEND_GET_MODULE(shmt)
#endif
