# -*- autoconf -*---------------------------------------------------------------
# HPX_DO_AC_CONFIG_FILES
#
# Use autoconf to configure all of the files that need to be processed.
# -----------------------------------------------------------------------------
AC_DEFUN([HPX_DO_AC_CONFIG_FILES], [
 AC_CONFIG_FILES([
   hpx.pc
   Makefile
   contrib/Makefile
   libsync/libsync.export   
   libsync/Makefile
   libsync/arch/Makefile
   libhpx/libhpx.export
   libhpx/Makefile
   libhpx/boot/Makefile
   libhpx/gas/Makefile
   libhpx/gas/pgas/Makefile
   libhpx/gas/agas/Makefile
   libhpx/memory/Makefile
   libhpx/network/Makefile
   libhpx/network/isir/Makefile
   libhpx/network/pwc/Makefile
   libhpx/system/Makefile
   libhpx/util/Makefile
   libhpx/scheduler/Makefile
   libhpx/scheduler/arch/Makefile
   libhpx/instrumentation/Makefile
   include/Makefile
   include/libhpx/Makefile
   examples/Makefile])

 AS_IF([test "x$enable_tutorial" != xno], [
   AC_CONFIG_FILES([
     tutorial/Makefile
     tutorial/Beginners/Makefile
     tutorial/Beginners/threads/Makefile
     tutorial/Beginners/gas/Makefile
     tutorial/Beginners/collectives/Makefile
     tutorial/Beginners/lco/Makefile
     tutorial/Beginners/parcel/Makefile
     tutorial/HeatSeq/Makefile
     tutorial/CollectiveBench/Makefile
     tutorial/cpi/Makefile])])
  
 AS_IF([test "x$have_docs" != xno], [
   AC_CONFIG_FILES([
     docs/Doxyfile
     docs/Makefile])])
  
 AS_IF([test "x$enable_tests" != xno -o "x$enable_lengthy_tests" != xno], [
   AC_CONFIG_FILES([
     tests/Makefile
     tests/unit/Makefile
     tests/perf/Makefile])])
])