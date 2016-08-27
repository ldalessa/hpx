// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#ifndef LIBHPX_THREAD_H
#define LIBHPX_THREAD_H

/// @file thread.h
/// @brief Defines the lightweight thread stack structure and interface for user
///        level threads.
#include <hpx/hpx.h>
#include <functional>

/// Forward declarations
/// @{
struct lco_class;
/// @}

typedef struct ustack {
  void *sp;                                    //!< checkpointed stack pointer
  hpx_parcel_t *parcel;                        //!< the progenitor parcel
  struct ustack  *next;                        //!< freelists and condition vars
  int        lco_depth;                        //!< how many lco locks we hold
  int           tls_id;                        //!< backs tls
  int         stack_id;                        //!< used by VALGRIND
  int             size;                        //!< the size of this stack
  short           cont;                        //!< the continuation flag
  short         masked;                        //!< should we checkpoint sigmask
  char         stack[];
} ustack_t;

/// This is the type of an HPX thread entry function.
typedef void (*thread_entry_t)(hpx_parcel_t *);

/// Sets the size of a stack.
///
/// All of the stacks in the system need to have the same size.
void thread_set_stack_size(int stack_bytes);

/// Initializes a thread.
///
/// The thread can be transferred to using thread_transfer() in order to start
/// execution. The @p entry function will be run as a result of the initial
/// transfer.
///
/// @param        stack The thread stack pointer.
/// @param       parcel The parcel that is generating this thread.
/// @param            f The entry function for the thread.
/// @param         size The thread size.
void thread_init(ustack_t *stack, hpx_parcel_t *parcel, thread_entry_t f,
                 size_t size)
  HPX_NON_NULL(1, 2);

/// Allocates and initializes a new thread.
///
/// This allocates and initializes a new user-level thread. User-level threads
/// are allocated in the global address space.
///
/// @param       parcel The parcel that is generating this thread.
/// @param            f The entry function for the thread.
///
/// @returns A new thread that can be transferred to.
ustack_t *thread_new(hpx_parcel_t *parcel, thread_entry_t f)
  HPX_NON_NULL(1) HPX_MALLOC;

/// Deletes the thread.
///
/// @param stack - The thread stack pointer.
void thread_delete(ustack_t *stack)
  HPX_NON_NULL(1);

/// Exit the current user-level thread, possibly with a return value.
void thread_exit(int status, const void *value, size_t size)
  HPX_NORETURN;

/// Invoking a thread's continuation.
///
/// @param       thread The thread.
/// @param            n The number of arguments to continue.
/// @param         args The arguments we are continuing.
void thread_continue_va(ustack_t* thread, int n, va_list* args)
  HPX_NON_NULL(1);

#endif  // LIBHPX_THREAD_H
