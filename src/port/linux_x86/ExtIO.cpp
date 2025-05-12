/********************************************************************************************************
    File Name:  ExtIO.cpp

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
#include "types.h"
#include <ctype.h>
#include <vector>
#include <string>
using namespace std;

/*******************************************************************************************************
                            INT32S ext_stricmp( CHAR8 *str1, CHAR8 *str2 )
 This routine performs a case insensitive string comparison, returning the difference between
 str1 and str2. This is identical to the stricmp function in the standard library.
********************************************************************************************************/
INT32S ext_stricmp( const CHAR8 *str1, const CHAR8 *str2 )
{
    INT32S f, l;

    do {
        f = (INT8U)(*(str1++));
        if ( (f >= 'A') && (f <= 'Z') )
            f += 0x20;

        l = (INT8U)(*(str2++));
        if ( (l >= 'A') && (l <= 'Z') )
            l += 0x20;
    } while ( f && (f == l) );

    return(f - l);
}

/*******************************************************************************************************
                       INT32S ext_strnicmp ( CHAR8 * str1, CHAR8 * str2, INT32U count )
 This routine performs a case insensitive string comparison on the first <count> cahracters,
 returning the difference between str1 and str2. This is identical to the strnicmp function
 in the standard library.
********************************************************************************************************/
INT32S ext_strnicmp ( const CHAR8 * str1, const CHAR8 * str2, INT32U count )
{
    INT32S f, l;

    do {
        if ( ((f = (unsigned char)(*(str1++))) >= 'A') && (f <= 'Z') )
            f -= 'A' - 'a';

        if ( ((l = (unsigned char)(*(str2++))) >= 'A') && (l <= 'Z') )
            l -= 'A' - 'a';

    } while ( --count && f && (f == l) );

    return ( f - l );
}

/*******************************************************************************************************
                            CHAR8* ext_strupr( CHAR8 *str)
 This routine converts a string to upper case, returning the pointer to the string
 This is identical to the ext_strupr function in the standard library.
********************************************************************************************************/

CHAR8 * ext_strupr(CHAR8 *str)
{
    CHAR8 *string = str;

    while (*str)
    {
        if ( (*str >= 'a') && (*str <= 'z') )
            *str -= 0x20;
         ++str;
    };

    return string;
}

/*******************************************************************************************************
                            CHAR8* ext_strlwr( CHAR8 *str)
 This routine converts a string to upper case, returning the pointer to the string
 This is identical to the ext_strlwr function in the standard library.
********************************************************************************************************/
CHAR8 * ext_strlwr(CHAR8 *str)
{
      CHAR8 *string = str;

      while (*str)
      {
          if ( (*str >= 'A') && (*str <= 'Z') )
              *str += 0x20;
           ++str;
      };

      return string;
}

#ifndef __x86_64__

/*******************************************************************************************************
                            CHAR8* strcasestr( const CHAR8 *str1, const CHAR8 *str2 )
 This routine returns the 1st occurence of str2 inside str1, case insensitive.
 This is identical to the strcasestr function in the standard library.
********************************************************************************************************/
CHAR8 * strcasestr( const CHAR8 *str1, const CHAR8 *str2 )
{
    const CHAR8 *pHaystackStart = str1;
    const CHAR8 *pHaystackCur = str1;
    const CHAR8 *pNeedle = str2;
    bool match = false;

    while( *pHaystackCur != '\0' )
    {
        match = false;

        if( *pHaystackCur >= 'a' && *pHaystackCur <= 'z' &&
            *pNeedle >='A' && *pNeedle <= 'Z')
        {
            if( (*pNeedle - 'A') ==  (*pHaystackCur - 'a') )
                match = true;
        }
        else if( *pHaystackCur >= 'A' && *pHaystackCur <= 'Z' &&
            *pNeedle >='a' && *pNeedle <= 'z')
        {
            if( (*pNeedle - 'a') ==  (*pHaystackCur - 'A') )
                match = true;
        }
        else if( *pHaystackCur == *pNeedle )
        {
            match = true;
        }

        pHaystackCur++;
        if( match )
        {
            pNeedle++;
            if( *pNeedle == '\0' )
            {
                return (CHAR8*)pHaystackStart;
            }
        }
        else
        {
            pNeedle = str2;
            pHaystackStart = pHaystackCur;
        }
    }

    return NULL;
}

#endif

/*******************************************************************************************************
                            CHAR8 * ext_Trim(CHAR8 *str)
 This routine removes the whitespace at the beginning and end of the string.
********************************************************************************************************/
CHAR8 * ext_Trim(CHAR8 *str)
{
    CHAR8 *startString = str;
    CHAR8 *string = str;

    // Work forward first to remove leading whitespace.
    while(*string)
    {
        if( (*string >= 0x09 && *string <= 0x0c) || *string == 0x20 )
        {
            string++;
        }
        else
        {
            // OK, here we have now set the beginning of the string excluding whitespace.
            while(*string)
            {
                *str = *string;
                str++;
                string++;
            }
            *str = *string;
            string = str;
            break;
        }
    }

    // Now work backwards to remove any trailing whitespace
    string--;
    while(string >= startString)
    {
        if( (*string >= 0x09 && *string <= 0x0c) || *string == 0x20 )
        {
            *string = '\0';
            string--;
        }
        else
        {
            break;
        }
    }

    return startString;
}

