#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
AC_INIT(rivus, 1.0)
AC_CONFIG_SRCDIR([lib])
AM_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE(rivus, 1.0)

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AM_PROG_CC_C_O

AC_ARG_ENABLE(debug,AS_HELP_STRING([--enable-debug],[enable DEBUG mode(default=no)]),
        [],
        [enable_debug=no])
AS_IF([test "x$enable_debug" = "xyes"],[CFLAGS="-g2 -O0 -DDEBUG"],
      [test "x$enable_debug" = "xno"],[CFLAGS="-O2"],
      [])

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h netinet/in.h stdlib.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset socket bzero])

AC_OUTPUT([Makefile
                 lib/Makefile
                 example/Makefile])
