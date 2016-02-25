
PHP_ARG_WITH(shmt, whether to enable the shmt extension,
	[--with-shmt			       Enable shmt support])

if test "$PHP_SHMT" != "no"; then
	PHP_SUBST(SHMT_SHARED_LIBADD)
	PHP_NEW_EXTENSION(shmt, shmt.c php_shmt.c, $ext_shared, , "-Wall")
fi
