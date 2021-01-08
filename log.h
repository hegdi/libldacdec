#ifndef __LOG_H_
#define __LOG_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG
#define LOG( format, ... ) \
{   \
    printf( format, __VA_ARGS__ );  \
}    

#define LOG_ARRAY( var, format )        \
{                                       \
    __auto_type len = sizeof(var)/sizeof(var[0]);      \
    printf("%s[%ld] =\n", #var, len );  \
    for( typeof(len) i=0; i<len; ++i )          \
    {                                   \
        printf( format, var[i] );       \
        if( ((i+1)%16) == 0 )           \
            printf("\n");               \
    }                                   \
    printf("\n");                       \
}

#define LOG_ARRAY_LEN( var, format, length )\
{                                           \
    __auto_type len = length;               \
    printf("%s[%d] =\n", #var, len );       \
    for( typeof(len) i=0; i<len; ++i )      \
    {                                       \
        printf( format, var[i] );           \
    }                                       \
    printf("\n");                           \
}

#define LOG_FUNCTION() { printf("%s\n", __FUNCTION__ ); }

#else

#define LOG( format, ... )
#define LOG_ARRAY( var, format )
#define LOG_ARRAY_LEN( var, format, len )
#define LOG_FUNCTION()

#endif


#endif // __LOG_H_

