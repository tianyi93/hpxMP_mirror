//
// Created by tianyi on 9/17/18.
//

#ifndef HPXMP_GCC_HPXMP_H
#define HPXMP_GCC_HPXMP_H

#include "kmp_ftn_os.h"
extern "C" void
xexpand(KMP_API_NAME_GOMP_PARALLEL)(void (*task)(void *), void *data, unsigned num_threads, unsigned int flags);
extern "C" void 
xexpand(KMP_API_NAME_GOMP_TASK)(void (*func)(void *), void *data, void (*copy_func)(void *, void *),
                                long arg_size, long arg_align, bool if_cond, unsigned gomp_flags);
extern "C" void
xexpand(KMP_API_NAME_GOMP_TASKWAIT)(void);

extern "C" int
xexpand(KMP_API_NAME_GOMP_SINGLE_START)(void);

extern "C" void *
xexpand(KMP_API_NAME_GOMP_SINGLE_COPY_START)(void);

extern "C" void
xexpand(KMP_API_NAME_GOMP_SINGLE_COPY_END)(void *data);

extern "C" void
xexpand(KMP_API_NAME_GOMP_BARRIER)(void);

extern "C" void
xexpand(KMP_API_NAME_GOMP_CRITICAL_START)(void);

extern "C" void
xexpand(KMP_API_NAME_GOMP_CRITICAL_END)(void);

extern "C" void
xexpand(KMP_API_NAME_GOMP_CRITICAL_NAME_START)(void** ptr);

extern "C" void
xexpand(KMP_API_NAME_GOMP_CRITICAL_NAME_END)(void** ptr);

extern "C" void
xexpand(KMP_API_NAME_GOMP_ATOMIC_START)(void);

extern "C" void
xexpand(KMP_API_NAME_GOMP_ATOMIC_END)(void);
#endif //HPXMP_GCC_HPXMP_H
