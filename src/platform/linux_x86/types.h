#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <libgen.h>
#include <cstring>

// Allows sharedptr and whatnot
#include <memory>

using namespace std;

// types
#define BOOLEAN bool
#define CHAR8 char
#define INT8U uint8_t
#define INT16U uint16_t
#define INT32U uint32_t
#define INT8S int8_t
#define INT16S int16_t
#define INT32S int32_t


#ifndef NULL
#define NULL nullptr
#endif

#define TRUE 1
#define FALSE 0

// OS
#define OS_TASK_NAME_SIZE 16
#define PetWatchdog() { }
#define OS_EVENT void*
#define OS_TCB void*
#define OS_TICKS_PER_SEC 100

// Error
#define ERROR_CODE_T int
#define STATUS_SUCCESS 0
#define OS_NO_ERR 0
#define OS_ERR_NONE 0
#define ERROR_FAILED -1
#define OS_ERR_FAILED -1
#define ERROR_CODE_INVALID_PARAM -2
#define ERROR_CODE_TIMEOUT -3
#define ERROR_CODE_NOT_SUPPORTED -4
#define ERROR_NOT_INITIALIZED -10
#define ERROR_INVALID_PARAMETER -11
#define ERROR_OPERATION_TIMED_OUT -12
#define STATUS_OPERATION_INCOMPLETE -13
#define ERROR_INVALID_CONFIGURATION -14
#define ERROR_INVALID_HANDLE -15
#define OS_ERR_TIMEOUT -16

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define SUCCEEDED(x) ((x) == STATUS_SUCCESS)
#define FAILED(x) ((x) != STATUS_SUCCESS)


#define RETURN_IF_FAILED(ec) \
    do { \
        ERROR_CODE_T _e = (ec); \
        if (_e != STATUS_SUCCESS) { \
            printf("RETURN_IF_FAILED: %s:%d -> error code: %d\n", __FILENAME__, __LINE__, _e); \
            return _e; \
        } \
    } while (0)

#define RETURN_EC_IF_TRUE(ec, condition) \
    do { \
        if (condition) { \
            printf("RETURN_EC_IF_TRUE: %s:%d -> condition true, returning %d\n", __FILENAME__, __LINE__, (ec)); \
            return (ec); \
        } \
    } while (0)

#define RETURN_EC_IF_FALSE(ec, condition) \
    do { \
        if (!(condition)) { \
            printf("RETURN_EC_IF_FALSE: %s:%d -> condition false, returning %d\n", __FILENAME__, __LINE__, (ec)); \
            return (ec); \
        } \
    } while (0)

#define RETURN_EC_IF_NULL(ec, ptr) \
    do { \
        if ((ptr) == NULL) { \
            printf("RETURN_EC_IF_NULL: %s:%d -> null pointer, returning %d\n", __FILENAME__, __LINE__, (ec)); \
            return (ec); \
        } \
    } while (0)

#define RETURN_EC_IF_FAILED(ec) \
    do { \
        if ((ec) != STATUS_SUCCESS) { \
            printf("RETURN_EC_IF_FAILED: %s:%d -> failed with %d\n", __FILENAME__, __LINE__, (ec)); \
            return (ec); \
        } \
    } while (0)

#define RETURN_NULL_IF_FAILED(ec) \
    do { \
        if ((ec) != STATUS_SUCCESS) { \
            printf("RETURN_EC_IF_FAILED: %s:%d -> failed with %d\n", __FILENAME__, __LINE__, (ec)); \
            return NULL; \
        } \
    } while (0)

#define RETURN_NULL_IF_TRUE(check) \
    do { \
        if (check) { \
            printf("RETURN_EC_IF_FAILED: %s:%d -> is not true when expected\n", __FILENAME__, __LINE__); \
            return NULL; \
        } \
    } while (0)

#define RETURN_NULL_IF_NULL(check) \
    do { \
        if (check == NULL) { \
            printf("RETURN_NULL_IF_NULL: %s:%d -> failed\n", __FILENAME__, __LINE__); \
            return NULL; \
        } \
    } while (0)

#define RETURN_BOOL_IF_FALSE(ret, check) \
    do { \
        if (!(check)) { \
            printf("RETURN_BOOL_IF_FALSE: %s:%d -> failed\n", __FILENAME__, __LINE__); \
            return ret; \
        } \
    } while (0)


#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

// Debug
#define DEBUG_NO_LOGGING 0
#define DEBUG_TRACE_MESSAGE 1
#define DEBUG_TRACE_ERROR 2
#define DEBUG_TRACE_WARNING 3
#define DEBUG_TRACE_INFO 4
#define DEBUG_TRACE_DEBUG 5
#define DEBUG_TRACE_VERBOSE 6

// Printing
#define DebugPrintf(verbosity, logLevel, debugID, format, ...) \
    do { \
        if ((verbosity) >= (0)) { \
            printf(format, ##__VA_ARGS__); \
        } \
    } while (0)


typedef enum
{
    TS_FALSE,
    TS_TRUE,
    TS_AUTO
} TRI_STATE_T;

