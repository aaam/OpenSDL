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
 *  This source file contains the routines to generate a listing file as the
 *  input file is parsed.  There are 4 interface functions:
 *
 *	1) sdl_open_list	- open a new listing file for write
 *	2) sdl_write_list	- write a string of input data to listing file
 *	3) sdl_write_err	- write an error message to the listing file
 *	4) sdl_close_list	- close the listing file
 *
 * Revision History:
 *
 *  V01.000	25-AUG-2018	Jonathan D. Belanger
 *  Initially written.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "opensdl_defs.h"
#include "opensdl_message.h"
#include "opensdl_main.h"

/*
 * Local variables used while generating a listing file.
 */
#define SDL_PAGE_WIDTH	132
#define SDL_PAGE_LENGTH	66
typedef char		SDL_HEADER_DEF[SDL_PAGE_WIDTH + 1];
static SDL_HEADER_DEF	sdl_listing_header[2];
static char		xBuf[SDL_PAGE_WIDTH + 1];
static int		xBufLoc = 0;
static uint32_t		listLine = 1;
static uint32_t		pageLine = 1;
static uint32_t		pageNo = 1;

/*
 * sdl_open_listing
 *  This function is called to open the listing file for write, and initialize
 *  the listing header information (which is placed at the top of each page).
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context block where all the information about the
 *	processing of the input file is maintained, as well as the listing
 *	output file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  NULL:	An error occurred opening the listing file for write.
 *  !NULL:	A pointer to the listing file.
 */
FILE *sdl_open_listing(SDL_CONTEXT *context)
{
    FILE	*retVal;

    retVal = fopen(context->listingFileName, "w");
    if (retVal != NULL)
    {
	context->listingFP = retVal;
	sprintf(sdl_listing_header[0],
		"%*s%02d-%s-%04d %02d:%02d:%02d OpenSDL %c%d.%d-%d\t\t\tPage ",
		58,
		"",
		context->inputTimeInfo->tm_mday,
		sdl_months[context->inputTimeInfo->tm_mon],
		(1900 + context->inputTimeInfo->tm_year),
		context->inputTimeInfo->tm_hour,
		context->inputTimeInfo->tm_min,
		context->inputTimeInfo->tm_sec,
		SDL_K_VERSION_TYPE,
		SDL_K_VERSION_MAJOR,
		SDL_K_VERSION_MINOR,
		SDL_K_VERSION_LEVEL);
	sprintf(sdl_listing_header[1],
		"%*s %02d-%s-%04d %02d:%02d:%02d\t%s",
		60,
		"",
		context->runTimeInfo->tm_mday,
		sdl_months[context->runTimeInfo->tm_mon],
		(1900 + context->runTimeInfo->tm_year),
		context->runTimeInfo->tm_hour,
		context->runTimeInfo->tm_min,
		context->runTimeInfo->tm_sec,
		context->inputPath);
    }

    /*
     * Return the results of this call back to the caller.
     */
    return(retVal);
}

/*
 * sdl_write_list
 *  This function is called by the Flex lexical analyzer any time a rule is
 *  about to be called.  This allows the read in code to be written out to the
 *  listing file.  Note, we may receive multiple lines to be written out to
 *  the listing file, or not a complete line.  We use the newline character
 *  to determine this, ignore the carriage-return character, and use the
 *  form-feed character to move to the next page.
 *
 * Input Parameters:
 *  fp:
 *	The address of the file pointer associated with the listing file.
 *  buf:
 *	A pointer to the buffer of characters to be written out to the listing
 *	file.
 *  len:
 *	A value indicating the size of the buf parameter.  When this is set to
 *	0, then get it from the strlen call.  Otherwise, we have a buffer that
 *	may not be null terminated, so don't assume it is.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  None.
 */
void sdl_write_list(FILE *fp, char *buf, size_t len)
{
    int		ii;
    size_t	myLen = (len == 0) ? strlen(buf) : len;

    /*
     * If we are on the first line of a page, then display the listing headers.
     */
    if (pageLine == 1)
    {
	if (listLine > 1)
	    fprintf(fp, "\f");
	fprintf(fp, "%s%4d\n", sdl_listing_header[0], pageNo);
	pageLine++;
	fprintf(fp, "%s\n", sdl_listing_header[1]);
	pageLine++;
    }

    /*
     * Scan through the buffer, outputting lines as we go.  NOTE: We may end up
     * with a partial output line.  If this is the case, then we'll be called
     * again with more to output.
     */
    for (ii = 0; ii < myLen; ii++)
    {

	/*
	 * If we are starting a new line, then insert the line number.
	 */
	if (xBufLoc == 0)
	    xBufLoc = sprintf(xBuf, "%7d ", listLine);

	/*
	 * If the character is a carriage-return, then just ignore it.
	 */
	if (buf[ii] == '\r')
	    continue;

	/*
	 * If the character is a new-line, then null-terminate the output
	 * buffer and output the line.
	 */
	if (buf[ii] == '\n')
	{
	    xBuf[xBufLoc] = '\0';
	    fprintf(fp, "%s\n", xBuf);
	    listLine++;
	    pageLine++;
	    xBufLoc = 0;
	    continue;
	}

	/*
	 * If the character is a form-feed, or we have exceeded the number of
	 * lines in a page, then insert a form-feed and reset the pageLine.
	 */
	if ((buf[ii] == '\f') || (pageLine > SDL_PAGE_LENGTH))
	{
	    xBuf[xBufLoc] = '\0';
	    fprintf(fp, "%s\f", xBuf);
	    xBuf[xBufLoc++] = ' ';
	    pageLine = 1;
	    continue;
	}

	/*
	 * We have just a regular character.  If there is room, then add it to
	 * the output buffer.  Otherwise, just swallow it.
	 */
	if (xBufLoc < SDL_PAGE_WIDTH)
	    xBuf[xBufLoc++] = buf[ii];
    }

    /*
     * Return back to the caller.
     */
    return;
}

/*
 * sdl_write_err
 *  This function is called by the Bison parser any time an error is returned.
 *  This allows errors to be emitted to the output file immediately after the
 *  line of code that generated it.  Note:, if there is a partial line waiting
 *  to be written, then the error text is held until the new-line or form-feed
 *  is detected.  This also means that it is possible to get more than one
 *  error for any particular line of code.
 *
 * Input Parameters:
 *  fp:
 *	The address of the file pointer associated with the listing file.
 *  msgVector:
 *	A pointer to the message vector containing all the information needed
 *	to display the error text.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  None.
 */
void sdl_write_err(FILE *fp, SDL_MSG_VECTOR *msgVector)
{

    /*
     * Return back to the caller.
     */
    return;
}

/*
 * sdl_close_listing
 *  This function is called to close the listing file.  If there is any output
 *  that has not yet been written out, then do so before closing the file.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context block where all the information about the
 *	processing of the input file is maintained, as well as the listing
 *	output file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  None.
 */
void sdl_close_listing(SDL_CONTEXT *context)
{

    /*
     * If there is anything in the output buffer, write it out now.
     */
    if (xBufLoc > 0)
    {
	xBuf[xBufLoc] = '\0';
	fprintf(context->listingFP, "%s\n", xBuf);
    }

    /*
     * All that is left to do is close the file, if it was opened.
     */
    if (context->listingFP != NULL)
    {
	fclose(context->listingFP);
	context->listingFP = NULL;
    }

    /*
     * Return back to the caller.
     */
    return;
}