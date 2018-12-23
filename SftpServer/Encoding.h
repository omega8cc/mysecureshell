/*
MySecureShell permit to add restriction to modified sftp-server
when using MySecureShell as shell.
Copyright (C) 2007-2018 MySecureShell Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation (version 2)

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef _ENCODING_H_
#define _ENCODING_H_

#if(HAVE_ICONV||HAVE_LIBICONV)

/*@null@*/ char    *convertToUtf8(char *str, int freeAfter);
/*@null@*/ char    *convertFromUtf8(char *str, int freeAfter);
void	setCharset(/*@null@*/ const char *charset);

#else
#include <string.h>

#define convertToUtf8(_X, _Y)	((_Y) ? _X : strdup(_X))
#define convertFromUtf8(_X, _Y)	((_Y) ? _X : strdup(_X))
#define setCharset(_X)

#endif //HAVE_ICONV||HAVE_LIBICONV

#endif //_ENCODING_H_
