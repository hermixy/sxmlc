/*
    This file is part of sxmlc.

    sxmlc is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    sxmlc is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with sxmlc.  If not, see <http://www.gnu.org/licenses/>.

	Copyright 2010 - Matthieu Labas
*/
#ifndef _UTILS_H_
#define _UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MEM_INCR_RLA
#define MEM_INCR_RLA 256 /* Initial buffer size and increment for memory reallocations */
#endif

#ifndef false
#define false 0
#endif

#ifndef true
#define true 1
#endif

/*
 Buffer data source used by 'read_line_alloc' when required.
 'buf' should be 0-terminated.
 */
typedef struct _DataSourceBuffer {
	const char* buf;
	int cur_pos;
} DataSourceBuffer;

typedef FILE DataSourceFile;

typedef enum _DataSourceType {
	DATA_SOURCE_FILE = 0,
	DATA_SOURCE_BUFFER,
	DATA_SOURCE_MAX
} DataSourceType;

/*
 Functions to get next byte from buffer data source and know if the end has been reached.
 Return as 'fgetc' and 'feof' would for 'FILE*'.
 */
int _bgetc(DataSourceBuffer* ds);
int _beob(DataSourceBuffer* ds);

/*
 Reads a line from data source 'in', eventually (re-)allocating a given buffer 'line'.
 Characters read will be stored in 'line' starting at 'i0' (this allows multiple calls to
 'read_line_alloc' on the same 'line' buffer without overwriting it at each call).
 'in_type' specifies the type of data source to be read: 'in' is 'FILE*' if 'in_type'
 'sz_line' is the size of the buffer 'line' if previously allocated. 'line' can point
 to NULL, in which case it will be allocated '*sz_line' bytes. After the function returns,
 '*sz_line' is the actual buffer size. This allows multiple calls to this function using the
 same buffer (without re-allocating/freeing).
 If 'sz_line' is non NULL and non 0, it means that '*line' is a VALID pointer to a location
 of '*sz_line'.
 Searches for character 'from' until character 'to'. If 'from' is 0, starts from
 current position. If 'to' is 0, it is replaced by '\n'.
 If 'keep_fromto' is 0, removes characters 'from' and 'to' from the line.
 If 'interest_count' is not NULL, will receive the count of 'interest' characters while searching
 for 'to' (e.g. use 'interest'='\n' to count lines in file).
 Returns the number of characters in the line or 0 if an error occurred.
 'read_line_alloc' uses constant 'MEM_INCR_RLA' to reallocate memory when needed. It is possible
 to override this definition to use another value.
 */
int read_line_alloc(void* in, DataSourceType in_type, char** line, int* sz_line, int i0, char from, char to, int keep_fromto, char interest, int* interest_count);

/*
 Concatenates the string pointed at by 'src1' with 'src2' into '*src1' and
 return it ('*src1').
 Return NULL when out of memory.
 */
char* strcat_alloc(char** src1, const char* src2);

/*
 Strip spaces at the beginning and end of 'str', modifying 'str'.
 If 'repl_sq' is not '\0', squeezes spaces to an single character ('repl_sq').
 If not '\0', 'protect' is used to protect spaces from being deleted (usually a backslash).
 Returns the string or NULL if 'protect' is a space (which would not make sense).
 */
char* strip_spaces(char* str, char repl_sq, char protect);

/*
 Remove '\' characters from 'str'.
 Return 'str'.
 */
char* str_unescape(char* str);

/*
 Split 'str' into a left and right part around a separator 'sep'.
 The left part is located between indexes 'l0' and 'l1' while the right part is
 between 'r0' and 'r1' and the separator position is at 'i_sep' (whenever these are
 not NULL).
 If 'ignore_spaces' is 'true', computed indexes will not take into account potential
 spaces around the separator as well as before left part and after right part.
 if 'ignore_quotes' is 'true', " or ' will not be taken into account when parsing left
 and right members.
 Whenever the right member is empty (e.g. "attrib" or "attrib="), '*r0' is initialized
 to 'str' size and '*r1' to '*r0-1'.
 If the separator was not found (i.e. left member only), '*i_sep' is '-1'.
 Return 'false' when 'str' is malformed, 'true' when splitting was successful.
 */
int split_left_right(char* str, char sep, int* l0, int* l1, int* i_sep, int* r0, int* r1, int ignore_spaces, int ignore_quotes);

/*
 Replace occurrences of special HTML characters escape sequences (e.g. '&amp;') found in 'html'
 by its character equivalent (e.g. '&') into 'str'.
 If 'html' and 'str' are the same pointer replacement is made in 'str' itself, overwriting it.
 If 'str' is NULL, replacement is made into 'html', overwriting it.
 Returns 'str' (or 'html' if 'str' was NULL).
 */
char* html2str(char* html, char* str);

/*
 Replace occurrences of special characters (e.g. '&') found in 'str' into their HTML escaped
 equivalent (e.g. '&amp;') into 'html'.
 'html' is supposed allocated to the correct size (e.g. using 'malloc(strlen_html(str))') and
 different from 'str' (unlike 'html2str'), as string will expand.
 Return 'html' or NULL if 'str' or 'html' are NULL, or when 'html' is 'str'.
*/
char* str2html(char* str, char* html);

/*
 Return the length of 'str' as if all its special character were replaced by their HTML
 equivalent.
 Return 0 if 'str' is NULL.
 */
int strlen_html(char* str);

/*
 Print 'str' to 'f', transforming special characters into their HTML equivalent.
 Returns the number of output characters.
 */
int fprintHTML(FILE* f, char* str);

/*
 Checks whether 'str' corresponds to 'pattern'.
 'pattern' can use wildcads such as '*' (any potentially empty string) or
 '?' (any character) and use '\' as an escape character.
 Returns 'true' when 'str' matches 'pattern', 'false' otherwise.
 */
int regstrcmp(char* str, char* pattern);

#ifdef __cplusplus
}
#endif

#endif
