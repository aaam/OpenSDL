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
 *  This source file contains the action routines called during the parsing of
 *  the input file..
 *
 * Revision History:
 *
 *  V01.000	25-AUG-2018	Jonathan D. Belanger
 *  Initially written.
 *
 *  V01.001	08-SEP-2018	Jonathan D. Belanger
 *  Updated the copyright to be GNUGPL V3 compliant.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "opensdl_defs.h"
#include "opensdl_lang.h"
#include "opensdl_utility.h"

extern _Bool trace;

/*
 * Define the language specific entry-points.
 */
#define SDL_K_COMMENT_CB	0
#define SDL_K_MODULE_CB		1
#define SDL_K_END_MODULE_CB	2
#define SDL_K_ITEM_CB		3
#define SDL_K_CONSTANT_CB	4

static SDL_LANG_FUNC _outputFuncs[SDL_K_LANG_MAX] =
{

    /*
     * For the C/C++ languages.
     */
    {
	(SDL_FUNC) &sdl_c_comment,
	(SDL_FUNC) &sdl_c_module,
	(SDL_FUNC) &sdl_c_module_end,
	(SDL_FUNC) &sdl_c_item,
	(SDL_FUNC) &sdl_c_constant
    }
};

/*
 * The following defines the default tags for the various data types.
 */
static char *_defaultTag[] =
{
    "K",	/* CONSTANT */
    "B",	/* BYTE [UNSIGNED] */
    "W",	/* WORD [UNSIGNED] */
    "L",	/* LONGWORD [UNSIGNED] */
    "Q",	/* QUADWORD [UNSIGNED] */
    "O",	/* OCTAWORD [UNSIGNED] */
    "F",	/* TFLOAT */
    "D",	/* SFLOAT */
    "P",	/* DECIMAL */
    "V",	/* BITFIELD 'S' and 'M' for size and mask, respectively */
    "T",	/* CHARACTER */
    "A",	/* ADDRESS/POINTER */
    "A",	/* POINTER_LONG */
    "A",	/* POINTER_QUAD */
    "A",	/* POINTER_HW/ADDRESS_HARDWARE */
    "",		/* ANY */
    "C",	/* BOOLEAN (Conditional) */
    "R",	/* STRUCTURE */
    "R"		/* UNION */
};

/*
 * Local Prototypes (found at the end of this module).
 */
static SDL_DECLARE *_sdl_get_declare(SDL_DECLARE_LIST *declare, char *name);
static SDL_ITEM *_sdl_get_item(SDL_ITEM_LIST *item, char *name);
static SDL_AGGREGATE *_sdl_get_aggregate(SDL_AGGREGATE_LIST *aggregate, char *name);
static char *_sdl_get_tag(
        SDL_CONTEXT *context,
        char *tag,
        int datatype,
        bool lower);
static __int64_t _sdl_sizeof(SDL_CONTEXT *context, int item);
static SDL_CONSTANT *_sdl_create_constant(
        char *id,
        char *prefix,
        char *tag,
        char *comment,
        char *typeName,
        int radix,
        __int64_t value,
        char *string);
static int _sdl_queue_constant(SDL_CONTEXT *context, SDL_CONSTANT *myConst);
static bool _sdl_all_lower(const char *str);
static void _sdl_reset_options(SDL_CONTEXT *context);
static int _sdl_iterate_members(
		SDL_CONTEXT *context,
		SDL_MEMBERS *member,
		void *qhead,
		int (*callback)(),
		int count);

/************************************************************************/
/* Functions called to create definitions from the Grammar file		*/
/************************************************************************/

/*
 * sdl_comment_line
 *  This function is called to output a line comment to the output file.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current parsing.
 *  comment:
 *  	A pointer to the comment string to be output.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 */
int sdl_comment_line(SDL_CONTEXT *context, char *comment)
{
    int	retVal = 1;
    int ii;

    /*
     * Trim all trailing space characters.
     */
    sdl_trim_str(comment, SDL_M_TRAIL);

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:sdl_comment_line\n", __FILE__, __LINE__);

    /*
     * Loop through all the possible languages and call the appropriate output
     * function for each of the enabled languages.
     */
    for (ii = 0; ((ii < SDL_K_LANG_MAX) && (retVal == 1)); ii++)
	if ((context->langSpec[ii] == true) && (context->langEna[ii] == true))
	    retVal = (*_outputFuncs[ii][SDL_K_COMMENT_CB])(
				context->outFP[ii],
				&comment[2],	/* Skip past comment token */
				true,
				false,
				false,
				false);

    /*
     * Return the results of this call back to the caller.
     */
    free(comment);
    return(retVal);
}

/*
 * sdl_comment_block
 *  This function is called to output a block comment to the output file.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current parsing.
 *  comment:
 *  	A pointer to the comment string to be output.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 */
int sdl_comment_block(SDL_CONTEXT *context, char *comment)
{
    char	*ptr, *nl;
    int		retVal = 1;
    int 	ii;
    bool	start_comment, start_done = false;
    bool	middle_comment;
    bool	end_comment;

    /*
     * Trim all trailing space characters.
     */
    sdl_trim_str(comment, SDL_M_TRAIL);

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:sdl_comment_block\n", __FILE__, __LINE__);

    /*
     * Loop through each line of the comment until we reach the end of the
     * comment string.
     */
    ptr = comment;
    while (*ptr != '\0')
    {
	start_comment = false;
	middle_comment = false;
	end_comment = false;
	nl = strchr(ptr, '\n');
	if (nl != NULL)
	{
	    *nl = '\0';
	    nl--;
	    if (*nl == '\r')
		*nl = '\0';
	    nl += 2;
	}
	else
	    nl = &ptr[strlen(ptr)];
	if (ptr[0] == '/')
	{
	    if ((ptr[1] == '+') && (start_done == false))
	    {
		ptr += 2;
		start_comment = true;
		start_done = true;
	    }
	    else if (ptr[1] == '/')
	    {
		ptr += 2;
		middle_comment = true;
	    }
	    else if (ptr[1] == '-')
	    {
		ptr += 2;
		end_comment = true;
	    }
	}
	if (strstr(ptr, "/-") != NULL)
	{
	    size_t	len = strlen(ptr);

	    end_comment = ((ptr[len - 2] == '/') && (ptr[len - 1] == '-'));
	    if (end_comment == true)
		ptr[len - 2] = '\0';
	}

	/*
	 * Loop through all the possible languages and call the appropriate
	 * output function for each of the enabled languages.
	 */
	for (ii = 0; ((ii < SDL_K_LANG_MAX) && (retVal == 1)); ii++)
	    if ((context->langSpec[ii] == true) && (context->langEna[ii] == true))
		retVal = (*_outputFuncs[ii][SDL_K_COMMENT_CB])(
					context->outFP[ii],
					ptr,
					false,
					start_comment,
					middle_comment,
					end_comment);

	/*
	 * Move to the next line.
	 */
	ptr = nl;
    }

    /*
     * Return the results of this call back to the caller.
     */
    free(comment);
    return(retVal);
}

/*
 * sdl_set_local
 *  This function is called to set the value of a local variable.  If the local
 *  variable has not yet been declared, then a new one will be allocated.
 *  Otherwise, the current value will be changed.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the local variables.
 *  name:
 *	A pointer to the name of the local variable.
 *  value:
 *	A 64-bit integer value the local variable it to be set.
 *
 * Output Parameters:
 *  None.
 *
 * Return Value
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 *  -1:	Normal Successful Completion, local variable created.
 */
int sdl_set_local(SDL_CONTEXT *context, char *name, __int64_t value)
{
    SDL_LOCAL_VARIABLE	*local = sdl_find_local(context, name);
    int			retVal = 1;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
	printf("%s:%d:sdl_set_local(", __FILE__, __LINE__);
	printf(
	    "%s, %ld (%016lx - %4.4s)\n",
	    name,
	    value,
	    value,
	    (char *) &value);
    }

    /*
     * OK, if we did not find a local variable with the same name, then we need
     * to go get one.
     */
    if (local == NULL)
    {
	local = calloc(1, sizeof(SDL_LOCAL_VARIABLE));
	if (local != NULL)
	{
	    local->id = name;
	    SDL_INSQUE(&context->locals, &local->header);
	    retVal = -1;
	}
	else
	{
	    free(name);
	    retVal = 0;
	}
    }

    /*
     * If we still have a success, then set the value.
     */
    local->value = value;

    /*
     * Return the results of this call back to the caller.
     */
    return(retVal);
}

/*
 * sdl_module
 *  This function is called when it gets to the MODULE keyword.  It starts the
 *  definitions.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current parsing.
 *  moduleName:
 *  	A pointer to the MODULE module-name string to be output.
 *  identString:
 *  	A pointer to the IDENT "ident-string" string to be output.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 */
