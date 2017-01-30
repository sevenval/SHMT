#include <php.h>
#include <Zend/zend_exceptions.h>
#include <ext/standard/info.h>

#include "php_shmt.h"
#include "shmt.h"

/* ==================================================================================================== */

zend_class_entry *ce_shmt;
zend_object_handlers shmt_handlers;

inline shmt_object *shmt_fetch(zend_object *object)
{
	return (shmt_object *)((void *)(object) - XtOffsetOf(shmt_object, std));
}

zend_object *shmt_new(zend_class_entry *ce)
{
	shmt_object *intern = (shmt_object *)ecalloc(1, sizeof(shmt_object) + zend_object_properties_size(ce));

	intern->shmt	= NULL;
	intern->map		= NULL;
	intern->tbl		= NULL;

	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &shmt_handlers;

	return &intern->std;
}

void shmt_free(zend_object *object)
{
	shmt_object *intern = shmt_fetch(object);

	zend_object_std_dtor(&intern->std);

	shmtFree(intern->shmt);
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
	shmt_object	*object = shmt_fetch(Z_OBJ_P(getThis()));
	char		*path;
	size_t		pathLen;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "s", &path, &pathLen)) {
		return;
	} else if (!shmtReadTable(path, &object->shmt)) {
		return;
	}

	/* Assign the pointer to the right addresses/offsets */
	object->map = (shmtHash *)((void *)object->shmt + sizeof(shmtHead));
	object->tbl = (shmtItem *)((void *)object->map + object->shmt->hashSize);
}

PHP_METHOD(SHMT, get)
{
	shmt_object	*object;
	char		*key;
	size_t		keyLen;
	uint32_t	hash;
	shmtItem	*pItem;
	shmtHash	*pHash;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "s", &key, &keyLen)) {
		return;
	}

	object = shmt_fetch(Z_OBJ_P(getThis()));

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	hash = shmtGetHash(key, keyLen, 0);
	pHash = &object->map[hash & object->shmt->mask];

	if (pHash->seed == UINT32_MAX) {
		/* Direct hit */
		pItem = &pHash->hit;
	} else {
		/* Resolved item */
		hash = shmtGetHash(key, keyLen, pHash->seed);
		pItem = &object->tbl[hash & object->shmt->mask];
	}

	if ((pItem->key_pos != SIZE_MAX) && (keyLen == pItem->key_len)) {
		if ((memcmp((const void *)((void *)object->shmt + pItem->key_pos), key, keyLen) == 0)) {
			RETURN_STRINGL((void *)object->shmt + pItem->key_pos + pItem->key_len, pItem->val_len);
		}
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	/* No match found -> return a NULL value */
	RETURN_NULL();
}

