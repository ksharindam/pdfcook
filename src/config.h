/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.in by autoheader.  */

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.6"

/* Do we have ghostscript? */
#define HAVE_GS 1

/* Do we want lzw? */
#define HAVE_LZW 1

/* Do we have zlib? */
#define HAVE_ZLIB 1

/* Define to the equivalent of the C99 'restrict' keyword, or to
   nothing if this is not supported.  Do not define if restrict is
   supported directly.  */
#define restrict __restrict
/* Work around a bug in Sun C++: it does not support _Restrict, even
   though the corresponding Sun C compiler does, which causes
   "#define restrict _Restrict" in the previous line.  Perhaps some future
   version of Sun C++ will work with _Restrict; if so, it'll probably
   define __RESTRICT, just as Sun C does.  */
#if defined __SUNPRO_CC && !defined __RESTRICT
# define _Restrict
#endif