int sdl_module(SDL_CONTEXT *context, char *moduleName, char *identName)
{
    int	retVal = 1;
    int ii;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:sdl_module\n", __FILE__, __LINE__);

    /*
     * Save the MODULE's module-name
     */
    context->module = moduleName;

    /*
     * Save the IDENT's indent-name.
     */
    context->ident = identName;

    /*
     * Loop through all the possible languages and call the appropriate output
     * function for each of the enabled languages.
     */
    for (ii = 0; ((ii < SDL_K_LANG_MAX) && (retVal == 1)); ii++)
	if ((context->langSpec[ii] == true) && (context->langEna[ii] == true))
	    retVal = (*_outputFuncs[ii][SDL_K_MODULE_CB])(
				context->outFP[ii],
				context);

    /*
     * Return the results of this call back to the caller.
     */
    return(retVal);
}

/*
 * sdl_module_end
 *  This function is called when it gets to the END_MODULE keyword.  It end the
 *  definitions.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current parsing.
 *  moduleName:
 *  	A pointer to the MODULE module-name string.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 */
int sdl_module_end(SDL_CONTEXT *context, char *moduleName)
{
    int			retVal = 1;
    int			ii;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:sdl_module_end\n", __FILE__, __LINE__);

    /*
     * OK, time to write out the OpenSDL Parser's MODULE footer.
     *
     * Loop through all the possible languages and call the appropriate output
     * function for each of the enabled languages.
     */
    for (ii = 0; ((ii < SDL_K_LANG_MAX) && (retVal == 1)); ii++)
	if ((context->langSpec[ii] == true) && (context->langEna[ii] == true))
	    retVal = (*_outputFuncs[ii][SDL_K_END_MODULE_CB])(
				context->outFP[ii],
				context);

    /*
     * Reset all the dimension entries.
     */
    for (ii = 0; ii < SDL_K_MAX_DIMENSIONS; ii++)
	context->dimensions[ii].inUse = 0;

    /*
     * Clean out all the local variables.
     */
    ii = 1;
    while(SDL_Q_EMPTY(&context->locals) == false)
    {
	SDL_LOCAL_VARIABLE *local;

	SDL_REMQUE(&context->locals, local);
	if (trace == true)
	{
	    if (ii == 1)
		printf("    Local Variables:\n");
	    printf(
	        "\t%2d: name: %s\n\t    value: %ld\n",
		ii++,
		local->id,
		local->value);
	}
	free(local->id);
	free(local);
    }

    /*
     * Clean out all the constant definitions.
     */
    ii = 1;
    while(SDL_Q_EMPTY(&context->constants) == false)
    {
	SDL_CONSTANT *constant;

	SDL_REMQUE(&context->constants, constant);
	if (trace == true)
	{
	    if (ii == 1)
		printf("    CONSTANTs:\n");
	    printf(
	        "\t%2d: name: %s\n\t    prefix: %s\n\t    tag: %s\n"
		"\t    typeName: %s\n\t    type: %s\n",
		ii++,
		constant->id,
		(constant->prefix == NULL ? "" : constant->prefix),
		(constant->tag == NULL ? "" : constant->tag),
		(constant->typeName == NULL ? "" : constant->typeName),
		(constant->type == SDL_K_CONST_STR ? "String" : "Number"));
	    if (constant->type == SDL_K_CONST_STR)
		printf("\t    value: %s\n", constant->string);
	    else
	    {
		printf(
		    "\t    value: %ld (%s)\n",
		    constant->value,
		    (constant->radix <= SDL_K_RADIX_DEC ? "Decimal" :
			(constant->radix == SDL_K_RADIX_OCT ? "Octal" :
				(constant->radix == SDL_K_RADIX_HEX ?
					"Hexidecimal" : "Invalid"))));
	    }
	    if (constant->comment != NULL)
		printf(
		    "\t    comment: %s\n",
		    constant->comment);
	}
	free(constant->id);
	if (constant->prefix != NULL)
	    free(constant->prefix);
	if (constant->tag != NULL)
	    free(constant->tag);
	if (constant->comment != NULL)
	    free(constant->comment);
	if (constant->typeName != NULL)
	    free(constant->typeName);
	free(constant);
    }

    /*
     * Clean out all the declares.
     */
    ii = 1;
    while (SDL_Q_EMPTY(&context->declares.header) == false)
    {
	SDL_DECLARE *declare;

	SDL_REMQUE(&context->declares.header, declare);
	if (trace == true)
	{
	    if (ii == 1)
		printf("    DECLAREs:\n");
	    printf(
	        "\t%2d: name: %s\n\t    prefix: %s\n\t    tag: %s\n"
		"\t    typeID: %d\n\t    type: %d\n\t    size: %ld\n",
	        ii++,
	        declare->id,
	        (declare->prefix != NULL ? declare->prefix : ""),
	        (declare->tag != NULL ? declare->tag : ""),
	        declare->typeID,
	        declare->type,
	        declare->size);
	}
	free(declare->id);
	if (declare->prefix != NULL)
	    free(declare->prefix);
	if (declare->tag != NULL)
	    free(declare->tag);
	free(declare);
    }

    /*
     * Clean out all the items.
     */
    ii = 1;
    while (SDL_Q_EMPTY(&context->items.header) == false)
    {
	SDL_ITEM *item;

	SDL_REMQUE(&context->items.header, item);
	if (trace == true)
	{
	    if (ii == 1)
		printf("    ITEMs:\n");
	    printf(
	        "\t%2d: name: %s\n\t    prefix: %s\n\t    tag: %s\n"
		"\t    typeID: %d\n\t    alignment: %d\n\t    type: %d\n"
		"\t    size: %ld\n\t    memSize: %ld\n\t    commonDef: %s\n"
		"\t    globalDef: %s\n\t    typeDef: %s\n",
	        ii++,
	        item->id,
	        (item->prefix != NULL ? item->prefix : ""),
	        (item->tag != NULL ? item->tag : ""),
	        item->typeID,
	        item->alignment,
	        item->type,
	        item->size,
	        item->memSize,
	        (item->commonDef == true ? "True" : "False"),
	        (item->globalDef == true ? "True" : "False"),
	        (item->typeDef == true ? "True" : "False"));
	    if (item->dimension == true)
		printf(
		    "\t    dimension: [%ld:%ld]\n",
		    item->lbound,
		    item->hbound);
	}
	free(item->id);
	if (item->prefix != NULL)
	    free(item->prefix);
	if (item->tag != NULL)
	    free(item->tag);
	if (item->comment != NULL)
	    free(item->comment);
	free(item);
    }

    /*
     * Clean out all the aggregates.
     */
    ii = 1;
    while (SDL_Q_EMPTY(&context->aggregates.header) == false)
    {
	SDL_AGGREGATE *aggregate;

	SDL_REMQUE(&context->aggregates.header, aggregate);
	if (trace == true)
	{
	    if (ii == 1)
		printf("    AGGREGATEs:\n");
	    printf(
	        "\t%2d: name: %s\n\t    prefix: %s\n\t    tag: %s\n"
		"\t    marker: %s\n\t    arrgType: %s\n\t    typeID: %d\n"
		"\t    alignment: %d\n\t    type: %d\n\t    _unsigned: %s\n"
		"\t    size: %ld\n\t    memSize: %ld\n\t    commonDef: %s\n"
		"\t    globalDef: %s\n\t    typeDef: %s\n\t    fill: %s\n"
		"\t    basedPtrName: %s\n\t    currentBitOffset: %d\n",
	        ii++,
	        aggregate->id,
	        (aggregate->prefix != NULL ? aggregate->prefix : ""),
	        (aggregate->tag != NULL ? aggregate->tag : ""),
	        (aggregate->marker != NULL ? aggregate->marker : ""),
	        (aggregate->structUnion == Structure ? "STRUCTURE" : "UNION"),
	        aggregate->typeID,
	        aggregate->alignment,
	        aggregate->type,
	        (aggregate->_unsigned == true ? "True" : "False"),
	        aggregate->size,
	        aggregate->memSize,
	        (aggregate->commonDef == true ? "True" : "False"),
	        (aggregate->globalDef == true ? "True" : "False"),
	        (aggregate->typeDef == true ? "True" : "False"),
	        (aggregate->fill == true ? "True" : "False"),
	        (aggregate->basedPtrName!= NULL ? aggregate->basedPtrName: ""),
	        aggregate->currentBitOffset);
	    if (aggregate->originPresent == true)
		printf("\t    origin: %s\n", aggregate->origin.id);
	    if (aggregate->dimension == true)
		printf(
		    "\t    dimension[%ld:%ld]\n",
		    aggregate->lbound,
		    aggregate->hbound);
	}
	if (SDL_Q_EMPTY(&aggregate->members) == false)
	    _sdl_iterate_members(
			context,
			aggregate->members.flink,
			&aggregate->members,
			NULL,
			1);

	free(aggregate->id);
	if (aggregate->prefix != NULL)
	    free(aggregate->prefix);
	if (aggregate->tag != NULL)
	    free(aggregate->tag);
	if (aggregate->marker != NULL)
	    free(aggregate->marker);
	if (aggregate->comment != NULL)
	    free(aggregate->comment);
	if (aggregate->basedPtrName != NULL)
	    free(aggregate->basedPtrName);
	if (aggregate->origin.id != NULL)
	    free(aggregate->origin.id);
	free(aggregate);
    }

    /*
     * Clean out all the entries.
     */
    ii = 1;
    while (SDL_Q_EMPTY(&context->entries) == false)
    {
	SDL_ENTRY *entry;

	SDL_REMQUE(&context->entries, entry);
	free(entry->id);
	if (entry->comment != NULL)
	    free(entry->comment);
	if (entry->alias != NULL)
	    free(entry->alias);
	if (entry->linkage != NULL)
	    free(entry->linkage);
	if (entry->typeName != NULL)
	    free(entry->typeName);
	free(entry);
    }

    /*
     * Reset the module-name and ident-string to zero length.
     */
    free(context->module);
    context->module = NULL;
    if (context->ident != NULL)
	free(context->ident);
    context->ident = NULL;

    /*
     * Return the results of this call back to the caller.
     */
    return(retVal);
}

