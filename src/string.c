/**
 * @file string.c
 * Implements string handling
 *
 * @note: for efficiency reasons, later builds may spread the
 * individual functions across different source modules. I was a 
 * bit lazy to do this right now and I am totally unsure if it
 * really is worth the effort.
 *//*
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
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>

#include "libestr.h"

#define ERR_ABORT {r = 1; goto done; }

#if !defined(NDEBUG)
#	define CHECK_STR 
#	define ASSERT_STR 
#else
#	define CHECK_STR \
		if(s->objID != ES_STRING_OID) { \
			r = -1; \
			goto done; \
		}
#	define ASSERT_STR(s) assert((s)->objID == ES_STRING_OID)
#endif /* #if !defined(NDEBUG) */


/* ------------------------------ HELPERS ------------------------------ */

/**
 * Extend string buffer.
 * This is called if the size is insufficient. Note that the string
 * pointer will be changed.
 * @param[in/out] ps pointer to (pointo to) string to be extened
 * @param[in] minNeeded minimum number of additional bytes needed
 * @returns 0 on success, something else otherwise
 */
int
es_extendBuf(es_str_t **ps, size_t minNeeded)
{
	int r = 0;
	es_str_t *s = *ps;
	size_t newSize;

	ASSERT_STR(s);
	/* first compute the new size needed */
	if(minNeeded > s->allocInc) {
		/* TODO: think about optimizing this based on allocInc */
		newSize = s->lenBuf + minNeeded;
	} else {
		newSize = s->lenBuf + s->allocInc;
		/* set new allocInc for fast growing string */
		if(2 * s->allocInc > 65535) /* prevent overflow! */
			s->allocInc = 65535;
		else
			s->allocInc = 2 * s->allocInc;
	}

	if((s = (es_str_t*) realloc(s, newSize + sizeof(es_str_t))) == NULL) {
		r = errno;
		goto done;
	}
	s->lenBuf = newSize;
	*ps = s;

done:
	return r;
}


/* ------------------------------ END HELPERS ------------------------------ */

es_str_t *
es_newStr(size_t lenhint)
{
	es_str_t *s;
	/* we round length to a multiple of 8 in the hope to reduce
	 * memory fragmentation.
	 */
	if(lenhint & 0x07)
		lenhint = lenhint - (lenhint & 0x07) + 8;

	if((s = malloc(sizeof(es_str_t) + lenhint)) == NULL)
		goto done;

#	ifndef NDEBUG
	s->objID = ES_STRING_OID;
#	endif
	s->lenBuf = lenhint;
	s->allocInc = lenhint;
	s->lenStr = 0;

done:
	return s;
}


es_str_t*
es_newStrFromCStr(char *cstr, size_t len)
{
	es_str_t *s;
	assert(strlen(cstr) == len);
	
	if((s = es_newStr(len)) == NULL) goto done;
	memcpy(es_getBufAddr(s), cstr, len);
	s->lenStr = len;

done:
	return s;
}


es_str_t*
es_newStrFromSubStr(es_str_t *str, size_t start, size_t len)
{
	es_str_t *s;
	
	if((s = es_newStr(len)) == NULL) goto done;

	if(start > es_strlen(str))
		goto done;
	else if(start + len > es_strlen(str))
		len = es_strlen(str) - start;

	memcpy(es_getBufAddr(s), es_getBufAddr(str)+start, len);
	s->lenStr = len;

done:
	return s;
}

void
es_deleteStr(es_str_t *s)
{
	ASSERT_STR(s);
#	if !defined(NDEBUG)
	s->objID = ES_STRING_FREED;
#	endif
	free(s);
}


int
es_strbufcmp(es_str_t *s, unsigned char *buf, size_t lenBuf)
{
	int r;
	size_t i;
	unsigned char *c;

	ASSERT_STR(s);
	assert(buf != NULL);
	if(s->lenStr < lenBuf)
		r = -1;
	else if(s->lenStr > lenBuf)
		r = 1;
	else {
		c = es_getBufAddr(s);
		r = 0;	/* assume: strings equal, will be reset if not */
		for(i = 0 ; i < s->lenStr ; ++i) {
			if(c[i] != buf[i]) {
				r = c[i] - buf[i];
				break;
			}
		}
	}
	return r;
}


int
es_addBuf(es_str_t **ps1, char *buf, size_t lenBuf)
{
	int r;
	size_t newlen;
	es_str_t *s1 = *ps1;

	ASSERT_STR(s1);
	if(lenBuf == 0) {
		r = 0;
		goto done;
	}

	newlen = s1->lenStr + lenBuf;
	if(s1->lenBuf < newlen) {
		/* we need to extend */
		if((r = es_extendBuf(ps1, newlen - s1->lenBuf)) != 0) goto done;
		s1 = *ps1;
	}
	
	/* do the actual copy, we now *have* the space required */
	memcpy(es_getBufAddr(s1)+s1->lenStr, buf, lenBuf);
	s1->lenStr = newlen;
	r = 0; /* all well */

done:
	return r;
}