/*******************************************************************************************************
********************************************************************************************************/
size_t ext_strnlen(const CHAR8 *pString, size_t max_len)
{
    size_t i = 0;
    for(; (i < max_len) && pString[i]; ++i);
    return i;
}

#include <algorithm>
#include <functional>

using namespace std;

/*******************************************************************************************************
                        bool is_numeric(const string &str)

 Returns true if the all characters are numbers. Does not account for decimal or + and -.

 Parameters:
 string &str:   The string to parse.

 Returns: true if all characters are numeric
********************************************************************************************************/
bool is_numeric(const string &str)
{
    string::const_iterator it = str.begin();
    while (it != str.end() && isdigit(*it)) ++it;
    return !str.empty() && it == str.end();
}

/*******************************************************************************************************
********************************************************************************************************/
string &ltrim(string &str)
{
    str.erase(str.begin(), find_if(str.begin(), str.end(),
            not1(ptr_fun<int, int>(isspace))));
    return str;
}

/*******************************************************************************************************
********************************************************************************************************/
string &rtrim(string &str)
{
    str.erase(find_if(str.rbegin(), str.rend(),
            not1(ptr_fun<int, int>(isspace))).base(), str.end());
    return str;
}

/*******************************************************************************************************
********************************************************************************************************/
string &trim(string &str)
{
    return ltrim(rtrim(str));
}

/*******************************************************************************************************
********************************************************************************************************/
string to_lower(string str)
{
    string outStr(str.size(), '\0');
    for (INT32U i=0; i<str.size(); i++)
    {
        CHAR8 c = str[i];
        if (c <= 'Z' && c >= 'A')
            c += 0x20;

        outStr[i] = c;
    }

    return outStr;
}

/*******************************************************************************************************
 ERROR_CODE_T SplitString(vector<string> &outStrings, CHAR8 *str, INT32U maxLength, CHAR8 delimiter, bool removeDelimiter)

 Splits a string, returning a vector type string.

 Parameters:
 vector<string> &outStrings: An out reference to a vector,string> object.
 CHAR8 *str:   The C Style string to parse.
 INT32U maxLength: Max length of the string to parse.
 CHAR8 delimiter: The char that the incoming string is split on.
 bool removeDelimiter: If set to true, the delimiter char is not included in the returned strings.

 Returns: STATUS_SUCCESS is sucessful.
********************************************************************************************************/
ERROR_CODE_T SplitString(vector<string> &outStrings, const CHAR8 *str, INT32U maxLength, CHAR8 delimiter, bool removeDelimiter)
{
    RETURN_EC_IF_NULL(ERROR_INVALID_PARAMETER, str);
    RETURN_EC_IF_TRUE(ERROR_INVALID_PARAMETER, maxLength == 0);

    INT32U bytesProcessed;
    INT32U delimiterAdd = (removeDelimiter==false?1:0);
    CHAR8 const *pStartString = str;
    outStrings.empty();

    for( bytesProcessed=0; bytesProcessed<maxLength; bytesProcessed++ )
    {
        if( str[bytesProcessed] == delimiter )
        {
            outStrings.push_back(string(pStartString, &str[bytesProcessed + delimiterAdd] - pStartString));
            pStartString = &str[bytesProcessed+1];
        }
        else if( str[bytesProcessed] == '\0' )
        {
            break;
        }
    }

    if( pStartString != &str[bytesProcessed] )
    {
        // We ended the parsing with some additional data at the end that was not terminated by a NULL or a delimiter.
        // We need to throw this data into the final vector entry.
        outStrings.push_back(string(pStartString, &str[bytesProcessed] - pStartString));
    }
    return STATUS_SUCCESS;
}

/*******************************************************************************************************
 ERROR_CODE_T SplitString(vector<string> &outStrings, string &str, CHAR8 delimiter, bool removeDelimiter)

 Splits a string, returning a vector type string.

 Parameters:
 vector<string> &outStrings: An out reference to a vector,string> object.
 string &str:   The string to parse.
 CHAR8 delimiter: The char that the incoming string is split on.
 bool removeDelimiter: If set to true, the delimiter char is not included in the returned strings.

 Returns: STATUS_SUCCESS is sucessful.
********************************************************************************************************/
ERROR_CODE_T SplitString(vector<string> &outStrings, const string &str, CHAR8 delimiter, bool removeDelimiter)
{
    return SplitString(outStrings, str.c_str(), str.size(), delimiter, removeDelimiter);
}