/*
 * sdl_literal
 *  This function is called when it gets a line of text within a LITERAL...
 *  END_LITERAL pair.  When the END_LITERAL is reached all the lines will be
 *  dumped out to the file as necessary.
 *
 * Input Parameters:
 *  literals:
 *	A pointer to the queue containing the current set of literal lines.
 *  line:
 *  	A pointer to the line of text from the input file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 */
int sdl_literal(SDL_QUEUE *literals, char *line)
{
    SDL_LITERAL	*literalLine;
    int		retVal = 1;
    size_t	len = strlen(line);

    /*
     * Before we go too far, strip ending control characters.
     */
    while (((line[len-1] == '\n') ||
	    (line[len-1] == '\f') ||
	    (line[len-1] == '\r')) &&
	   (len > 0))
	line[--len] = '\0';

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:sdl_literal(%.*s)\n", __FILE__, __LINE__, (int) len, line);

    literalLine = (SDL_LITERAL *) calloc(1, sizeof(SDL_LITERAL));
    if (literalLine != NULL)
    {
	literalLine->line = line;
	SDL_INSQUE(literals, &literalLine->header);
    }
    else
    {
	retVal = 0;
	free(line);
    }

    /*
     * Return the results of this call back to the caller.
     */
    return(retVal);
}

/*
 * sdl_literal_end
 *  This function is called when it gets an END_LITERAL.  All the lines saved
 *  since the LITERAL statement will be dumped out to the file(s).
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current parsing.
 *  literals:
 *	A pointer to the queue containing the current set of literal lines.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 */
int sdl_literal_end(SDL_CONTEXT *context, SDL_QUEUE *literals)
{
    SDL_LITERAL	*literalLine;
    int		retVal = 1;
    int		ii;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:sdl_literal_end\n", __FILE__, __LINE__);

    /*
     * Keep pulling off literal lines until there are no more.
     */
    while ((SDL_Q_EMPTY(literals) == false) && (retVal == 1))
    {
	SDL_REMQUE(literals, literalLine);

	/*
	 * Loop through each of the supported languages and if we are
	 * generating a file for the language, write out the line to it.
	 */
	for (ii = 0; ((ii < SDL_K_LANG_MAX) && (retVal == 1)); ii++)
	    if ((context->langSpec[ii] == true) && (context->langEna[ii] == true))
	    {
		if (fprintf(context->outFP[ii], "%s\n", literalLine->line) < 0)
		    retVal = 0;
	    }

	/*
	 * Free up all the memory we allocated for this literal line.
	 */
	free(literalLine->line);
	free(literalLine);
    }

    /*
     * Return the results of this call back to the caller.
     */
    return(retVal);
}

/*
 * sdl_declare
 *  This function is called to start the creation of a DECLARE record, if one
 *  does not already exist.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current parsing.
 *  name:
 *	A pointer to a string containing the name of the type.
 *  sizeType:
 *	A value indicating the size or datatype of the declaration.
 *
 * Output Parameters:
 *  None.
 *
 * Return Value:
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 */
int sdl_declare(SDL_CONTEXT *context, char *name, int sizeType)
{
    SDL_DECLARE	*myDeclare = _sdl_get_declare(&context->declares, name);
    int		retVal = 1;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:sdl_declare\n", __FILE__, __LINE__);

    /*
     * We only create, never update.
     */
    if (myDeclare == NULL)
    {
	myDeclare = (SDL_DECLARE *) calloc(1, sizeof(SDL_DECLARE));

	if (myDeclare != NULL)
	{
	    myDeclare->id = name;
	    myDeclare->typeID = context->declares.nextID++;
	    if (sizeType < 0)
	    {
		myDeclare->size = -sizeType;
		myDeclare->type = SDL_K_TYPE_CHAR;
	    }
	    else
	    {
		myDeclare->size = _sdl_sizeof(context, sizeType);
		myDeclare->type = sizeType;
	    }
	    SDL_INSQUE(&context->declares.header, &myDeclare->header);
	}
	else
	{
	    retVal = 0;
	    free(name);
	}
    }

    /*
     * Return the results of this call back to the caller.
     */
    return(retVal);
}

/*
 * sdl_declare_compl
 *  This function is called to finish creating a DECLARE record.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current parsing.
 *
 * Output Parameters:
 *  None.
 *
 * Return Value:
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 */
int sdl_declare_compl(SDL_CONTEXT *context)
{
    SDL_DECLARE	*myDeclare = (SDL_DECLARE *) context->declares.header.blink;
    char 	*prefix = NULL;
    char 	*tag = NULL;
    int		ii, retVal = 1;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:sdl_declare_compl\n", __FILE__, __LINE__);

    /*
     * Go find our options
     */
    for (ii = 0; ii < context->optionsIdx; ii++)
	if (context->options[ii].option == Prefix)
	{
	    prefix = context->options[ii].string;
	    context->options[ii].string = NULL;
	}
	else if (context->options[ii].option == Tag)
	{
	    tag = context->options[ii].string;
	    context->options[ii].string = NULL;
	}

    /*
     * We only finish creating, never starting new.
     */
    if (myDeclare != NULL)
    {
	myDeclare->prefix = prefix;
	myDeclare->tag = _sdl_get_tag(
		    context,
		    tag,
		    myDeclare->type,
		    _sdl_all_lower(myDeclare->id));
    }

    /*
     * Return the results of this call back to the caller.
     */
    _sdl_reset_options(context);
    return(retVal);
}

/*
 * sdl_item
 *  This function is called to start the creation of an ITEM, save it into the
 *  context.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current parsing.
 *  name:
 *	A pointer the the name of the item to be defined.
 *  datatype:
 *	A value to be associated with the datatype for this item.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 */
int sdl_item(SDL_CONTEXT *context, char *name, int datatype)
{
    SDL_ITEM	*myItem = _sdl_get_item(&context->items, name);
    int		retVal = 1;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:sdl_item\n", __FILE__, __LINE__);

    /*
     * We only complete, never create.
     */
    if (myItem == NULL)
    {
	myItem = (SDL_ITEM *) calloc(1, sizeof(SDL_ITEM));

	if (myItem != NULL)
	{
	    myItem->id = name;
	    myItem->typeID = context->items.nextID++;
	    if (datatype < 0)
	    {
		myItem->_unsigned = false;
		datatype = -datatype;
	    }
	    else
		myItem->_unsigned = true;
	    myItem->type = datatype;
	    if (datatype == SDL_K_TYPE_DECIMAL)
	    {
		myItem->precision = context->precision;
		myItem->scale = context->scale;
	    }
	    myItem->size = _sdl_sizeof(context, datatype);
	    SDL_INSQUE(&context->items.header, &myItem->header);
	}
    }
    else
    {
	retVal = 0;
	free(name);
    }

    /*
     * Return the results of this call back to the caller.
     */
    return(retVal);
}

/*
 * sdl_item_compl
 *  This function is called to finish the creation of an ITEM, and write out
 *  the item to the output file.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current parsing.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 */