PHP_METHOD(SHMT, create)
{
	char			*path;
	size_t			pathLen;
	zval			*data, currKey, *currVal;
	HashTable		*htData;
	HashPosition	hpPos;
	uint32_t		iCount, iItemIndex, iterator = 0;
	FILE			*pFile;
	shmtHead		sHead;
	shmtHash		*pMap;
	shmtItem		*pTbl, *pItem;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "sa", &path, &pathLen, &data)) {
		return;
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	htData = Z_ARRVAL_P(data);
	zend_hash_internal_pointer_reset_ex(htData, &hpPos);

	iCount = zend_hash_num_elements(htData);

	if (iCount < 1) {
		return shmtCleanup(NULL, NULL, NULL, NULL, NULL, NULL, "SHMT: Empty data array given");
	} else if ((pFile = fopen(path, "w")) == NULL) {
		return shmtCleanup(NULL, NULL, NULL, NULL, NULL, NULL, "SHMT: Cannot open the file");
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	sHead = shmtCreateHeader(iCount);

	if (sHead.mask == UINT32_MAX) {
		return shmtCleanup(&sHead, NULL, pFile, path, NULL, NULL, "SHMT: Number of the given elements is out of range");
	}

	if ((fwrite(&sHead, 1, sizeof(sHead), pFile)) < sizeof(sHead)) {
		return shmtCleanup(&sHead, NULL, pFile, path, NULL, NULL, "SHMT: Cannot write the header");
	}

	if (!shmtCreateMapTable(&pMap, &pTbl, sHead)) {
		return shmtCleanup(&sHead, NULL, pFile, path, NULL, NULL, "SHMT: Cannot create the table");
	}

	if (fseek(pFile, sHead.mapSize, SEEK_CUR)) {
		return shmtCleanup(&sHead, pMap, pFile, path, NULL, NULL, "SHMT: Unexpected internal \"seek\" error");
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	uint32_t n, newHashVal, newHashMod, newSeed;
	shmtCreatorItem	*pCItem = NULL, *pCItems = NULL;
	shmtCreatorList	*pLItem = NULL, *pLItems = NULL;

	if ((pCItems = (shmtCreatorItem *)emalloc((sHead.mask + 1) * sizeof(shmtCreatorItem))) == NULL) {
		return shmtCleanup(&sHead, pMap, pFile, path, NULL, NULL, "SHMT: Unexpected internal \"malloc\" error");
	}

	if ((pLItems = (shmtCreatorList *)emalloc((sHead.mask + 1) * sizeof(shmtCreatorList))) == NULL) {
		return shmtCleanup(&sHead, pMap, pFile, path, pCItems, NULL, "SHMT: Unexpected internal \"malloc\" error");
	}

	memset(pCItems, 0, (sHead.mask + 1) * sizeof(shmtCreatorItem));
	memset(pLItems, 0, (sHead.mask + 1) * sizeof(shmtCreatorList));

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	while ((currVal = zend_hash_get_current_data_ex(htData, &hpPos)) != NULL) {
		zend_hash_get_current_key_zval_ex(htData, &currKey, &hpPos);

		convert_to_string(currVal);
		convert_to_string(&currKey);

		iItemIndex		= iterator++;
		pCItem			= ((shmtCreatorItem *)(pCItems)) + iItemIndex;

		pCItem->hash	= shmtGetHash(Z_STRVAL(currKey), Z_STRLEN(currKey), 0);
		pCItem->hash	= (uint32_t)(pCItem->hash & sHead.mask);

		pCItem->key		= estrndup(Z_STRVAL(currKey), Z_STRLEN(currKey));
		pCItem->key_len	= Z_STRLEN(currKey);
		pCItem->val		= estrndup(Z_STRVAL_P(currVal), Z_STRLEN_P(currVal));
		pCItem->val_len	= Z_STRLEN_P(currVal);

		pLItem			= ((shmtCreatorList *)(pLItems)) + pCItem->hash;

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

	qsort((void *)pLItems, sHead.mask + 1, sizeof(shmtCreatorList), shmtSortCmp);

	for (iterator = 0; iterator <= sHead.mask; iterator++) {
		pLItem = ((shmtCreatorList *)(pLItems)) + iterator;

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
				pCItem = ((shmtCreatorItem *)(pCItems)) + iItemIndex;

				newHashVal = shmtGetHash(pCItem->key, pCItem->key_len, newSeed);
				newHashMod = newHashVal & sHead.mask;

				pItem = ((shmtItem *)(pTbl)) + newHashMod;

				if (pItem->key_pos != SIZE_MAX) {
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
				pCItem = ((shmtCreatorItem *)(pCItems)) + iItemIndex;
				pItem = ((shmtItem *)(pTbl)) + aTempMod[n];

				if (!shmtWriteItem(pCItem, pItem, pFile)) {
					return shmtCleanup(&sHead, pMap, pFile, path, pCItems, pLItems, "SHMT: Unexpected internal \"write\" error");
				}

				iItemIndex = pCItem->next;
			}
		} else {
			/* Collision free items are written directly into the pMap (not pTbl) array. */
			pCItem = ((shmtCreatorItem *)(pCItems)) + iItemIndex;

			if (!shmtWriteItem(pCItem, &pMap[pCItem->hash].hit, pFile)) {
				return shmtCleanup(&sHead, pMap, pFile, path, pCItems, pLItems, "SHMT: Unexpected internal \"write\" error");
			}
		}
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	if (fseek(pFile, sizeof(sHead), SEEK_SET) || ((fwrite(pMap, 1, sHead.mapSize, pFile)) < sHead.mapSize)) {
		return shmtCleanup(&sHead, pMap, pFile, path, pCItems, pLItems, "SHMT: Cannot write the table");
	}

	if (
		(fseek(pFile, 0, SEEK_END) != 0) ||
		((sHead.fileSize = ftell(pFile)) <= 0) ||
		(fseek(pFile, (void *)&sHead.fileSize - (void *)&sHead, SEEK_SET) != 0) ||
		((fwrite(&sHead.fileSize, 1, sizeof(sHead.fileSize), pFile)) < sizeof(sHead.fileSize))
	) {
		return shmtCleanup(&sHead, pMap, pFile, path, pCItems, pLItems, "SHMT: Unexpected internal \"finalize\" error");
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	shmtCleanup(&sHead, pMap, pFile, NULL, pCItems, pLItems, NULL);

	RETURN_TRUE;
}

/* ==================================================================================================== */

zend_function_entry shmt_methods[] = {
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

	memcpy(&shmt_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	shmt_handlers.offset = XtOffsetOf(shmt_object, std);
	shmt_handlers.dtor_obj = zend_objects_destroy_object;
	shmt_handlers.free_obj = shmt_free;

	INIT_CLASS_ENTRY(ce, "SHMT", shmt_methods);
	ce.create_object = shmt_new;
	ce_shmt = zend_register_internal_class_ex(&ce, NULL);

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
