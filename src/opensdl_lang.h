/*
 * Copyright (C) Jonathan D. Belanger 2018.
 *
 *  OpenSDL is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  OpenSDL is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with OpenSDL.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Description:
 *
 *  This header file contains all the output function definitions for all the
 *  supported languages.
 *
 * Revision History:
 *
 *  V01.000	Aug 26, 2018	Jonathan D. Belanger
 *  Initially written.
 *
 *  V01.001	Sep  6, 2018	Jonathan D. Belanger
 *  Updated the copyright to be GNUGPL V3 compliant.
 */
#ifndef _OPENSDL_LANH_H_
#define _OPENSDL_LANH_H_ 1

#include <time.h>

#define SDL_K_FUNC_PER_LANG	4
#define SDL_K_COMMENT_LEN	80+1

/*
 * Typedefs to define output function calling arrays.
 */
typedef	int (*_SDL_FUNC)();
typedef _SDL_FUNC SDL_LANG_FUNC[SDL_K_FUNC_PER_LANG];

/*
 * Define the C/C++ output function prototypes.
 */
int sdl_c_commentStars(FILE *fp, _Bool twoNL);
int sdl_c_createdByInfo(FILE *fp, struct tm *timeInfo);
int sdl_c_fileInfo(FILE *fp, struct tm *timeInfo, char *fullFilePath);
int sdl_c_comment(
		FILE *fp,
		char *comment,
		_Bool lineComment,
		_Bool startComment,
		_Bool endComment);
int sdl_c_module(FILE *fp, char *moduleName, char *identName);
int sdl_c_module_end(FILE *fp, char *moduleName);
int sdl_c_item(FILE *fp, SDL_ITEM *item, SDL_CONTEXT *context);

#endif	/* _OPENSDL_LANH_H_ */