int sdl_item_compl(SDL_CONTEXT *context)
{
    SDL_ITEM	*myItem = context->items.header.blink;
    char	*prefix = NULL;
    char	*tag = NULL;
    int		ii, retVal = 1;
    int		storage = 0;
    int		basealign = 0;
    int		dimension;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:sdl_item_compl\n", __FILE__, __LINE__);

    /*
     * Go find our options
     */
    for (ii = 0; ii < context->optionsIdx; ii++)
	if (context->options[ii].option == Prefix)
	{
	    prefix = context->options[ii].string;
	    context->options[ii].string = NULL;
	}
	else if (context->options[ii].option == Tag)
	{
	    tag = context->options[ii].string;
	    context->options[ii].string = NULL;
	}
	else if (context->options[ii].option == BaseAlign)
	    basealign= context->options[ii].value;
	else if (context->options[ii].option == Dimension)
	{
	    dimension = context->options[ii].value;
	    myItem->dimension = true;
	}
	else if (context->options[ii].option == Common)
	    storage |= SDL_M_STOR_COMM;
	else if (context->options[ii].option == Global)
	    storage |= SDL_M_STOR_GLOB;
	else if (context->options[ii].option == Typedef)
	    storage |= SDL_M_STOR_TYPED;

    /*
     * We only update, never create.
     */
    if (myItem != NULL)
    {
	myItem->commonDef = (storage & SDL_M_STOR_COMM) == SDL_M_STOR_COMM;
	myItem->globalDef = (storage & SDL_M_STOR_GLOB) == SDL_M_STOR_GLOB;
	myItem->typeDef = (storage & SDL_M_STOR_TYPED) == SDL_M_STOR_TYPED;
	myItem->alignment = basealign;
	if (myItem->dimension == true)
	{
	    myItem->lbound = context->dimensions[dimension].lbound;
	    myItem->hbound = context->dimensions[dimension].hbound;
	    context->dimensions[dimension].inUse = false;
	}
	myItem->prefix = prefix;
	myItem->tag = _sdl_get_tag(
		    context,
		    tag,
		    myItem->type,
		    _sdl_all_lower(myItem->id));

	/*
	 * Loop through all the possible languages and call the appropriate
	 * output function for each of the enabled languages.
	 */
	for (ii = 0; ((ii < SDL_K_LANG_MAX) && (retVal == 1)); ii++)
	    if ((context->langSpec[ii] == true) && (context->langEna[ii] == true))
		retVal = (*_outputFuncs[ii][SDL_K_ITEM_CB])(
					context->outFP[ii],
					myItem,
					context);
    }

    /*
     * Return the results of this call back to the caller.
     */
    _sdl_reset_options(context);
    return(retVal);
}

/*
 * sdl_constant
 *  This function is called to start the definition of one or more constant
 *  values.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current parsing.
 *  id:
 *	A pointer the the name/names of the constants to be defined.
 *  value:
 *	A value to be associated with the constant.
 *  valueStr:
 *	A pointer to a string to be associated with the constant.  If this is
 *	NULL, then value is used.  Otherwise, this is used.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 */
int sdl_constant(
		SDL_CONTEXT *context,
		char *id,
		__int64_t value,
		char *valueStr)
{
    int		retVal = 1;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:sdl_constant\n", __FILE__, __LINE__);

    /*
     * Set up the information needed when we get around to completing the
     * constant definition(s).
     */
    context->constDef.id = id;
    if (valueStr != NULL)
    {
	context->constDef.valueStr = valueStr;
	context->constDef.string = true;
    }
    else
    {
	context->constDef.value = value;
	context->constDef.string = false;
    }

    /*
     * Return the results of this call back to the caller.
     */
    return (retVal);
}

/*
 * sdl_constant_compl
 *  This function is called to complete the definition of one or more constant
 *  values.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current parsing.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 */
#define _SDL_OUTPUT_COMMENT	0
#define _SDL_COMMA_		2
#define _SDL_COMMENT_LIST_NULL	3
int sdl_constant_compl(SDL_CONTEXT *context)
{
    SDL_CONSTANT	*myConst;
    char 		*id = context->constDef.id;
    __int64_t		value = context->constDef.value;
    char		*valueStr = context->constDef.string ?
					context->constDef.valueStr :
					NULL;
    char		*comma = strchr(id, ',');
    static char		*commentList[] = {"/*", "{", ",", NULL};
    int			ii, retVal = 1;
    char		*prefix = NULL;
    char		*tag = NULL;
    char		*counter = NULL;
    char		*typeName = NULL;
    __int64_t		increment = 0;
    __int64_t		radix = SDL_K_RADIX_DEF;
    bool		incrementPresent = false;
    bool		localCreated = false;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:sdl_constant_compl\n", __FILE__, __LINE__);

    /*
     * Go find our options
     */
    for (ii = 0; ii < context->optionsIdx; ii++)
	if (context->options[ii].option == Prefix)
	{
	    prefix = context->options[ii].string;
	    context->options[ii].string = NULL;
	}
	else if (context->options[ii].option == Tag)
	{
	    tag = context->options[ii].string;
	    context->options[ii].string = NULL;
	}
	else if (context->options[ii].option == Counter)
	{
	    counter = context->options[ii].string;
	    context->options[ii].string = NULL;
	    localCreated = sdl_set_local(context, counter, value) < 0;
	}
	else if (context->options[ii].option == TypeName)
	{
	    typeName = context->options[ii].string;
	    context->options[ii].string = NULL;
	}
	else if (context->options[ii].option == Increment)
	{
	    increment = context->options[ii].value;
	    incrementPresent = true;
	}
	else if (context->options[ii].option == Radix)
	    radix = context->options[ii].value;

    /*
     * Before we go too far, we need to determine what kind of definition
     * we have.  First let's see if there is one or multiple names needing
     * to be created.
     */
    if (comma == NULL)
    {
	if (tag == NULL)
	    tag = _sdl_get_tag(
			context,
			NULL,
			SDL_K_TYPE_CONST,
			_sdl_all_lower(id));
	myConst = _sdl_create_constant(
				id,
				prefix,
				tag,
				NULL,
				typeName,
				radix,
				value,
				valueStr);
	if (myConst != NULL)
	    retVal = _sdl_queue_constant(context, myConst);
	else
	    retVal = 0;
    }
    else
    {
	char 		*ptr = id;
	char		*nl;
	__int64_t	prevValue = value;
	bool		freeTag = tag == NULL;
	bool		done = false;

	sdl_trim_str(ptr, SDL_M_LEAD);
	while ((done == false) && (retVal == 1))
	{
	    char	*name = ptr;
	    char	*comment;

	    ii = 0;
	    while (commentList[ii] != NULL)
	    {
		comment = strstr(name, commentList[ii]);
		if ((comment != NULL) || (ii == _SDL_COMMA_))
		{
		    if ((comment != NULL) && (*comment != ','))
		    {
			comment += strlen(commentList[ii]);
			nl = strchr(comment, '\n');
			if (nl != NULL)
			{
			    ptr = nl + 1;
			    if (ii == _SDL_OUTPUT_COMMENT)
				*nl = '\0';	/* Null-terminate comment */
			    else
				comment = NULL; /* Local comment, ignore */
			}
			else
			    ptr = strchr(comment, '\0');
		    }
		    else
			comment = NULL;
		    nl = name;
		    while (isalnum(*nl) || (*nl == '_') || (*nl == '$'))
			nl++;
		    if (ii == _SDL_COMMA_)
			ptr = (*nl == '\0') ? nl : (nl + 1);
		    *nl = '\0';		/* Null-terminate name */
		    ii = _SDL_COMMENT_LIST_NULL;
		}
		else
		    ii++;
	    }
	    if (strlen(name) > 0)
	    {
		if (freeTag == true)
		    tag = _sdl_get_tag(
				context,
				NULL,
				SDL_K_TYPE_CONST,
				_sdl_all_lower(id));
		myConst = _sdl_create_constant(
					name,
					prefix,
					tag,
					comment,
					typeName,
					radix,
					value,
					NULL);
		if (myConst != NULL)
		    retVal = _sdl_queue_constant(context, myConst);
		else
		    retVal = 0;
		if (freeTag == true)
		{
		    free(tag);
		    tag = NULL;
		}
	    }
	    if ((retVal == 1) && (counter != NULL) && (prevValue != value))
	    {
		retVal = sdl_set_local(context, counter, value);
		prevValue = value;
	    }
	    if (incrementPresent == true)
		value += increment;
	    sdl_trim_str(ptr, SDL_M_LEAD);
	    done = *ptr == '\0';
	}
    }

    /*
     * Deallocate all the memory.
     */
    free(id);
    if (prefix != NULL)
	free(prefix);
    if (tag != NULL)
	free(tag);
    if ((counter != NULL) && (localCreated == false))
	free(counter);
    if (typeName != NULL)
	free(typeName);

    /*
     * Return the results of this call back to the caller.
     */
    _sdl_reset_options(context);
    return (retVal);
}

/*
 * sdl_aggregate
 *  This function is called to create the AGGREGATE structure that will contain
 *  all the information about the aggregate being defined.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current parsing.
 *  name:
 *	A pointer to the string to be associated with this aggregate
 *	definition.
 *  datatype:
 *	A value to be associated with the datatype for this item.
 *  unionAggr:
 *	A boolean value indicating if this AGGREGATE is for a UNION or
 *	STRUCTURE definition.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 */
int sdl_aggregate(
	SDL_CONTEXT *context,
	char *name,
	__int64_t datatype,
	bool unionAggr)
{
    SDL_AGGREGATE	*myAggr = calloc(1, sizeof(SDL_AGGREGATE));
    int			retVal = 1;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:sdl_aggregate\n", __FILE__, __LINE__);

    if (myAggr != NULL)
    {
	myAggr->id = name;
	myAggr->typeID = context->aggregates.nextID++;
	if (datatype < 0)
	{
	    myAggr->_unsigned = false;
	    datatype = -datatype;
	}
	else if (datatype > 0)
	    myAggr->_unsigned = true;
	myAggr->type = datatype;
	myAggr->structUnion = unionAggr ? Union : Structure;
	SDL_Q_INIT(&myAggr->members);
	SDL_INSQUE(&context->aggregates.header, &myAggr->header);
	context->currentAggr = myAggr;
	context->aggregateDepth++;
    }
    else
	retVal = 0;

    /*
     * Return the results of this call back to the caller.
     */
    return (retVal);
}

