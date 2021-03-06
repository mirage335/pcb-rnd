#ifndef VTP0_H
#define VTP0_H

#include <stdlib.h>
#include <string.h>

/* Elem=void *; init=NULL
   pointer vector with new elements initialized to NULL.
*/

/* For documentation on the settings, check vti0.h */

typedef void * vtp0_ptr_t;

#define GVT(x) vtp0_ ## x
#define GVT_ELEM_TYPE vtp0_ptr_t
#define GVT_SIZE_TYPE size_t
#define GVT_DOUBLING_THRS 4096
#define GVT_START_SIZE 32
#define GVT_FUNC
#define GVT_SET_NEW_BYTES_TO 0

#include <genvector/genvector_impl.h>

#define GVT_REALLOC(vect, ptr, size)  realloc(ptr, size)
#define GVT_FREE(vect, ptr)           free(ptr)

#include <genvector/genvector_undef.h>

#endif
