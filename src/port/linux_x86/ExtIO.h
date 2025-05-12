/********************************************************************************************************
    File Name:  ExtIO.h

    Author:     Rob Dahlgren

    Notes:      These are support routines that extend the common library
				included with the IAR compiler.

-------------------------------------------------HISTORY-------------------------------------------------

    REV  Date       Initials  Notes
    -----------------------------------------------------------------------------------------------------
    New             RLD       Initial Release
    -----------------------------------------------------------------------------------------------------
    A    8-01-06    RLD       Added strnicmp function
---------------------------------------------------------------------------------------------------------
    © Copyright 2006 Innovative Advantage Inc.  All Rights Reserved.
********************************************************************************************************/
#ifndef _EXT_IO
#define _EXT_IO

#include "types.h"
#include <vector>

// Performs a case insensitive string comparison.
INT32S ext_stricmp( const CHAR8 *str1, const CHAR8 *str2 );

// Performs a case insensitive string comparison on the first <count> characters.
INT32S ext_strnicmp ( const CHAR8 * str1, const CHAR8 * str2, INT32U count );

// converts a string to upper case
CHAR8 * ext_strupr(CHAR8 *str);

// converts a string to lower case
CHAR8 * ext_strlwr(CHAR8 *str);

// Trims leading and trailing whitespace from a string.
CHAR8 * ext_Trim(CHAR8 *str);

size_t ext_strnlen(const CHAR8 *pString, size_t max_len);

#ifdef __cplusplus
extern "C++"
{
// Returns true if string is numeric value.
extern bool is_numeric(const string &str);

// std::string trim routines
extern string &ltrim(string &str);
extern string &rtrim(string &str);
extern string &trim(string &str);

template<typename T>
string to_string(T x)
{
  int length = snprintf( NULL, 0, "%d", x );
  char* buf = new char[length + 1];
  snprintf( buf, length + 1, "%d", x );
  string str( buf );
  delete[] buf;
  return str;
}

string to_lower(string str);

// Splits a C string into a vector<string> based on the delimiter char supplied.
extern ERROR_CODE_T SplitString(vector<string> &outStrings, const CHAR8 *str, INT32U maxLength, CHAR8 delimiter, bool removeDelimiter);

// Splits a C string into a vector<string> based on the delimiter char supplied.
extern ERROR_CODE_T SplitString(vector<string> &outStrings, const string &str, CHAR8 delimiter, bool removeDelimiter);
}
#endif

#endif