/*
 * sdl_aggregate_member
 *  This function is called to define a member in an AGGREGATE definition.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current parsing.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 */
int sdl_aggregate_member(
		SDL_CONTEXT *context,
		char *name,
		__int64_t datatype,
		char *aggrId,
		SDL_AGGR_TYPE subaggrType)
{
    SDL_MEMBERS		*myMember = NULL;
    SDL_AGGREGATE	*myAggr = (context->aggregateDepth > 1 ?
					NULL :
					(SDL_AGGREGATE *) context->currentAggr);
    SDL_SUBAGGR		*mySubAggr = (context->aggregateDepth > 1 ?
					(SDL_SUBAGGR *) context->currentAggr :
					NULL);
    int			retVal = 1;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:sdl_aggregate_member\n", __FILE__, __LINE__);

    /*
     * Before we go too far, there may have been one or more options defined to
     * the previous member/aggregate/subaggregate.
     */
    if (context->optionsIdx > 0)
    {
	__int64_t	dimension;
	int		ii;

	/*
	 * Determine if the previous item we worked on was an ITEM, an
	 * AGGREGATE, or a subaggregate.  If ITEM, that is the one that needs
	 * to be updated with any saved options.  Otherwise, we already have
	 * the thing that needs to have these options applied to it.
	 */
	if (mySubAggr != NULL)
	{
	    if (SDL_Q_EMPTY(&mySubAggr->members) == false)
		myMember = (SDL_MEMBERS *) mySubAggr->members.blink;
	}
	else
	{
	    if (SDL_Q_EMPTY(&myAggr->members) == false)
		myMember = (SDL_MEMBERS *) myAggr->members.blink;
	}

	/*
	 * If the current member is not Unknown (and ITEM member), then it is
	 * the current aggregate, of which we already have the address.
	 */
	if ((myMember != NULL) && (myMember->type != Unknown))
	    myMember = NULL;

	/*
	 * Go find our options
	 */
	for (ii = 0; ii < context->optionsIdx; ii++)
	    switch (context->options[ii].option)
	    {

		/*
		 * Present only options.
		 */
		case Align:
		    if (myMember != NULL)
			myMember->item.alignment = SDL_K_ALIGN;
		    else if (mySubAggr != NULL)
			mySubAggr->alignment = SDL_K_ALIGN;
		    else
			myAggr->alignment = SDL_K_ALIGN;
		    break;

		case Common:
		    if ((myAggr != NULL) && (myMember == NULL))
			myAggr->commonDef = true;
		    break;

		case Fill:
		    if (myMember != NULL)
			myMember->item.fill = true;
		    else if (mySubAggr != NULL)
			mySubAggr->fill = true;
		    else
			myAggr->fill = true;
		    break;

		case Global:
		    if ((myAggr != NULL) && (myMember == NULL))
			myAggr->globalDef = true;
		    break;

		case Mask:
		    if ((myMember != NULL) &&
			(myMember->type == Unknown) &&
			(myMember->item.type == SDL_K_TYPE_BITFLD))
			myMember->item.mask = true;
		    break;

		case NoAlign:
		    if (myMember != NULL)
			myMember->item.alignment = SDL_K_NOALIGN;
		    else if (mySubAggr != NULL)
			mySubAggr->alignment = SDL_K_NOALIGN;
		    else
			myAggr->alignment = SDL_K_NOALIGN;
		    break;

		case Typedef:
		    if (myMember != NULL)
			myMember->item.typeDef = true;
		    else if (mySubAggr != NULL)
			mySubAggr->typeDef = true;
		    else
			myAggr->typeDef = true;
		    break;

		case Signed:
		    if ((myMember != NULL) &&
			(myMember->type == Unknown) &&
			(myMember->item.type == SDL_K_TYPE_BITFLD))
			myMember->item._signed = true;
		    break;

		/*
		 * String options.
		 */
		case Based:
		    if ((myAggr != NULL) && (myMember == NULL))
			myAggr->basedPtrName = context->options[ii].string;
		    else
			free(context->options[ii].string);
		    context->options[ii].string = NULL;
		    break;

		case Marker:
		    if ((mySubAggr != NULL) && (myMember == NULL))
			mySubAggr->marker = context->options[ii].string;
		    else if (myMember == NULL)
			myAggr->marker = context->options[ii].string;
		    context->options[ii].string = NULL;
		    break;

		case Origin:
		    if ((myAggr != NULL) && (myMember == NULL))
		    {
			myAggr->origin.id = context->options[ii].string;
			myAggr->originPresent = true;
		    }
		    else
			free(context->options[ii].string);
		    context->options[ii].string = NULL;
		    break;

		case Prefix:
		    if (myMember != NULL)
			myMember->item.prefix = context->options[ii].string;
		    else if (mySubAggr != NULL)
			mySubAggr->prefix = context->options[ii].string;
		    else
			myAggr->prefix = context->options[ii].string;
		    context->options[ii].string = NULL;
		    break;

		case Tag:
		    if (myMember != NULL)
			myMember->item.tag = context->options[ii].string;
		    else if (mySubAggr != NULL)
			mySubAggr->tag = context->options[ii].string;
		    else
			myAggr->tag = context->options[ii].string;
		    context->options[ii].string = NULL;
		    break;

		/*
		 * Numeric options.
		 */
		case BaseAlign:
		    if (myMember != NULL)
			myMember->item.alignment = context->options[ii].value;
		    else if (mySubAggr != NULL)
			mySubAggr->alignment = context->options[ii].value;
		    else
			myAggr->alignment = context->options[ii].value;
		    break;

		case Dimension:
		    dimension = context->options[ii].value;
		    if (myMember != NULL)
		    {
			myMember->item.lbound =
				context->dimensions[dimension].lbound;
			myMember->item.hbound =
				context->dimensions[dimension].hbound;
			myMember->item.dimension = true;
		    }
		    else if (mySubAggr != NULL)
		    {
			mySubAggr->lbound =
				context->dimensions[dimension].lbound;
			mySubAggr->hbound =
				context->dimensions[dimension].hbound;
			mySubAggr->dimension = true;
		    }
		    else
		    {
			myAggr->lbound = context->dimensions[dimension].lbound;
			myAggr->hbound = context->dimensions[dimension].hbound;
			myAggr->dimension = true;
		    }
		    context->dimensions[dimension].inUse = false;
		    break;

		case Length:
		    if ((myMember != NULL) &&
			(myMember->type == Unknown) &&
			(myMember->item.type == SDL_K_TYPE_BITFLD))
			myMember->item.length = context->options[ii].value;
		    break;

		default:
		    break;
	    }

	/*
	 * We have what we want, reset the options list for the member we are
	 * about to start.
	 */
	_sdl_reset_options(context);
    }

    /*
     * OK, we took care of adding the options to our predecessor, so now we
     * need to start a new member.
     */
    myMember = (SDL_MEMBERS *) calloc(1, sizeof(SDL_MEMBERS));
    if (myMember != NULL)
    {

	/*
	 * Determine the type of member we are being asked to create.
	 */
	myMember->type = subaggrType;
	switch (subaggrType)
	{
	    case Unknown:
		myMember->item.id = name;
		if (datatype < 0)
		{
		    myMember->item._unsigned = false;
		    datatype = -datatype;
		}
		else if (datatype > 0)
		    myMember->item._unsigned = true;
		if (aggrId != NULL)
		{
		    myAggr = _sdl_get_aggregate(&context->aggregates, aggrId);

		    myMember->item.type = myAggr->typeID;
		}
		else
		    myMember->item.type = datatype;
		if (datatype == SDL_K_TYPE_DECIMAL)
		{
		    myMember->item. precision = context->precision;
		    myMember->item.scale = context->scale;
		}
		myMember->item.size = _sdl_sizeof(context, datatype);
		break;

	    case Structure:
	    case Union:
		myMember->subaggr.id = name;
		myMember->subaggr.structUnion = subaggrType;
		if (datatype < 0)
		{
		    myMember->subaggr._unsigned = false;
		    datatype = -datatype;
		}
		else if (datatype > 0)
		    myMember->subaggr._unsigned = true;
		myMember->subaggr.parent = context->currentAggr;
		SDL_Q_INIT(&myMember->subaggr.members);
		context->aggregateDepth++;
		context->currentAggr = &myMember->subaggr;
		break;
	}
	if (mySubAggr != NULL)
	{
	    SDL_INSQUE(&mySubAggr->members, &myMember->header);
	}
	else
	{
	    SDL_INSQUE(&myAggr->members, &myMember->header);
	}
    }
    else
	retVal = 0;

   /*
    * Return the results of this call back to the caller.
    */
   return (retVal);
}

