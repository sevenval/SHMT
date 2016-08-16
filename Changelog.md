
1.0.1

 * Fixed: Memory leaks in the `SHMT::create()` method (`7ea1490`)
 * Fixed: The order of the `include`s (`cc9afee`)

 * Improved: Using of appropriate return macros makes superfluous `zval`s initialization unnecessary (`a71018e`)
 * Improved: Less pointer-pointer handlings/accesses (`bff5a9e`)
 * Improved: Using `void *` casts instead of `char *` for better code readability (`bff5a9e`)

 * Added: Travis tests (`037d13a`, `6966870`)

1.0.0

 * Improved: Trivial documentation and comment fixes (`59e63a3`)
 * Improved: More links and a note about the platform in the `README.md` (`fb1aa69`)
