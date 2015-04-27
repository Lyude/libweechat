AC_INIT([libweechat], [0.0], [thatslyude@gmail.com], [libweechat], [https://lyude.net])
AM_INIT_AUTOMAKE([foreign -Wall])
AC_CONFIG_MACRO_DIR([m4])

# Check for programs
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_AWK
AC_PROG_MKDIR_P
AC_PROG_SED
AM_PROG_CC_C_O
AM_PROG_AR
PKG_PROG_PKG_CONFIG

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T

AX_GCC_FUNC_ATTRIBUTE(constructor)

AM_SILENT_RULES([yes])

PKG_CHECK_MODULES([GLIB], [glib-2.0])
PKG_CHECK_MODULES([GIO], [gio-2.0])

LT_INIT

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile tools/Makefile])
AC_OUTPUT