/*
 * sdl_aggregate_compl
 *  This function is called to complete the definition of an aggregate
 *  definition.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current parsing.
 *  name:
 *	A pointer to the string to be associated with this aggregate
 *	definition.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 */
int sdl_aggregate_compl(SDL_CONTEXT *context, char *name)
{
    SDL_AGGREGATE	*myAggr = (SDL_AGGREGATE *) context->currentAggr;
    SDL_SUBAGGR		*mySubAggr = (SDL_SUBAGGR *) context->currentAggr;
    int			retVal = 1;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:sdl_aggregate_compl\n", __FILE__, __LINE__);

    /*
     * If we have any options that had been processed, they are for the most
     * recent ITEM member.
     */
    if (context->optionsIdx > 0)
    {
	SDL_MEMBERS	*myMember = NULL;
	__int64_t	dimension;
	int		ii;

	/*
	 * OK, the issue is where is the most recent ITEM member.  It is in the
	 * current aggregate.  Don't assume that the queues actually contain
	 * anything, and make sure that the member is actually not a
	 * subaggregate.
	 */
	if (context->aggregateDepth == 1)
	{
	    if (SDL_Q_EMPTY(&myAggr->members) == false)
		myMember = myAggr->members.blink;
	}
	else
	{
	    if (SDL_Q_EMPTY(&mySubAggr->members) == false)
		myMember = mySubAggr->members.blink;
	}
	if ((myMember != NULL) && (myMember->type != Unknown))
	    myMember = NULL;
	for (ii = 0; ii < context->optionsIdx; ii++)
	    switch (context->options[ii].option)
	    {

		/*
		 * Present only options.
		 */
		case Align:
		    if (myMember != NULL)
			myMember->item.alignment = SDL_K_ALIGN;
		    break;

		case Fill:
		    if (myMember != NULL)
			myMember->item.fill = true;
		    break;

		case Mask:
		    if (myMember != NULL)
			myMember->item.mask = true;
		    break;

		case NoAlign:
		    if (myMember != NULL)
			myMember->item.alignment = SDL_K_NOALIGN;
		    break;

		case Signed:
		    if (myMember != NULL)
			myMember->item._signed = true;
		    break;

		case Typedef:
		    if (myMember != NULL)
			myMember->item.typeDef = true;
		    break;

		/*
		 * String options.
		 */
		case Prefix:
		    if (myMember != NULL)
			myMember->item.prefix = context->options[ii].string;
		    else
			free(context->options[ii].string);
		    context->options[ii].string = NULL;
		    break;

		case Tag:
		    if (myMember != NULL)
			myMember->item.tag = context->options[ii].string;
		    else
			free(context->options[ii].string);
		    context->options[ii].string = NULL;
		    break;

		/*
		 * Numeric options.
		 */
		case BaseAlign:
		    if (myMember != NULL)
			myMember->item.alignment = context->options[ii].value;
		    break;

		case Dimension:
		    dimension = context->options[ii].value;
		    if (myMember != NULL)
		    {
			myMember->item.lbound = context->dimensions[dimension].lbound;
			myMember->item.hbound = context->dimensions[dimension].hbound;
			myMember->item.dimension = true;
		    }
		    context->dimensions[dimension].inUse = false;
		    break;

		case Length:
		    if (myMember != NULL)
			myMember->item.length = context->options[ii].value;
		    break;

		default:
		    break;
	    }

	/*
	 * We have what we want, reset the options list for the member we are
	 * about to start.
	 */
	_sdl_reset_options(context);
    }

    /*
     * We are completing either a subaggregate or an AGGREGATE.  Decrement the
     * depth.
     */
    context->aggregateDepth--;

    /*
     * If we are at zero, then we have completed the entire AGGREGATE
     * definition and can now write out the results.
     */
    if (context->aggregateDepth == 0)
    {
	printf("\n\n+---------------------+\n");
	printf(    "| AGGREGATE Completed |\n");
	printf(    "+---------------------+\n\n");
    }

    /*
     * We just closed a subaggregate.  Make the previous subaggregate the
     * current one.
     */
    else
	context->currentAggr = mySubAggr->parent;

    /*
     * Return the results of this call back to the caller.
     */
    return (retVal);
}

/************************************************************************/
/* Local Functions							*/
/************************************************************************/

/*
 * _sdl_get_declare
 *  This function is called to get the record of a previously defined local
 *  variable.  If the local variable has not yet been declared, then a NULL
 *  will be returned.  Otherwise, the a pointer to the found DECLARE will be
 *  returned to the caller.
 *
 * Input Parameters:
 *  declare:
 *	A pointer to the list containing all currently defined DECLAREs.
 *  name:
 *	A pointer to the name of the declared user-type.
 *
 * Output Parameters:
 *  None.
 *
 * Return Value
 *  NULL:	Local variable not found.
 *  !NULL:	An existing local variable.
 */
static SDL_DECLARE *_sdl_get_declare(SDL_DECLARE_LIST *declare, char *name)
{
    SDL_DECLARE	*retVal = (SDL_DECLARE *) declare->header.flink;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:_sdl_get_declare\n", __FILE__, __LINE__);

    while (retVal != (SDL_DECLARE *) &declare->header)
	if (strcmp(retVal->id, name) == 0)
	    break;
	else
	    retVal = (SDL_DECLARE *) retVal->header.flink;
    if (retVal == (SDL_DECLARE *) &declare->header)
	retVal = NULL;

    /*
     * Return the results of this call back to the caller.
     */
    return(retVal);
}

/*
 * _sdl_get_item
 *  This function is called to get the record of a previously defined ITEM.  If
 *  the ITEM has not yet been defined, then a NULL will be returned.
 *  Otherwise, the a pointer to the found ITEM will be returned to the caller.
 *
 * Input Parameters:
 *  item:
 *	A pointer to the list containing all currently defined ITEMs.
 *  name:
 *	A pointer to the name of the declared item.
 *
 * Output Parameters:
 *  None.
 *
 * Return Value
 *  NULL:	ITEM not found.
 *  !NULL:	An existing ITEM.
 */
static SDL_ITEM *_sdl_get_item(SDL_ITEM_LIST *item, char *name)
{
    SDL_ITEM	*retVal = (SDL_ITEM *) item->header.flink;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:_sdl_get_item\n", __FILE__, __LINE__);

    while (retVal != (SDL_ITEM *) &item->header)
	if (strcmp(retVal->id, name) == 0)
	    break;
	else
	    retVal = (SDL_ITEM *) retVal->header.flink;
    if (retVal == (SDL_ITEM *) &item->header)
	retVal = NULL;

    /*
     * Return the results of this call back to the caller.
     */
    return(retVal);
}

/*
 * _sdl_get_aggregate
 *  This function is called to get the record of a previously defined
 *  AGGREGATE.  If the AGGREGATE has not yet been defined, then a NULL will be
 *  returned. Otherwise, the a pointer to the found AGGREGATE will be returned
 *  to the caller.
 *
 * Input Parameters:
 *  item:
 *	A pointer to the list containing all currently defined AGGREGATEs.
 *  name:
 *	A pointer to the name of the declared aggregate.
 *
 * Output Parameters:
 *  None.
 *
 * Return Value
 *  NULL:	AGGREGATE not found.
 *  !NULL:	An existing AGGREGATE.
 */
static SDL_AGGREGATE *_sdl_get_aggregate(SDL_AGGREGATE_LIST *aggregate, char *name)
{
    SDL_AGGREGATE	*retVal = (SDL_AGGREGATE *) aggregate->header.flink;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:_sdl_get_aggregate\n", __FILE__, __LINE__);

    while (retVal != (SDL_AGGREGATE *) &aggregate->header)
	if (strcmp(retVal->id, name) == 0)
	    break;
	else
	    retVal = (SDL_AGGREGATE *) retVal->header.flink;
    if (retVal == (SDL_AGGREGATE *) &aggregate->header)
	retVal = NULL;

    /*
     * Return the results of this call back to the caller.
     */
    return(retVal);
}

/*
 * _sdl_get_tag
 *  This function is used to determine the tag that should be used for a
 *  particular definition.  The user can specify a tag, which will be used
 *  instead of the default.  If one is not specified, then we need to determine
 *  what default tag should be used.  This is not necessarily as
 *  straightforward as you'd think.  Because a definition can be based off of a
 *  previous definition, which, in turn, can be based off of yet another
 *  definition, we need to loop until we find a base type to be utilized.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current state of the parsing.
 *  tag:
 *	A pointer to a string containing the user specified tag (or NULL).
 *  datatype:
 *	An integer indicating either a base type or a user type.  If a base,
 *	get the default.  If a user, we may need to call ourselves again to
 *	get what we came to get.
 *  lower:
 *	A boolean value to indicate that the defaulted tag should be lower
 *	case.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  A pointer to a user specified tag or a default tag.
 */
