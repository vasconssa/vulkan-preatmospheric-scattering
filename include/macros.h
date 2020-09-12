/*
 * Copyright (c) 2012-2020 Daniele Bartolini and individual contributors.
 * License: https://github.com/dbartolini/crown/blob/master/LICENSE
 */

#pragma once

#include "sx/platform.h"
#include "sx/macros.h"
#include <stdint.h>
#include <stddef.h>

#ifndef GB_DEBUG
	#define GB_DEBUG 0
#endif // GB_DEBUG

#ifndef GB_DEVELOPMENT
	#define GB_DEVELOPMENT 0
#endif // GB_DEVELOPMENT

/// @defgroup Core Core

/// @addtogroup Core
/// @{
typedef int8_t   s8;
typedef uint8_t  u8;
typedef int16_t  s16;
typedef uint16_t u16;
typedef int32_t  s32;
typedef uint32_t u32;
typedef int64_t  s64;
typedef uint64_t u64;
typedef float    f32;
typedef double   f64;
/// @}


#if defined(_MSC_VER)
	#define _ALLOW_KEYWORD_MACROS
#endif

#if !defined(alignof)
	#define alignof(x) __alignof(x)
#endif

#if !defined(__va_copy)
	#define __va_copy(dest, src) (dest = src)
#endif

#define countof(arr) (sizeof(arr)/sizeof(arr[0]))
#define container_of(ptr, type, member) ((char*)ptr - offsetof(type, member))

#define GB_STRINGIZE(value) GB_STRINGIZE_(value)
#define GB_STRINGIZE_(value) #value

#define GB_NOOP(...) do { (void)0; } while (0)
#define GB_UNUSED(x) do { (void)(x); } while (0)
#define GB_STATIC_ASSERT(condition, ...) static_assert(condition, "" # __VA_ARGS__)

#if SX_COMPILER_GCC || SX_COMPILER_CLANG
	#define GB_LIKELY(x) __builtin_expect((x), 1)
	#define GB_UNLIKELY(x) __builtin_expect((x), 0)
	#define GB_UNREACHABLE() __builtin_unreachable()
	#define GB_ALIGN_DECL(align, decl) decl __attribute__ ((aligned (align)))
	#define GB_THREAD __thread
#elif SX_COMPILER_MSVC
	#define GB_LIKELY(x) (x)
	#define GB_UNLIKELY(x) (x)
	#define GB_UNREACHABLE()
	#define GB_ALIGN_DECL(align_, decl) __declspec (align(align_)) decl
	#define GB_THREAD __declspec(thread)
#else
	#error "Unknown compiler"
#endif

