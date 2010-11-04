/**
 * @mainpage
 * libestr - some essentials for string handling (and a bit more)
 *
 * Copyright 2010 by Rainer Gerhards and Adiscon GmbH.
 *
 *
 *//*
 *
 * libestr - some essentials for string handling (and a bit more)
 * Copyright 2010 by Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of libestr.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * A copy of the LGPL v2.1 can be found in the file "COPYING" in this distribution.
 */
#ifndef LIBESTR_H_INCLUDED
#define	LIBESTR_H_INCLUDED


#define ES_STRING_OID 0x00FAFA00
#define ES_STRING_FREED 0x00FFFF00

typedef struct
{	
#ifndef	NDEBUG
	unsigned objID;		/**< object ID */
#endif
	unsigned short allocInc; /**< size that new allocs will be grown */
	size_t lenBuf;		/**< length of buffer (including free space) */
	size_t lenStr;		/**< actual length of string */
} es_str_t;


/**
 * Return library version as a classical NUL-terminated C-String.
 */
char *es_version(void);

/**
 * Return length of provided string object.
 */
static inline size_t es_strlen(es_str_t *str)
{
	return(str->lenStr);
}

/**
 * Create a new string object.
 * @param[in] lenhint expected max length of string. Do \b not use too large value.
 * @returns pointer to new object or NULL on error
 */
es_str_t* es_newStr(size_t lenhint);

/**
 * delete a string object.
 * @param[in] str string to be deleted.
 */
void es_deleteStr(es_str_t *str);


/**
 * Create a new string object based on a "traditional" C string.
 * @param[in] cstr traditional, '\0'-terminated C string
 * @param[in] len length of str. Use strlen() if you don't know it, but often it
 *  		the length is known and we use this as a time-safer (if present).
 * @returns pointer to new object or NULL on error
 */
es_str_t* es_newStrFromCStr(char *cstr, size_t len);


/**
 * Compare two string objects.
 * Semantics are the same as strcmp().
 *
 * @param[in] s1 frist string
 * @param[in] s2 second string
 * @returns 0 if equal, negative if s1<s2, positive if s1>s2
*/
int es_strcmp(es_str_t *s1, es_str_t *s2);


/**
 * Get the base address for the string's buffer.
 * Proper use for library users is to gain read-only access to the buffer,
 * so that it may be used inside an i/o request or similar things. Note that
 * it is an \b invalid assumption that the buffer address keeps constant between
 * library calls. This is only guaranteed for read-only methods. For example,
 * the methods used to grow the string may be forced to reallocate the buffer
 * on a new address with sufficiently free space.
 *
 * @param[in] s string object
 * @returns address of buffer <b>Note: this is NOT a zero-terminated C string!</b>
 */
static inline unsigned char *
es_getBufAddr(es_str_t *s)
{
	return ((unsigned char*) s) + sizeof(s);
}

/**
 * Extend string buffer.
 * This is called if the size is insufficient. Note that the string
 * pointer will be changed. This is an \b internal function that should
 * \b not be called from any lib user app.
 *
 * @param[in/out] ps pointer to (pointo to) string to be extened
 * @param[in] minNeeded minimum number of additional bytes needed
 * @returns 0 on success, something else otherwise
 */
int es_extendBuf(es_str_t **ps, size_t minNeeded);

/**
 * Append a character to the current string object.
 * Note that the pointer to the string object may change. This
 * is because we may need to aquire more memory.
 * @param[in/out] ps string to be extened (updatedable pointer required!)
 * @returns 0 on success, something else otherwise
 */
static inline int
es_addChar(es_str_t **ps, unsigned char c)
{
	int r = 0;

	if((*ps)->lenStr >= (*ps)->lenBuf) {  
		if((r = es_extendBuf(ps, 1)) != 0) goto done;
	}

	/* ok, when we reach this, we have sufficient memory */
	*(es_getBufAddr(*ps) + (*ps)->lenStr++) = c;

done:
	return r;
}


/**
 * Append a memory buffer to a string.
 * This is the method that almost all other append methods actually use.
 *
 * @param[in/out] ps1 updateable pointer to to-be-appended-to string
 * @param[in] buf buffer to append
 * @param[in] lenBuf length of buffer
 *
 * @returns 0 on success, something else otherwise
 */
int es_addBuf(es_str_t **ps1, char *buf, size_t lenBuf);


/**
 * Append a second string to the first one.
 *
 * @param[in/out] ps1 updateable pointer to to-be-appended-to string
 * @param[in] s2 string to append
 *
 * @returns 0 on success, something else otherwise
 */
static inline int
es_addStr(es_str_t **ps1, es_str_t *s2)
{
	return es_addBuf(ps1, (char*) es_getBufAddr(s2), s2->lenStr);
}

/**
 * Obtain a traditional C-String from a string object.
 * The string object is not modified. Note that the C string is not
 * necessarily exactly the same string: C Strings can not contain NUL
 * characters, and as such they need to be either encoded or dropped.
 * This is done by this function. The user can specify with which character
 * sequence (a traditional C String) it shall be replaced.
 * @note
 * This function has to do a lot of work, and should not be called unless
 * absolutely necessary. If possible, use the native representation of
 * the string object. For example, you can use the buffer address and 
 * string length in most i/o calls, if you use the native versions and avoid
 * the C string i/o calls.
 *
 * @param[in] s string object
 * @param[in] nulEsc escape sequence for NULs. If NULL, NUL characters will be dropped.
 *
 * @returns NULL in case of error, otherwise a suitably-encoded standard C string.
 * 	This string is allocated from the dynamic memory pool and must be freed
 * 	by the caller.
 */
char *es_str2cstr(es_str_t *s, char *nulEsc);

#endif /* #ifndef LIBESTR_H_INCLUDED */