static char *_sdl_get_tag(SDL_CONTEXT *context, char *tag, int datatype,
bool lower)
{
    char	*retVal = tag;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:_sdl_get_tag\n", __FILE__, __LINE__);

    /*
     * If the user specified one, then we are done here.
     */
    if (tag == NULL)
    {

	/*
	 * If the datatype is a base type, then return then get the default
	 * tag for this type.
	 */
	if (datatype == SDL_K_TYPE_CONST)
	    retVal = strdup(_defaultTag[SDL_K_TYPE_CONST]);
	else if ((datatype >= SDL_K_BASE_TYPE_MIN) &&
		 (datatype <= SDL_K_BASE_TYPE_MAX))
	    retVal = strdup(_defaultTag[datatype]);
	else
	{

	    /*
	     * OK, the datatype is not a base type.  So it is either a DECLARE,
	     * ITEM, or AGGREGATE type.  Therefore we need to go figure out
	     * which it is and see if there is either a TAG defined there or
	     * it is a base type.
	     */
	    if ((datatype >= SDL_K_DECLARE_MIN) &&
		(datatype <= SDL_K_DECLARE_MAX))
	    {
		SDL_DECLARE *myDeclare = sdl_get_declare(
		        &context->declares,
		        datatype);

		if (myDeclare != NULL)
		{
		    if (strlen(myDeclare->tag) > 0)
			retVal = strdup(myDeclare->tag);
		    else
			retVal = _sdl_get_tag(
			        context,
			        tag,
			        myDeclare->typeID,
			        lower);
		}
		else
		    retVal = strdup(_defaultTag[SDL_K_TYPE_ANY]);
	    }
	    else if ((datatype >= SDL_K_ITEM_MIN) &&
		     (datatype <= SDL_K_ITEM_MAX))
	    {
		SDL_ITEM *myItem = sdl_get_item(&context->items, datatype);

		if (myItem != NULL)
		{
		    if (strlen(myItem->tag) > 0)
			retVal = strdup(myItem->tag);
		    else
			retVal = _sdl_get_tag(
			        context,
			        tag,
			        myItem->typeID,
			        lower);
		}
		else
		    retVal = strdup(_defaultTag[SDL_K_TYPE_ANY]);
	    }
	    else if ((datatype >= SDL_K_AGGREGATE_MIN) &&
		     (datatype <= SDL_K_AGGREGATE_MAX))
	    {
		SDL_AGGREGATE *myAggregate =
			sdl_get_aggregate(&context->aggregates, datatype);

		if (myAggregate != NULL)
		{
		    if (strlen(myAggregate->tag) > 0)
			retVal = strdup(myAggregate->tag);
		    else
			retVal = _sdl_get_tag(
					context,
					tag,
					myAggregate->typeID,
					lower);
		}
		else
		    retVal = strdup(_defaultTag[SDL_K_TYPE_ANY]);
	    }
	}
	if (lower == true)
	{
	    int ii, len = strlen(retVal);

	    for (ii = 0; ii < len; ii++)
		retVal[ii] = tolower(retVal[ii]);
	}
    }
    else
    {
	size_t len = strlen(retVal);
	size_t ii;
	_Bool done = false;

	/*
	 * Start at the end of the tag string and if the last character is an
	 * underscore, then change it to a null character.  Then check the next
	 * until we come across something other than an underscore or we run out
	 * of characters to check.
	 */
	for (ii = len - 1; ((ii > 0) && (done == false)); ii--)
	{
	    if (retVal[ii] == '_')
		retVal[ii] = '\0';
	    else
		done = true;
	}
    }

    /*
     * Return the results of this call back to the caller.
     */
    return (retVal);
}

/*
 * _sdl_sizeof
 *  This function is called to return the length of an item, based on the item
 *  indicated.  The item can be either a base type, or a user type.
 *
 * Input Parameters:
 *  item:
 *	A value indicating the item type for which we need the size.
 *
 * Output Parameters:
 *  None.
 *
 * Return Value:
 *  A value greater than or equal to zero, representing the size of the
 *  indicated type.
 */
static __int64_t _sdl_sizeof(SDL_CONTEXT *context, int item)
{
    __int64_t	retVal = 0;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:sdl_sizeof\n", __FILE__, __LINE__);

    if ((item >= SDL_K_BASE_TYPE_MIN) && (item <= SDL_K_BASE_TYPE_MAX))
	switch (item)
	{
	    case SDL_K_TYPE_BYTE:
		retVal = sizeof(char);
		break;

	    case SDL_K_TYPE_WORD:
		retVal = sizeof(short int);
		break;

	    case SDL_K_TYPE_LONG:
		retVal = sizeof(int);
		break;

	    case SDL_K_TYPE_QUAD:
		retVal = sizeof(__int64_t);
		break;

	    case SDL_K_TYPE_OCTA:
		retVal = sizeof(__int128_t);
		break;

	    case SDL_K_TYPE_TFLT:
		retVal = sizeof(float);
		break;

	    case SDL_K_TYPE_SFLT:
		retVal = sizeof(double);
		break;

	    case SDL_K_TYPE_DECIMAL:
		retVal = sizeof(float);
		break;

	    case SDL_K_TYPE_CHAR:
		retVal = sizeof(char);
		break;

	    case SDL_K_TYPE_ADDR:
		retVal = sizeof(void *);
		break;

	    case SDL_K_TYPE_ADDRL:
		retVal = sizeof(long int);
		break;

	    case SDL_K_TYPE_ADDRQ:
		retVal = sizeof(long long int);
		break;

	    case SDL_K_TYPE_ADDRHW:
		retVal = sizeof(void *);
		break;

	    case SDL_K_TYPE_BOOL:
		retVal = sizeof(_Bool);
		break;

	    default:
		break;
	}
    else if ((item >= SDL_K_DECLARE_MIN) && (item <= SDL_K_DECLARE_MAX))
    {
	SDL_DECLARE *myDeclare = sdl_get_declare(&context->declares, item);

	if (myDeclare != NULL)
	    retVal = myDeclare->size;
    }
    else if ((item >= SDL_K_ITEM_MIN) && (item <= SDL_K_ITEM_MAX))
    {
	SDL_ITEM *myItem = sdl_get_item(&context->items, item);

	if (myItem != NULL)
	    retVal = myItem->size;
    }
    else if ((item >= SDL_K_AGGREGATE_MIN) && (item <= SDL_K_AGGREGATE_MAX))
    {
	SDL_AGGREGATE *myAggregate =
	    sdl_get_aggregate(&context->aggregates, item);

	if (myAggregate != NULL)
	    retVal = myAggregate->size;
    }

    /*
     * Return the results of this call back to the caller.
     */
    return(retVal);
}

/*
 * _sdl_create_constant
 *  This function is called to create a constant record and return it back to
 *  the caller.
 *
 * Input Parameters:
 *  id:
 *  	A pointer to the constant identifier string.
 *  prefix:
 *	A pointer to the prefix to be prepended before the tag and id.
 *  tag:
 *	A pointer to the tag to be added between the prefix and the id.
 *  comment:
 *	A pointer to a comment string to be associated with this constant.
 *  typeName:
 *	A pointer to the typename associated to this constant.
 *  radix:
 *	A value indicating the radix the value is to be displayed.
 *  value:
 *	A value for the actual constant.
 *  string:
 *	A pointer to a string value for the actual constant.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  NULL:	Failed to allocate the memory required.
 *  !NULL:	Normal Successful Completion.
 */
static SDL_CONSTANT *_sdl_create_constant(
        char *id,
        char *prefix,
        char *tag,
        char *comment,
        char *typeName,
        int radix,
        __int64_t value,
        char *string)
{
    SDL_CONSTANT *retVal = calloc(1, sizeof(SDL_CONSTANT));

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:_sdl_create_constant\n", __FILE__, __LINE__);

    /*
     * Initialize the constant information.
     */
    if (retVal != NULL)
    {
	retVal->id = strdup(id);
	if (prefix != NULL)
	    retVal->prefix = strdup(prefix);
	else
	    retVal->prefix = NULL;
	retVal->tag = strdup(tag);
	if (comment != NULL)
	    retVal->comment = strdup(comment);
	else
	    retVal->comment = NULL;
	if (typeName != NULL)
	    retVal->typeName = strdup(typeName);
	else
	    retVal->typeName = NULL;
	retVal->radix = radix;
	if (string == NULL)
	{
	    retVal->type = SDL_K_CONST_NUM;
	    retVal->value = value;
	}
	else
	{
	    retVal->type = SDL_K_CONST_STR;
	    retVal->string = string;
	}
    }

    /*
     * Return the results back to the caller.
     */
    return (retVal);
}

/*
 * _sdl_queue_constant
 *  This function is called to queue a newly defined CONSTANT into the list in
 *  the context and then call the language specific routines to write out the
 *  language specific definition.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current state of the parsing.
 *  myConst:
 *	A pointer to the constant structure to be queued upo and put put.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 */
