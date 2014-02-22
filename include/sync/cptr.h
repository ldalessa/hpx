// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef HPX_SYNC_CPTR_H
#define HPX_SYNC_CPTR_H

#include <stdint.h>
#include "sync.h"

/// ----------------------------------------------------------------------------
/// A "counted pointer" structure.
///
/// This common pattern is used in non-blocking algorithms where CAS is the
/// primitive atomic operation. It represents a normal pointer, where each CAS
/// updates the "version" of the pointer, and avoids the ABA problem for up to
/// 64-bits worth of CAS operations.
///
/// It relies on having access to a CAS that is twice as big as a pointer.
///
/// @field p - the actual pointer that we're protecting
/// @field c - the count of the number of times this pointer has been CASed
/// ----------------------------------------------------------------------------
typedef struct {
  void *p;
  uint64_t c;
} cptr_t __attribute__((aligned(16)));

/// ----------------------------------------------------------------------------
/// CAS a counter pointer.
///
/// This performs a compare-and-swap of the counted pointer, attempting to
/// atomically update @p ptr->p from @from->p to @p to, while updating the count
/// from @p from->c to @p from->p + 1. If either the pointer or count is not
/// correct, it will fail and return the actual "seen" value in *from. This only
/// works because it is "static inline".
///
/// @param          ptr - the memory location we are going to cas
/// @param[in/out] from - the pointer we're expecting to see at *ptr
/// @param           to - the address that we're trying to update *ptr to
/// ----------------------------------------------------------------------------
static inline void
sync_cptr_cas_val(volatile cptr_t *ptr, cptr_t *from, void *to) {
  __asm__ __volatile__("lock\t  cmpxchg16b %[addr]\n"
                       : [addr] "+m" (*ptr),
                                "+d" (from->c),
                                "+a" (from->p)
                       :        "c"  (from->c + 1),
                                "b"  (to)
                       : "cc");
}

/// ----------------------------------------------------------------------------
/// CAS a counted pointer.
///
/// This performs a compare-and-swap of the counted pointer, returning true if
/// it succeeds and false if it fails.
/// ----------------------------------------------------------------------------
static inline bool
sync_cptr_cas(volatile cptr_t *ptr, cptr_t from, void *to) {
  bool result;
  __asm__ __volatile__("lock\t  cmpxchg16b %[addr]\n"
                       "setz\t  %[result]\n"
                       : [result] "=q" (result),
                           [addr] "+m" (*ptr),
                                  "+d" (from.c),
                                  "+a" (from.p)
                       :          "c"  (from.c + 1),
                                  "b"  (to)
                       : "cc");
  return result;
}

/// ----------------------------------------------------------------------------
/// Atomically load a counted pointer.
///
/// For x86_64, which we've implemented here, the only valid way to read a 16
/// byte memory address atomically is with the cmpxch16b instruction.
/// ----------------------------------------------------------------------------
static inline void
sync_cptr_load(volatile cptr_t *ptr, cptr_t *out) {
  __asm__ __volatile__("lock cmpxchg16b %[addr]\n"
                       : [addr] "+m" (*ptr),
                         "=d" (out->c),
                         "=a" (out->p)
                       :
                       : "cc");
}

/// ----------------------------------------------------------------------------
/// Checks to see if a counted pointer has changed.
///
/// Because we need to use an atomic cmpchg16b to read a location anyway, we can
/// find out if it has changed by supplying an expected value.
/// ----------------------------------------------------------------------------
static inline bool
sync_cptr_is_consistent(volatile cptr_t *ptr, cptr_t *val) {
  bool result;
  __asm__ __volatile__("lock\t  cmpxchg16b %[addr]\n"
                       "setz\t  %[result]\n"
                       : [result] "=q" (result),
                           [addr] "+m" (*ptr),
                                  "+d" (val->c),
                                  "+a" (val->p)
                       :          "c"  (val->c),
                                  "b"  (val->p)
                       : "cc");
  return result;
}

#endif // HPX_SYNC_CPTR_H
