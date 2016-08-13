#ifndef PHP_SHMT_H
	#define PHP_SHMT_H

	#define PHP_SHMT_EXTNAME	"SHMT"
	#define PHP_SHMT_EXTVER		"1.0.1"

	#ifdef HAVE_CONFIG_H
		#include "config.h"
	#endif /* HAVE_CONFIG_H */

	extern zend_module_entry shmt_module_entry;
	#define phpext_shmt_ptr &shmt_module_entry;

#endif /* PHP_SHMT_H */