static int _sdl_queue_constant(SDL_CONTEXT *context, SDL_CONSTANT *myConst)
{
    int retVal = 1;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
	printf("%s:%d:_sdl_queue_constant\n", __FILE__, __LINE__);

    if (myConst != NULL)
    {
	int ii;

	SDL_INSQUE(&context->constants, &myConst->header);

	/*
	 * Loop through all the possible languages and call the
	 * appropriate output function for each of the enabled
	 * languages.
	 */
	for (ii = 0; ((ii < SDL_K_LANG_MAX) && (retVal == 1)); ii++)
	    if ((context->langSpec[ii] == true)
		    && (context->langEna[ii] == true))
		retVal = (*_outputFuncs[ii][SDL_K_CONSTANT_CB])(
		        context->outFP[ii],
		        myConst,
		        context);
    }
    else
	retVal = 0;

    /*
     * Return the results back to the caller.
     */
    return (retVal);
}

/*
 * _sdl_all_lower
 *  This function is called to determine if the supplied string is all lower
 *  case.
 *
 * Input Parameters:
 *  str:
 *	A pointer to the string to check.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  true:	'str' is all lower case.
 *  false:	'str' is either all uppercase or mixed case.
 */
static bool _sdl_all_lower(const char *str)
{
    int ii, len = (str ? strlen(str) : 0);
    bool retVal = true;

    for (ii = 0; ((ii < len) && (retVal == true)); ii++)
    {
	if (isalpha(str[ii]) == true)
	    retVal = islower(str[ii]);
    }

    /*
     * Return the results back to the caller.
     */
    return(retVal);
}

/*
 * _sdl_reset_options
 *  This function is called to reset the options array in the context block.
 *  If any of the saved options is a string and it is not NULL, then free it.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current state of the parsing.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  None.
 */
static void _sdl_reset_options(SDL_CONTEXT *context)
{
    int	ii;

    /*
     * Loop through each of the options, and if we have a string option and it
     * is not NULL, then free it.
     */
    for (ii = 0; ii < context->optionsIdx; ii++)
	if (((context->options[ii].option == Prefix) ||
	     (context->options[ii].option == Tag) ||
	     (context->options[ii].option == Counter) ||
	     (context->options[ii].option == TypeName) ||
	     (context->options[ii].option == Marker)) &&
	    (context->options[ii].string != NULL))
	    free(context->options[ii].string);

    /*
     * Reset the option index back to the beginning.
     */
    context->optionsIdx = 0;

    /*
     * Return back to the caller.
     */
    return;
}

/*
 * _sdl_iterate_members
 *  This function is called to iterate through each of the members and if that
 *  member is a subaggregate, call itself to iterate through that level.  If
 *  callback is a NULL, then we are just cleaning up the member tree.
 *
 * Input Parameters:
 *  context:
 *	A pointer to the context structure where we maintain information about
 *	the current state of the parsing.
 *  member:
 *	A pointer to the current member are are iterating through.
 *  qhead:
 *	The address of the queue header.  Used to determine when we have
 *	search through each of the member entries in the AGGREGATE or
 *	subaggregate.
 *  callback:
 *	A pointer to the entry point to call to do something with the member
 *	information.  If this is NULL, then we are just deallocating all the
 *	member memory.  If tracing is turned on, we will display the records
 *	as we iterate through.
 *  counter:
 *	A value indicating the item within the current level.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  1:	Normal Successful Completion.
 *  0:	An error occurred.
 */
static int _sdl_iterate_members(
		SDL_CONTEXT *context,
		SDL_MEMBERS *member,
		void *qhead,
		int (*callback)(),
		int count)
{
    int		retVal = 1;

    if ((member->type == Structure) ||
	(member->type == Union))
    {

	/*
	 * If the callback is NULL and tracing is turned on, then trace the
	 * information about this subaggregate.
	 */
	if ((callback == NULL) && (trace == true))
	{
	    if (count == 1)
		printf("    MEMBERs:\n");
	    printf(
		"\t%2d: name: %s\n\t    prefix: %s\n\t    tag: %s\n"
		"\t    marker: %s\n\t    arrgType: %s\n\t    typeID: %d\n"
		"\t    alignment: %d\n\t    type: %d\n\t    _unsigned: %s\n"
		"\t    size: %ld\n\t    memSize: %ld\n\t    typeDef: %s\n"
		"\t    fill: %s\n\t    basedPtrName: %s\n"
		"\t    currentBitOffset: %d\n",
		count,
		member->subaggr.id,
		(member->subaggr.prefix != NULL ? member->subaggr.prefix : ""),
		(member->subaggr.tag != NULL ? member->subaggr.tag : ""),
		(member->subaggr.marker != NULL ? member->subaggr.marker : ""),
		(member->subaggr.structUnion == Structure ? "STRUCTURE" : "UNION"),
		member->subaggr.typeID,
		member->subaggr.alignment,
		member->subaggr.type,
		(member->subaggr._unsigned == true ? "True" : "False"),
		member->subaggr.size,
		member->subaggr.memSize,
		(member->subaggr.typeDef == true ? "True" : "False"),
		(member->subaggr.fill == true ? "True" : "False"),
		(member->subaggr.basedPtrName!= NULL ? member->subaggr.basedPtrName: ""),
		member->subaggr.currentBitOffset);
	    if (member->subaggr.dimension == true)
		printf(
		    "\t    dimension[%ld:%ld]\n",
		    member->subaggr.lbound,
		    member->subaggr.hbound);
	}

	/*
	 * If the callback is not NULL, then call the function that should
	 * process this record future (usually to output to the language
	 * files).  If the callback is NULL, then we are just deallocating the
	 * memory associated with this AGGREGATE.
	 */
	if (callback != NULL)
	    retVal = (*callback)(context, NULL, member->subaggr.members.flink);

	/*
	 * We are performing an top-down iteration on AGGREGATEs and
	 * subaggregates, so before we may want to deallocate the memory, call
	 * ourselves to continue through the tree.
	 */
	if ((retVal == 1) && (member->subaggr.members.flink != qhead))
	    retVal = _sdl_iterate_members(
				context,
				member->subaggr.members.flink,
				&member->subaggr.members,
				callback,
				1);

	/*
	 * If the callback is NULL, we can now deallocate the memory for this
	 * subaggregate.
	 */
	if (callback == NULL)
	{
	    SDL_REMQUE(&member->header, member);
	    free(member->subaggr.id);
	    if (member->subaggr.prefix != NULL)
		free(member->subaggr.prefix);
	    if (member->subaggr.tag != NULL)
		free(member->subaggr.tag);
	    if (member->subaggr.marker != NULL)
		free(member->subaggr.marker);
	    if (member->subaggr.comment != NULL)
		free(member->subaggr.comment);
	    if (member->subaggr.basedPtrName != NULL)
		free(member->subaggr.basedPtrName);
	    free(member);
	}
    }
    else
    {

	/*
	 * If the callback is NULL and tracing is turned on, then trace the
	 * information about this subaggregate.
	 */
	if ((callback == NULL) && (trace == true))
	{
	    if (count == 1)
		printf("    MEMBERs:\n");
	    printf(
	        "\t%2d: name: %s\n\t    prefix: %s\n\t    tag: %s\n"
		"\t    typeID: %d\n\t    alignment: %d\n\t    type: %d\n"
		"\t    _unsigned: %s\n\t    size: %ld\n\t    memSize: %ld\n"
		"\t    typeDef: %s\n\t    fill: %s\n\t    length: %ld\n"
		"\t    mask: %s\n\t    signed: %s\n",
	        count,
	        member->item.id,
	        (member->item.prefix != NULL ? member->item.prefix : ""),
	        (member->item.tag != NULL ? member->item.tag : ""),
	        member->item.typeID,
	        member->item.alignment,
	        member->item.type,
	        (member->item._unsigned == true ? "True" : "False"),
	        member->item.size,
	        member->item.memSize,
	        (member->item.typeDef == true ? "True" : "False"),
	        (member->item.fill == true ? "True" : "False"),
	        member->item.length,
	        (member->item.mask == true ? "True" : "False"),
	        (member->item._signed == true ? "True" : "False"));
	    if (member->item.dimension == true)
		printf(
		    "\t    dimension: [%ld:%ld]\n",
		    member->item.lbound,
		    member->item.hbound);
	}
	if (callback != NULL)
	    retVal = (*callback)(context, member, NULL);

	/*
	 * Process the next member by calling ourselves back.
	 */
	if ((retVal == 1) && (member->header.flink != qhead))
	    retVal = _sdl_iterate_members(
				context,
				member->header.flink,
				qhead,
				callback,
				(count + 1));

	/*
	 * If the callback is NULL, we can now deallocate the memory for this
	 * member ITEM.
	 */
	if (callback == NULL)
	{
	    SDL_REMQUE(&member->header, member);
	    free(member->item.id);
	    if (member->item.prefix != NULL)
		free(member->item.prefix);
	    if (member->item.tag != NULL)
		free(member->item.tag);
	    if (member->item.comment != NULL)
		free(member->item.comment);
	    free(member);
	}
    }

    /*
     * Return the results back to the caller.
     */
    return(retVal);
}