char *
es_str2cstr(es_str_t *s, char *nulEsc)
{
	char *cstr;
	size_t lenEsc;
	int nbrNUL;
	size_t i, iDst;
	unsigned char *c;

	/* detect number of NULs inside string */
	c = es_getBufAddr(s);
	nbrNUL = 0;
	for(i = 0 ; i < s->lenStr ; ++i) {
		if(c[i] == 0x00)
			++nbrNUL;
	}

	if(nbrNUL == 0) {
		/* no special handling needed */
		if((cstr = malloc(s->lenStr + 1)) == NULL) goto done;
		if(s->lenStr > 0)
			memcpy(cstr, c, s->lenStr);
		cstr[s->lenStr] = '\0';
	} else {
		/* we have NUL bytes present and need to process them
		 * during creation of the C string.
		 */
		lenEsc = (nulEsc == NULL) ? 0 : strlen(nulEsc);
		if((cstr = malloc(s->lenStr + nbrNUL * (lenEsc - 1) + 1)) == NULL)
			goto done;
		for(i = iDst = 0 ; i < s->lenStr ; ++i) {
			if(c[i] == 0x00) {
				if(lenEsc == 1) {
					cstr[iDst++] = *nulEsc;
				} else {
					memcpy(cstr + iDst, nulEsc, lenEsc);
					iDst += lenEsc;
				}
			} else {
				cstr[iDst++] = c[i];
			}
		}
		cstr[iDst] = '\0';
	}

done:
	return cstr;
}


/**
 * Get numerical value of a hex digit. This is a helper function.
 * @param[in] c a character containing 0..9, A..Z, a..z anything else
 * is an (undetected) error.
 */
static inline int hexDigitVal(char c)
{
	int r;
	if(c < 'A')
		r = c - '0';
	else if(c < 'a')
		r = c - 'A' + 10;
	else
		r = c - 'a' + 10;
	return r;
}

/* Handle the actual unescaping.
 * a helper to es_unescapeStr(), to help make the function easier to read.
 */
static inline void
doUnescape(unsigned char *c, size_t lenStr, size_t *iSrc, size_t iDst)
{
	if(c[*iSrc] == '\\') {
		if(++(*iSrc) == lenStr) {
			/* error, incomplete escape, treat as single char */
			c[iDst] = '\\';
		}
		/* regular case, unescape */
		switch(c[*iSrc]) {
		case '0':
			c[iDst] = '\0';
			break;
		case 'a':
			c[iDst] = '\007';
			break;
		case 'b':
			c[iDst] = '\b';
			break;
		case 'f':
			c[iDst] = '\014';
			break;
		case 'n':
			c[iDst] = '\n';
			break;
		case 'r':
			c[iDst] = '\r';
			break;
		case 't':
			c[iDst] = '\t';
			break;
		case '\'':
			c[iDst] = '\'';
			break;
		case '"':
			c[iDst] = '"';
			break;
		case '?':
			c[iDst] = '?';
			break;
		case 'x':
			if(    (*iSrc)+2 == lenStr
			   || !isxdigit(c[(*iSrc)+1])
			   || !isxdigit(c[(*iSrc)+2])) {
				/* error, incomplete escape, use as is */
				c[iDst] = '\\';
				--(*iSrc);
			}
			c[iDst] = (hexDigitVal(c[(*iSrc)+1]) << 4) +
				  hexDigitVal(c[(*iSrc)+2]);
			*iSrc += 2;
		/* TODO: other sequences */
		}
	} else {
		/* regular character */
		c[iDst] = c[*iSrc];
	}
}

void
es_unescapeStr(es_str_t *s)
{
	size_t iSrc, iDst;
	unsigned char *c;
	assert(s != NULL);

	c = es_getBufAddr(s);
	/* scan for first escape sequence (if we are luky, there is none!) */
	iSrc = 0;
	while(iSrc < s->lenStr && c[iSrc] != '\\')
		++iSrc;
	/* now we have a sequence or end of string. In any case, we process
	 * all remaining characters (maybe 0!) and unescape.
	 */
	if(iSrc != s->lenStr) {
		iDst = iSrc;
		while(iSrc < s->lenStr) {
			doUnescape(c, s->lenStr, &iSrc, iDst);
			++iSrc;
			++iDst;
		}
		s->lenStr = iDst;
	}
}
