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
 *  V01.000    25-AUG-2018    Jonathan D. Belanger
 *  Initially written.
 *
 *  V01.001    08-SEP-2018    Jonathan D. Belanger
 *  Updated the copyright to be GNUGPL V3 compliant.
 *
 *  V01.002    10-OCT-2018    Jonathan D. Belanger
 *  Added a more complete definition of the possible data type keywords we can
 *  get from the parser.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <complex.h>
#include "opensdl_defs.h"
#include "library/language/opensdl_lang.h"
#include "library/utility/opensdl_plugin_funcs.h"
#include "library/common/opensdl_blocks.h"
#include "library/common/opensdl_message.h"
#include "library/utility/opensdl_utility.h"
#include "library/utility/opensdl_actions.h"
#include "opensdl/opensdl_main.h"

/*
 * The following defines the default tags for the various data types.
 */
static char *_defaultTag[] =
{
    "K",    /* CONSTANT */
    "B",    /* BYTE */
    "IB",   /* INTEGER_BYTE */
    "W",    /* WORD */
    "IW",   /* INTEGER_WORD */
    "L",    /* LONGWORD */
    "IL",   /* INTEGER_LONG */
    "IS",   /* INTEGER */
    "IH",   /* INTEGER_HW */
    "HI",   /* HARDWARE_INTEGER */
    "Q",    /* QUADWORD */
    "IQ",   /* INTEGER_QUAD */
    "O",    /* OCTAWORD */
    "T",    /* T_FLOATING */
    "TC",   /* T_FLOATING_COMPLEX */
    "S",    /* S_FLOATING */
    "SC",   /* S_FLOATING COMPLEX */
    "X",    /* X_FLOATING */
    "XC",   /* X_FLOATING COMPLEX */
    "F",    /* F_FLOATING */
    "FC",   /* F_FLOATING_COMPLEX */
    "D",    /* D_FLOATING */
    "DC",   /* D_FLOATING COMPLEX */
    "G",    /* G_FLOATING */
    "GC",   /* G_FLOATING_COMPLEX */
    "H",    /* H_FLOATING */
    "HC",   /* H_FLOATING COMPLEX */
    "P",    /* DECIMAL */
    "V",    /* BITFIELD        ("M" for mask;    "S" for size) */
    "VB",   /* BITFIELD BYTE    ("MB" for mask;    "SB" for size) */
    "VW",   /* BITFIELD WORD    ("MW" for mask;    "SW" for size) */
    "VL",   /* BITFIELD LONGWORD     ("ML" for mask;    "SL" for size) */
    "VQ",   /* BITFIELD QUADWORD    ("MQ" for mask;    "SQ" for size) */
    "VO",   /* BITFIELD OCTAWORD    ("MO" for mask;    "SO" for size) */
    "C",    /* CHAR */
    "CV",   /* CHAR VARYING */
    "CS",   /* CHAR * */
    "A",    /* ADDRESS */
    "AL",   /* ADDRESS_LONG */
    "AQ",   /* ADDRESS QUAD */
    "AH",   /* ADDRESS_HW */
    "HA",   /* HARDWARE_ADDRESS */
    "PS",   /* POINTER */
    "PL",   /* POINTER_LONG */
    "PQ",   /* POINTER_QUAD */
    "PH",   /* POINTER_HW */
    "",     /* ANY */
    "Z",    /* VOID */
    "B",    /* BOOLEAN */
    "R",    /* STRUCTURE */
    "R",    /* UNION */
    "N",    /* ENUM */
    "E",    /* ENTRY */
};

/*
 * Local Prototypes (found at the end of this module).
 */
static uint32_t _sdl_aggregate_callback(SDL_CONTEXT *context,
                                        SDL_MEMBERS *member,
                                        bool ending,
                                        int depth);
static SDL_DECLARE *_sdl_get_declare(SDL_DECLARE_LIST *declare, char *name);
static SDL_ITEM *_sdl_get_item(SDL_ITEM_LIST *item, char *name);
static char *_sdl_get_tag(SDL_CONTEXT *context,
                          char *tag,
                          int datatype,
                          bool lower);
static SDL_CONSTANT *_sdl_create_constant(char *id,
                                          char *prefix,
                                          char *tag,
                                          char *comment,
                                          char *typeName,
                                          int radix,
                                          int64_t value,
                                          char *string,
                                          int size,
                                          SDL_YYLTYPE *loc);
static uint32_t _sdl_queue_constant(SDL_CONTEXT *context,
                                    SDL_CONSTANT *myConst);
static SDL_ENUMERATE *_sdl_create_enum(SDL_CONTEXT *context,
                                       char *id,
                                       char *prefix,
                                       char *tag,
                                       bool typeDef,
                                       SDL_YYLTYPE *loc);
static uint32_t _sdl_enum_compl(SDL_CONTEXT *context, SDL_ENUMERATE *myEnum);
static void _sdl_reset_options(SDL_CONTEXT *context);
static uint32_t _sdl_iterate_members(SDL_CONTEXT *context,
                                     SDL_MEMBERS *member,
                                     void *qhead,
                                     int (*callback)(),
                                     int depth,
                                     int count);
static void _sdl_determine_offsets(SDL_CONTEXT *context,
                                   SDL_MEMBERS *member,
                                   SDL_QUEUE *memberList,
                                   bool parentIsUnion);
static void _sdl_fill_bitfield(SDL_QUEUE *memberList,
                               SDL_MEMBERS *member,
                               int bits,
                               int number,
                               SDL_YYLTYPE *loc);
static int64_t _sdl_aggregate_size(SDL_CONTEXT *context,
                                   SDL_AGGREGATE *aggr,
                                   SDL_SUBAGGR *subAggr);
static void _sdl_checkAndSetOrigin(SDL_CONTEXT *context, SDL_MEMBERS *member);
static void _sdl_check_bitfieldSizes(SDL_CONTEXT *context,
                                     SDL_QUEUE *memberList,
                                     SDL_MEMBERS *member,
                                     int64_t length,
                                     SDL_MEMBERS *newMember,
                                     bool *updated);
static uint32_t _sdl_create_bitfield_constants(SDL_CONTEXT *context,
                                               SDL_QUEUE *memberList);

/************************************************************************/
/* Functions called to create definitions from the Grammar file        */
/************************************************************************/

/*
 * sdl_comment_line
 *  This function is called to output a line comment to the output file.
 *
 * Input Parameters:
 *  context:
 *    A pointer to the context structure where we maintain information about
 *    the current parsing.
 *  comment:
 *      A pointer to the comment string to be output.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_ABORT:      An error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
uint32_t sdl_comment_line(SDL_CONTEXT *context,
                          char *comment,
                          SDL_YYLTYPE *loc)
{
    uint32_t retVal = SDL_NORMAL;

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL and comments are turned on, then go ahead and perform
     * the processing.
     */
    if ((context->processingEnabled == true) &&
        (context->argument[ArgComments].on == true))
    {

        /*
         * Trim all trailing space characters.
         */
        sdl_trim_str(comment, SDL_M_TRAIL);

        /*
         * If tracing is turned on, write out this call (calls only, no returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_comment_line ([%d:%d] to [%d:%d])\n",
                   __FILE__,
                   __LINE__,
                   loc->first_line, loc->first_column,
                   loc->last_line, loc->last_column);
        }

        /*
         * If this comment is in an AGGREGATE or subaggregate, then store it as
         * a member item.  It will be included in the output of the AGGREGATE
         * near where it existed in the input file.
         */
        if ((context->state == Aggregate) || (context->state == Subaggregate))
        {
            retVal = sdl_aggregate_member(context,
                                          &comment[2],/* ignore comment token */
                                          SDL_K_TYPE_COMMENT,
                                          SDL_K_TYPE_NONE,
                                          loc,
                                          true,
                                          false,
                                          false,
                                          false);
        }
        else
        {
            retVal = sdl_call_comment(context->langEnableVec,
                                      &comment[2],  /* ignore comment token */
                                      true,
                                      false,
                                      false,
                                      false);
        }
    }

    /*
     * Return the results of this call back to the caller.
     */
    sdl_free(comment);
    return(retVal);
}

/*
 * sdl_comment_block
 *  This function is called to output a block comment to the output file.
 *
 * Input Parameters:
 *  context:
 *    A pointer to the context structure where we maintain information about
 *    the current parsing.
 *  comment:
 *      A pointer to the comment string to be output.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_ABORT:      An error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
uint32_t sdl_comment_block(SDL_CONTEXT *context,
                           char *comment,
                           SDL_YYLTYPE *loc)
{
    char *ptr, *nl;
    uint32_t retVal = SDL_NORMAL;
    bool start_comment, start_done = false;
    bool middle_comment;
    bool end_comment;

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL and comments are turned on, then go ahead and perform
     * the processing.
     */
    if ((context->processingEnabled == true) &&
        (context->argument[ArgComments].on == true))
    {

        /*
         * Trim all trailing space characters.
         */
        sdl_trim_str(comment, SDL_M_TRAIL);

        /*
         * If tracing is turned on, write out this call (calls only, no
         * returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_comment_block ([%d:%d] to [%d:%d])\n",
                   __FILE__,
                   __LINE__,
                   loc->first_line,
                   loc->first_column,
                   loc->last_line,
                   loc->last_column);
        }

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
                {
                    *nl = '\0';
                }
                nl += 2;
            }
            else
            {
                nl = &ptr[strlen(ptr)];
            }
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
                size_t    len = strlen(ptr);

                end_comment = ((ptr[len - 2] == '/') && (ptr[len - 1] == '-'));
                if (end_comment == true)
                {
                    ptr[len - 2] = '\0';
                }
            }

            /*
             * If this comment is in an AGGREGATE or subaggregate, then store
             * it as a member item.  It will be included in the output of the
             * AGGREGATE near where it existed in the input file.
             */
            if ((context->state == Aggregate) ||
                (context->state == Subaggregate))
            {
                retVal = sdl_aggregate_member(context,
                                              ptr,
                                              SDL_K_TYPE_COMMENT,
                                              SDL_K_TYPE_NONE,
                                              loc,
                                              false,
                                              start_comment,
                                              middle_comment,
                                              end_comment);
            }
            else
            {
                retVal = sdl_call_comment(context->langEnableVec,
                                          ptr,
                                          false,
                                          start_comment,
                                          middle_comment,
                                          end_comment);
            }

            /*
             * Move to the next line.
             */
            ptr = nl;
        }
    }

    /*
     * Return the results of this call back to the caller.
     */
    sdl_free(comment);
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
 *    A pointer to the context structure where we maintain information about
 *    the local variables.
 *  name:
 *    A pointer to the name of the local variable.
 *  value:
 *    A 64-bit integer value the local variable it to be set.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Value
 *  SDL_NORMAL:     Normal Successful Completion, nothing action performed.
 *  SDL_CREATED:    Normal Successful Completion, local created.
 *  SDL_NOTCREATED: Normal Successful Completion, local not created.
 *  SDL_ABORT:      An unexpected error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
uint32_t sdl_set_local(SDL_CONTEXT *context,
                       char *name,
                       int64_t value,
                       SDL_YYLTYPE *loc)
{
    uint32_t retVal = SDL_NORMAL;

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL, then go ahead and perform the processing.
     */
    if (context->processingEnabled == true)
    {
        SDL_LOCAL_VARIABLE *local = sdl_find_local(context, name);

        /*
         * If tracing is turned on, write out this call (calls only, no
         * returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_set_local(", __FILE__, __LINE__);
            printf("%s, %ld (%016lx - %4.4s)\n",
                   name,
                   value,
                   value,
                   (char *) &value);
        }

        /*
         * OK, if we did not find a local variable with the same name, then we
         * need to go get one.
         */
        if (local == NULL)
        {
            local = sdl_allocate_block(LocalBlock, NULL, loc);
            if (local != NULL)
            {
                local->id = name;
                SDL_INSQUE(&context->locals, &local->header.queue);
                retVal = SDL_CREATED;
            }
            else
            {
                sdl_free(name);
                retVal = SDL_ABORT;
                if (sdl_set_message(msgVec,
                                    2,
                                    retVal,
                                    ENOMEM) != SDL_NORMAL)
                {
                    retVal = SDL_ERREXIT;
                }
            }
        }
        else
        {
            retVal = SDL_NOTCREATED;
        }

        /*
         * If we still have a success, then set the value.
         */
        local->value = value;
    }

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
 *    A pointer to the context structure where we maintain information about
 *    the current parsing.
 *  moduleName:
 *      A pointer to the MODULE module-name string to be output.
 *  identString:
 *      A pointer to the IDENT "ident-string" string to be output.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_ABORT:      An unexpected error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
uint32_t sdl_module(SDL_CONTEXT *context,
                    char *moduleName,
                    char *identName,
                    SDL_YYLTYPE *loc)
{
    uint32_t retVal = SDL_NORMAL;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:sdl_module ([%d:%d] to [%d:%d])\n",
               __FILE__,
               __LINE__,
               loc->first_line,
               loc->first_column,
               loc->last_line,
               loc->last_column);
    }

    /*
     * Save the MODULE's module-name (and the source line number for it).
     */
    context->module = moduleName;
    SDL_COPY_LOC(context->modStartloc, loc);

    /*
     * Save the IDENT's indent-name.
     */
    context->ident = identName;

    retVal = sdl_call_module(context->langEnableVec, context);

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
 *    A pointer to the context structure where we maintain information about
 *    the current parsing.
 *  moduleName:
 *      A pointer to the MODULE module-name string.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_MATCHEND:   The module name specified on END does not match MODULE.
 *  SDL_ABORT:      An unexpected error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
uint32_t sdl_module_end(SDL_CONTEXT *context,
                        char *moduleName,
                        SDL_YYLTYPE *loc)
{
    uint32_t retVal = SDL_NORMAL;
    int ii;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:sdl_module_end ([%d:%d] to [%d:%d])\n",
               __FILE__,
               __LINE__,
               loc->first_line,
               loc->first_column,
               loc->last_line,
               loc->last_column);
    }

    /*
     * Save the source line number for the END_MODULE.
     */
    SDL_COPY_LOC(context->modEndloc, loc);

    if ((moduleName != NULL) && (strcmp(context->module, moduleName) != 0))
    {
        retVal = SDL_MATCHEND;
        if (sdl_set_message(msgVec,
                            1,
                            retVal,
                            context->module,
                            loc->first_line) != SDL_NORMAL)
        {
            retVal = SDL_ERREXIT;
        }
    }

    /*
     * OK, time to write out the OpenSDL Parser's MODULE footer.
     */
    if (retVal == SDL_NORMAL)
    {
        retVal = sdl_call_moduleEnd(context->langEnableVec, context);
    }

    /*
     * Reset all the dimension entries.
     */
    for (ii = 0; ii < SDL_K_MAX_DIMENSIONS; ii++)
    {
        context->dimensions[ii].inUse = 0;
    }

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
            printf("--------------------------------\n");
            if (ii == 1)
            {
                printf("    Local Variables:\n");
            }
            printf("\t%2d: name: %s\n\t    value: %ld\n",
                   ii++,
                   local->id,
                   local->value);
        }
        sdl_deallocate_block(&local->header);
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
            printf("--------------------------------\n");
            if (ii == 1)
            {
                printf("    CONSTANTs:\n");
            }
            printf("\t%2d: name: %s\n\t    prefix: %s\n\t    tag: %s\n"
                   "\t    typeName: %s\n\t    type: %s\n",
                   ii++,
                   constant->id,
                   (constant->prefix == NULL ? "" : constant->prefix),
                   (constant->tag == NULL ? "" : constant->tag),
                   (constant->typeName == NULL ? "" : constant->typeName),
                   (constant->type == SDL_K_CONST_STR ? "String" : "Number"));
            if (constant->type == SDL_K_CONST_STR)
            {
                printf("\t    value: %s\n", constant->string);
            }
            else
            {
                printf("\t    value: %ld (%s)\n",
                       constant->value,
                       (constant->radix <= SDL_K_RADIX_DEC ? "Decimal" :
                           (constant->radix == SDL_K_RADIX_OCT ? "Octal" :
                               (constant->radix == SDL_K_RADIX_HEX ?
                                   "Hexidecimal" : "Invalid"))));
            }
            if (constant->comment != NULL)
            {
                printf("\t    comment: %s\n", constant->comment);
            }
        }
        sdl_deallocate_block(&constant->header);
    }

    /*
     * Clean out all the enumeration definitions.
     */
    ii = 1;
    while(SDL_Q_EMPTY(&context->enums.header) == false)
    {
        SDL_ENUMERATE *_enum;
        SDL_ENUM_MEMBER *member;
        int jj;

        SDL_REMQUE(&context->enums.header, _enum);
        if (trace == true)
        {
            printf("--------------------------------\n");
            if (ii == 1)
            {
                printf("    ENUMs:\n");
            }
            printf("\t%2d: name: %s\n\t    prefix: %s\n\t    tag: %s\n"
                   "\t    _typeDef: %s\n",
                   ii++,
                   _enum->id,
                   (_enum->prefix == NULL ? "" : _enum->prefix),
                   (_enum->tag == NULL ? "" : _enum->tag),
                   (_enum->typeDef == true ? "True" : "False"));
        }
        jj = 1;
        member = (SDL_ENUM_MEMBER *) _enum->members.flink;
        while (&member->header.queue != &_enum->members)
        {
            if (trace == true)
            {
                if (jj == 1)
                {
                    printf("    ENUM_MEMBERs:\n");
                }
                printf("\t%2d: name: %s\n\t    value: %ld\n\t    valueSet: %s\n",
                       jj++,
                       member->id,
                       member->value,
                       (member->valueSet == true ? "True" : "False"));
                if (member->comment != NULL)
                {
                    printf("\t    comment: %s\n", member->comment);
                }
            }
            member = (SDL_ENUM_MEMBER *) member->header.queue.flink;
        }
        sdl_deallocate_block(&_enum->header);
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
            printf("--------------------------------\n");
            if (ii == 1)
            {
                printf("    DECLAREs:\n");
            }
            printf("\t%2d: name: %s\n\t    prefix: %s\n\t    tag: %s\n"
                   "\t    typeID: %d\n\t    type: %d\n\t    size: %ld\n",
                   ii++,
                   declare->id,
                   (declare->prefix != NULL ? declare->prefix : ""),
                   (declare->tag != NULL ? declare->tag : ""),
                   declare->typeID,
                   declare->type,
                   declare->size);
        }
        sdl_deallocate_block(&declare->header);
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
            printf("--------------------------------\n");
            if (ii == 1)
            {
                printf("    ITEMs:\n");
            }
            printf("\t%2d: name: %s\n\t    prefix: %s\n\t    tag: %s\n"
                   "\t    typeID: %d\n\t    alignment: %d\n\t    type: %d\n"
                   "\t    size: %ld\n\t    commonDef: %s\n"
                   "\t    globalDef: %s\n\t    typeDef: %s\n",
                   ii++,
                   item->id,
                   (item->prefix != NULL ? item->prefix : ""),
                   (item->tag != NULL ? item->tag : ""),
                   item->typeID,
                   item->alignment,
                   item->type,
                   item->size,
                   (item->commonDef == true ? "True" : "False"),
                   (item->globalDef == true ? "True" : "False"),
                   (item->typeDef == true ? "True" : "False"));
            if (item->dimension == true)
            {
                printf("\t    dimension: [%ld:%ld]\n",
                       item->lbound,
                       item->hbound);
            }
        }
        sdl_deallocate_block(&item->header);
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
            printf("--------------------------------\n");
            if (ii == 1)
            {
                printf("    AGGREGATEs:\n");
            }
            printf("\t%2d: name: %s\n\t    structUnion: %s\n\t    prefix: %s\n"
                   "\t    marker: %s\n\t    tag: %s\n\t    origin: %s\n"
                   "\t    typeID: %d\n\t    alignment: %d\n\t    type: %d\n"
                   "\t    bitOffset: %d\n\t    byteOffset: %ld\n\t    size: %ld\n"
                   "\t    commonDef: %s\n"
                   "\t    globalDef: %s\n\t    typeDef: %s\n\t    fill: %s\n"
                   "\t    _unsigned: %s\n",
                   ii++,
                   aggregate->id,
                   (aggregate->aggType == SDL_K_TYPE_STRUCT ? "STRUCTURE" : "UNION"),
                   (aggregate->prefix != NULL ? aggregate->prefix : ""),
                   (aggregate->marker != NULL ? aggregate->marker : ""),
                   (aggregate->tag != NULL ? aggregate->tag : ""),
                   (aggregate->origin.id != NULL ? aggregate->origin.id : ""),
                   aggregate->typeID,
                   aggregate->alignment,
                   aggregate->type,
                   aggregate->currentBitOffset,
                   aggregate->currentOffset,
                   aggregate->size,
                   (aggregate->commonDef == true ? "True" : "False"),
                   (aggregate->globalDef == true ? "True" : "False"),
                   (aggregate->typeDef == true ? "True" : "False"),
                   (aggregate->fill == true ? "True" : "False"),
                   (aggregate->_unsigned == true ? "True" : "False"));
            if (aggregate->dimension == true)
            {
                printf("\t    dimension: [%ld:%ld]\n",
                       aggregate->lbound,
                       aggregate->hbound);
            }
        }
        if (SDL_Q_EMPTY(&aggregate->members) == false)
        {
            _sdl_iterate_members(context,
                                 aggregate->members.flink,
                                 &aggregate->members,
                                 NULL,
                                 1,
                                 1);
        }
        sdl_deallocate_block(&aggregate->header);
    }

    /*
     * Clean out all the entries.
     */
    ii = 1;
    while (SDL_Q_EMPTY(&context->entries) == false)
    {
        SDL_ENTRY *entry;
        SDL_PARAMETER *param;
        int jj;

        SDL_REMQUE(&context->entries, entry);
        if (trace == true)
        {
            printf("--------------------------------\n");
            if (ii == 1)
            {
                printf("    ENTRYs:\n");
            }
            printf("\t%2d: name: %s\n", ii++, entry->id);
            if (entry->alias != NULL)
            {
                printf("\t    alias: %s\n", entry->alias);
            }
            if (entry->typeName != NULL)
            {
                printf("\t    typeName: %s\n", entry->typeName);
            }
            if (entry->linkage != NULL)
            {
                printf("\t    linkage: %s\n", entry->linkage);
            }
            printf("\t    returns.type: %ld\n\t    returns._unsigned: %s\n",
                   entry->returns.type,
                   (entry->returns._unsigned ? "True" : "False"));
            if (entry->returns.name != NULL)
            {
                printf("\t    returns.named: %s\n", entry->returns.name);
            }
        }
        jj = 1;
        param = (SDL_PARAMETER *) entry->parameters.flink;
        while (&param->header.queue != &entry->parameters)
        {
            if (trace == true)
            {
                if (jj == 1)
                {
                    printf("    PARAMETERs:\n");
                }
                printf("\t%2d: name: %s\n\t    type: %d\n\t    typeName: %s\n"
                       "\t    bound: %ld\n\t    defaultValue: %ld\n"
                       "\t    defaultPresent: %s\n\t    dimension: %s\n"
                       "\t    in: %s\n\t    out: %s\n\t    list: %s\n"
                       "\t    optional: %s\n\t    _unsigned: %s\n",
                       jj++,
                       param->name,
                       param->type,
                       param->typeName,
                       param->bound,
                       param->defaultValue,
                       (param->defaultPresent == true ? "True" : "False"),
                       (param->dimension== true ? "True" : "False"),
                       (param->in == true ? "True" : "False"),
                       (param->out == true ? "True" : "False"),
                       (param->list == true ? "True" : "False"),
                       (param->optional == true ? "True" : "False"),
                       (param->_unsigned == true ? "True" : "False"));
            }
            param = (SDL_PARAMETER *) param->header.queue.flink;
        }
        sdl_deallocate_block(&entry->header);
    }

    /*
     * Reset the module-name and ident-string to zero length.
     */
    sdl_free(context->module);
    context->module = NULL;
    if (context->ident != NULL)
    {
        sdl_free(context->ident);
    }
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
 *  context:
 *    A pointer to the context structure where we maintain information about
 *    the current parsing.
 *  literals:
 *    A pointer to the queue containing the current set of literal lines.
 *  line:
 *      A pointer to the line of text from the input file.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_ABORT:      An unexpected error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
uint32_t sdl_literal(SDL_CONTEXT *context,
                     SDL_QUEUE *literals,
                     char *line,
                     SDL_YYLTYPE *loc)
{
    SDL_LITERAL *literalLine;
    uint32_t retVal = SDL_NORMAL;
    size_t len = strlen(line);

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL, then go ahead and perform the processing.
     */
    if (context->processingEnabled == true)
    {

        /*
         * Before we go too far, strip ending control characters.
         */
        while (((line[len-1] == '\n') ||
                (line[len-1] == '\f') ||
                (line[len-1] == '\r')) &&
               (len > 0))
        {
            line[--len] = '\0';
        }

        /*
         * If tracing is turned on, write out this call (calls only, no
         * returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_literal(%.*s)\n",
                   __FILE__,
                   __LINE__,
                   (int) len,
                   line);
        }

        literalLine = sdl_allocate_block(LiteralBlock, NULL, loc);
        if (literalLine != NULL)
        {
            literalLine->line = line;
            SDL_INSQUE(literals, &literalLine->header.queue);
        }
        else
        {
            retVal = SDL_ABORT;
            if (sdl_set_message(msgVec,
                                2,
                                retVal,
                                ENOMEM) != SDL_NORMAL)
            {
                retVal = SDL_ERREXIT;
            }
            sdl_free(line);
        }
    }
    else
    {
        sdl_free(line);
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
 *    A pointer to the context structure where we maintain information about
 *    the current parsing.
 *  literals:
 *    A pointer to the queue containing the current set of literal lines.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_ABORT:      An unexpected error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
uint32_t sdl_literal_end(SDL_CONTEXT *context,
                         SDL_QUEUE *literals,
                         SDL_YYLTYPE *loc)
{
    SDL_LITERAL *literalLine;
    uint32_t retVal = SDL_NORMAL;

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL, then go ahead and perform the processing.
     */
    if (context->processingEnabled == true)
    {

        /*
         * If tracing is turned on, write out this call (calls only, no
         * returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_literal_end\n", __FILE__, __LINE__);
        }

        /*
         * Keep pulling off literal lines until there are no more.
         */
        while ((SDL_Q_EMPTY(literals) == false) && (retVal == SDL_NORMAL))
        {
            SDL_REMQUE(literals, literalLine);

            retVal = sdl_call_literal(context->langEnableVec,
                                      literalLine->line);

            /*
             * Free up all the memory we allocated for this literal line.
             */
            sdl_deallocate_block(&literalLine->header);
        }
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
 *    A pointer to the context structure where we maintain information about
 *    the current parsing.
 *  name:
 *    A pointer to a string containing the name of the type.
 *  sizeType:
 *    A value indicating the size or datatype of the declaration.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Value:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_ABORT:      An unexpected error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
uint32_t sdl_declare(SDL_CONTEXT *context,
                     char *name,
                     int64_t sizeType,
                     SDL_YYLTYPE *loc)
{
    uint32_t retVal = SDL_NORMAL;

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL, then go ahead and perform the processing.
     */
    if (context->processingEnabled == true)
    {
        SDL_DECLARE *myDeclare = _sdl_get_declare(&context->declares, name);

        /*
         * If tracing is turned on, write out this call (calls only, no
         * returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_declare\n", __FILE__, __LINE__);
        }

        /*
         * We only create, never update.
         */
        if (myDeclare == NULL)
        {
            myDeclare = sdl_allocate_block(DeclareBlock, NULL, loc);
            if (myDeclare != NULL)
            {
                myDeclare->id = name;
                myDeclare->typeID = context->declares.nextID++;
                myDeclare->_unsigned = sdl_isUnsigned(context, &sizeType);
                if (sizeType >= SDL_K_SIZEOF_MIN)
                {
                    myDeclare->size = sizeType / SDL_K_SIZEOF_MIN;
                    myDeclare->type = SDL_K_TYPE_CHAR;
                }
                else
                {
                    myDeclare->size = sdl_sizeof(context, sizeType);
                    myDeclare->type = sizeType;
                }
                SDL_INSQUE(&context->declares.header,
                           &myDeclare->header.queue);
            }
            else
            {
                retVal = SDL_ABORT;
                if (sdl_set_message(msgVec,
                                    2,
                                    retVal,
                                    ENOMEM) != SDL_NORMAL)
                {
                    retVal = SDL_ERREXIT;
                }
                sdl_free(name);
            }
        }
    }
    else
    {
        sdl_free(name);
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
 *    A pointer to the context structure where we maintain information about
 *    the current parsing.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Value:
 *  SDL_NORMAL:     Normal Successful Completion.
 */
uint32_t sdl_declare_compl(SDL_CONTEXT *context, SDL_YYLTYPE *loc)
{
    SDL_DECLARE *myDeclare = (SDL_DECLARE *) context->declares.header.blink;
    char *prefix = NULL;
    char *tag = NULL;
    uint32_t ii, retVal = SDL_NORMAL;

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL, then go ahead and perform the processing.
     */
    if (context->processingEnabled == true)
    {

        /*
         * If tracing is turned on, write out this call (calls only, no
         * returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_declare_compl\n", __FILE__, __LINE__);
        }

        /*
         * Go find our options
         */
        for (ii = 0; ii < context->optionsIdx; ii++)
        {
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
        }

        /*
         * We only finish creating, never starting new.
         */
        if (myDeclare != NULL)
        {
            myDeclare->prefix = prefix;
            myDeclare->tag = _sdl_get_tag(context,
                                          tag,
                                          myDeclare->type,
                                          sdl_all_lower(myDeclare->id));
        }
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
 *    A pointer to the context structure where we maintain information about
 *    the current parsing.
 *  name:
 *    A pointer the the name of the item to be defined.
 *  datatype:
 *    A value to be associated with the datatype for this item.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_ABORT:      An unexpected error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
uint32_t sdl_item(
        SDL_CONTEXT *context,
        char *name,
        int64_t datatype,
        SDL_YYLTYPE *loc)
{
    uint32_t retVal = SDL_NORMAL;

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL, then go ahead and perform the processing.
     */
    if (context->processingEnabled == true)
    {
        SDL_ITEM    *myItem = _sdl_get_item(&context->items, name);

        /*
         * If tracing is turned on, write out this call (calls only, no
         * returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_item\n", __FILE__, __LINE__);
        }

        /*
         * If we did not find a item that had already been created, then go and
         * create one now.
         */
        if (myItem == NULL)
        {
            myItem = sdl_allocate_block(ItemBlock, NULL, loc);
            if (myItem != NULL)
            {
                myItem->id = name;
                myItem->typeID = context->items.nextID++;
                myItem->_unsigned = sdl_isUnsigned(context, &datatype);
                myItem->type = datatype;
                if (datatype == SDL_K_TYPE_DECIMAL)
                {
                    myItem->precision = context->precision;
                    myItem->scale = context->scale;
                }
                myItem->size = sdl_sizeof(context, datatype);
                SDL_INSQUE(&context->items.header, &myItem->header.queue);
            }
        }
        else
        {
            retVal = SDL_ABORT;
            if (sdl_set_message(msgVec,
                                2,
                                retVal,
                                ENOMEM) != SDL_NORMAL)
            {
                retVal = SDL_ERREXIT;
            }
            sdl_free(name);
        }
    }
    else
    {
        sdl_free(name);
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
 *    A pointer to the context structure where we maintain information about
 *    the current parsing.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:        Normal Successful Completion.
 *  SDL_ADROBJBAS:    Aggregate must have BASED storage class.
 *  SDL_ERREXIT:    Error exit.
 */
uint32_t sdl_item_compl(SDL_CONTEXT *context, SDL_YYLTYPE *loc)
{
    SDL_ITEM *myItem = context->items.header.blink;
    char *prefix = NULL;
    char *tag = NULL;
    int64_t addrType = SDL_K_TYPE_NONE;
    uint32_t retVal = SDL_NORMAL;
    int ii;
    int storage = 0;
    int basealign = 0;
    int dimension = 0;

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL, then go ahead and perform the processing.
     */
    if (context->processingEnabled == true)
    {

        /*
         * If tracing is turned on, write out this call (calls only, no
         * returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_item_compl\n", __FILE__, __LINE__);
        }

        /*
         * Go find our options
         */
        for (ii = 0; ii < context->optionsIdx; ii++)
        {
            switch(context->options[ii].option)
            {
                case Prefix:
                    prefix = context->options[ii].string;
                    context->options[ii].string = NULL;
                    break;

                case Tag:
                    tag = context->options[ii].string;
                    context->options[ii].string = NULL;
                    break;

                case BaseAlign:
                    basealign= context->options[ii].value;
                    break;

                case Dimension:
                    dimension = context->options[ii].value;
                    myItem->dimension = true;
                    break;

                case Common:
                    storage |= SDL_M_STOR_COMM;
                    break;

                case Global:
                    storage |= SDL_M_STOR_GLOB;
                    break;

                case Typedef:
                    storage |= SDL_M_STOR_TYPED;
                    break;

                case SubType:
                    addrType= context->options[ii].value;
                    break;

                default:
                    break;
            }
        }

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
            myItem->tag = _sdl_get_tag(context,
                                       tag,
                                       myItem->type,
                                       sdl_all_lower(myItem->id));

            /*
             * Addresses can have sub-types.
             */
            if (sdl_isAddress(myItem->type) == true)
            {
                myItem->subType = addrType;
                if ((addrType >= SDL_K_AGGREGATE_MIN) &&
                    (addrType <= SDL_K_AGGREGATE_MAX))
                {
                    SDL_AGGREGATE *myAggr;

                    myAggr = sdl_get_aggregate(&context->aggregates, addrType);
                    if ((myAggr != NULL) && (myAggr->basedPtrName == NULL))
                    {
                        retVal = SDL_ADROBJBAS;
                        if (sdl_set_message(msgVec,
                                            1,
                                            retVal,
                                            myAggr->id,
                                            loc->first_line) != SDL_NORMAL)
                        {
                            retVal = SDL_ERREXIT;
                        }
                    }
                }
            }
            if (retVal == SDL_NORMAL)
            {
                retVal = sdl_call_item(context->langEnableVec,
                                       myItem,
                                       context);
            }
        }
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
 *    A pointer to the context structure where we maintain information about
 *    the current parsing.
 *  id:
 *    A pointer the the name/names of the constants to be defined.
 *  value:
 *    A value to be associated with the constant.
 *  valueStr:
 *    A pointer to a string to be associated with the constant.  If this is
 *    NULL, then value is used.  Otherwise, this is used.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 */
uint32_t sdl_constant(SDL_CONTEXT *context,
                      char *id,
                      int64_t value,
                      char *valueStr,
                      SDL_YYLTYPE *loc)
{
    uint32_t retVal = SDL_NORMAL;

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL, then go ahead and perform the processing.
     */
    if (context->processingEnabled == true)
    {

        /*
         * If tracing is turned on, write out this call (calls only, no returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_constant\n", __FILE__, __LINE__);
        }

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
    }
    else if (valueStr != NULL)
    {
        sdl_free(valueStr);
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
 *    A pointer to the context structure where we maintain information about
 *    the current parsing.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_ABORT:      An expected error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
#define _SDL_OUTPUT_COMMENT    0
#define _SDL_COMMA_        2
#define _SDL_COMMENT_LIST_NULL    3
uint32_t sdl_constant_compl(SDL_CONTEXT *context, SDL_YYLTYPE *loc)
{
    SDL_CONSTANT *myConst;
    SDL_ENUMERATE *myEnum = NULL;
    char *id = context->constDef.id;
    int64_t value = context->constDef.value;
    char *valueStr = (context->constDef.string ?
                        context->constDef.valueStr :
                        NULL);
    char *comma = strchr(id, ',');
    static char *commentList[] = {"/*", "{", ",", NULL};
    uint32_t ii, retVal = SDL_NORMAL;
    char *prefix = NULL;
    char *tag = NULL;
    char *counter = NULL;
    char *typeName = NULL;
    char *enumName = NULL;
    int64_t increment = 0;
    int64_t radix = SDL_K_RADIX_DEF;
    int datatype = SDL_K_TYPE_CONST;
    int size = context->argument[ArgWordSize].value;
    uint32_t localCreated = SDL_NOTCREATED;
    bool incrementPresent = false;
    bool typeDef = false;

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL, then go ahead and perform the processing.
     */
    if (context->processingEnabled == true)
    {

        /*
         * If tracing is turned on, write out this call (calls only, no returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_constant_compl\n", __FILE__, __LINE__);
        }

        /*
         * Go find our options
         */
        for (ii = 0; ii < context->optionsIdx; ii++)
        {
            switch(context->options[ii].option)
            {
                case Prefix:
                    prefix = context->options[ii].string;
                    context->options[ii].string = NULL;
                    break;

                case Tag:
                    tag = context->options[ii].string;
                    context->options[ii].string = NULL;
                    break;

                case Counter:
                    counter = context->options[ii].string;
                    context->options[ii].string = NULL;
                    localCreated = sdl_set_local(context,
                                                 counter,
                                                 value,
                                                 loc);
                    break;

                case TypeName:
                    typeName = context->options[ii].string;
                    context->options[ii].string = NULL;
                    break;

                case Increment:
                    increment = context->options[ii].value;
                    incrementPresent = true;
                    break;

                case Radix:
                    radix = context->options[ii].value;
                    break;

                case Enumerate:
                    enumName = context->options[ii].string;
                    context->options[ii].string = NULL;
                    break;

                case Typedef:
                    typeDef = true;
                    break;

                default:
                    break;
            }
        }

        /*
         * Before we go too far, we need to determine what kind of definition
         * we have.  First let's see if there is one or multiple names needing
         * to be created.
         */
        if (comma == NULL)    /* single CONSTANT or ENUM */
        {

            /*
             * If we are not declaring a string constant and an enum name was
             * specified, then we are not creating a CONSTANT, but an ENUM.  Adjust
             * the data-type accordingly.
             */
            if ((valueStr == NULL) && (enumName != NULL))
            {
                datatype = SDL_K_TYPE_ENUM;
            }
            if (tag == NULL)
            {
                tag = _sdl_get_tag(context,
                                   NULL,
                                   datatype,
                                   sdl_all_lower(id));
            }

            /*
             * OK, we have the tag we need, so if we are really creating a
             * CONSTANT, then do so now.
             */
            if ((valueStr != NULL) || (enumName == NULL))
            {
                myConst = _sdl_create_constant(id,
                                               prefix,
                                               tag,
                                               NULL,
                                               typeName,
                                               radix,
                                               value,
                                               valueStr,
                                               size,
                                               loc);
                if (myConst != NULL)
                {
                    retVal = _sdl_queue_constant(context, myConst);
                }
                else
                {
                    retVal = SDL_ABORT;
                    if (sdl_set_message(msgVec,
                                        2,
                                        retVal,
                                        ENOMEM) != SDL_NORMAL)
                    {
                        retVal = SDL_ERREXIT;
                    }
                }
            }

            /*
             * Otherwise, we are creating an enumeration (just one).
             */
            else
            {
                myEnum = _sdl_create_enum(context,
                                          enumName,
                                          prefix,
                                          tag,
                                          typeDef,
                                          loc);
                if (myEnum != NULL)
                {
                    SDL_ENUM_MEMBER *myMem = sdl_allocate_block(EnumMemberBlock,
                                                                &myEnum->header,
                                                                loc);

                    if (myMem != NULL)
                    {

                        /*
                         * Initialize the lone enumeration member and queue it
                         * up.
                         */
                        myMem->id = id;
                        myMem->value = value;
                        myMem->valueSet = value != 0;
                        SDL_INSQUE(&myEnum->members, &myMem->header.queue);
                    }
                    else
                    {
                        sdl_free(myEnum);
                        myEnum = NULL;
                        retVal = SDL_ABORT;
                        if (sdl_set_message(msgVec,
                                            2,
                                            retVal,
                                            ENOMEM) != SDL_NORMAL)
                        {
                            retVal = SDL_ERREXIT;
                        }
                    }
                }
                else
                {
                    myEnum = NULL;
                    if (sdl_set_message(msgVec,
                                        2,
                                        retVal,
                                        ENOMEM) != SDL_NORMAL)
                    {
                        retVal = SDL_ERREXIT;
                    }
                }
            }
        }
        else    /* list of CONSTANTs or ENUMs */
        {
            char *ptr = id;
            char *nl;
            int64_t prevValue = value;
            bool freeTag = tag == NULL;
            bool done = false;

            /*
             * If an enum name was specified, then we are not creating a
             * CONSTANT, but an ENUM.  Adjust the data-type accordingly.  Also,
             * allocate and initialize the ENUM parent.
             */
            if (enumName != NULL)
            {
                datatype = SDL_K_TYPE_ENUM;
                myEnum = _sdl_create_enum(context,
                                          enumName,
                                          prefix,
                                          tag,
                                          typeDef,
                                          loc);
                if (myEnum == NULL)
                {
                    retVal = SDL_ABORT;
                    if (sdl_set_message(msgVec,
                                        2,
                                        retVal,
                                        ENOMEM) != SDL_NORMAL)
                    {
                        retVal = SDL_ERREXIT;
                    }
                }
            }

            sdl_trim_str(ptr, SDL_M_LEAD);
            while ((done == false) && (retVal == SDL_NORMAL))
            {
                char *name = ptr;
                char *comment;

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
                                {
                                    *nl = '\0';    /* Null-terminate comment */
                                }
                                else
                                {
                                    comment = NULL; /* Local comment, ignore */
                                }
                            }
                            else if (ii == _SDL_OUTPUT_COMMENT)
                            {
                                ptr = strchr(comment, '\0');
                            }
                            else
                            {
                                comment = NULL;    /* Local comment, ignore */
                            }
                        }
                        else
                        {
                            comment = NULL;
                        }
                        nl = name;
                        while (isalnum(*nl) || (*nl == '_') || (*nl == '$'))
                        {
                            nl++;
                        }
                        if (ii == _SDL_COMMA_)
                        {
                            ptr = (*nl == '\0') ? nl : (nl + 1);
                        }
                        *nl = '\0';        /* Null-terminate name */
                        ii = _SDL_COMMENT_LIST_NULL;
                    }
                    else
                    ii++;
                }
                if (strlen(name) > 0)
                {
                    if (myEnum == NULL)
                    {
                        if (freeTag == true)
                        {
                            tag = _sdl_get_tag(context,
                                               NULL,
                                               datatype,
                                               sdl_all_lower(id));
                        }
                        myConst = _sdl_create_constant(name,
                                                       prefix,
                                                       tag,
                                                       comment,
                                                       typeName,
                                                       radix,
                                                       value,
                                                       NULL,
                                                       size,
                                                       loc);
                        if (myConst != NULL)
                        {
                            retVal = _sdl_queue_constant(context, myConst);
                        }
                        else
                        {
                            retVal = SDL_ABORT;
                            if (sdl_set_message(msgVec,
                                                2,
                                                retVal,
                                                ENOMEM) != SDL_NORMAL)
                            {
                                retVal = SDL_ERREXIT;
                            }
                        }
                        if (freeTag == true)
                        {
                            sdl_free(tag);
                            tag = NULL;
                        }
                    }
                    else
                    {
                        SDL_ENUM_MEMBER *myMem = sdl_allocate_block(EnumMemberBlock,
                                                                    &myEnum->header,
                                                                    loc);

                        if (myMem != NULL)
                        {
                            myMem->id = id;
                            myMem->value = value;
                            myMem->valueSet = (value - prevValue) != 1;
                            SDL_INSQUE(&myEnum->members, &myMem->header.queue);
                        }
                    }
                }
                if ((retVal == SDL_NORMAL) &&
                    (counter != NULL) &&
                    (prevValue != value))
                {
                    (void) sdl_set_local(context, counter, value, loc);
                    prevValue = value;
                }
                if (incrementPresent == true)
                {
                    value += increment;
                }
                sdl_trim_str(ptr, SDL_M_LEAD);
                done = *ptr == '\0';
            }
        }

        /*
         * If we created an enumeration, then go complete it (this will output
         * it to the language output files).
         */
        if ((retVal == SDL_NORMAL) && (myEnum != NULL))
        {
            retVal = _sdl_enum_compl(context, myEnum);
        }
    }

    /*
     * Deallocate all the memory.
     */
    if (id != NULL)
    {
        sdl_free(id);
    }
    if (prefix != NULL)
    {
        sdl_free(prefix);
    }
    if (tag != NULL)
    {
        sdl_free(tag);
    }
    if ((counter != NULL) && (localCreated != SDL_CREATED))
    {
        sdl_free(counter);
    }
    if (typeName != NULL)
    {
        sdl_free(typeName);
    }
    if (enumName != NULL)
    {
        sdl_free(enumName);
    }

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
 *    A pointer to the context structure where we maintain information about
 *    the current parsing.
 *  name:
 *    A pointer to the string to be associated with this aggregate
 *    definition.
 *  datatype:
 *    A value to be associated with the datatype for this item.  If this is
 *    specified as one of the base type, then we have an implicit union.
 *  aggType:
 *    A value indicating if this AGGREGATE is for a UNION or STRUCTURE
 *    definition.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_ABORT:      An unexpected error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
uint32_t sdl_aggregate(SDL_CONTEXT *context,
                       char *name,
                       int64_t datatype,
                       int aggType,
                       SDL_YYLTYPE *loc)
{
    uint32_t retVal = SDL_NORMAL;

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL, then go ahead and perform the processing.
     */
    if (context->processingEnabled == true)
    {
        SDL_AGGREGATE    *myAggr = sdl_allocate_block(AggregateBlock,
                                                      NULL,
                                                      loc);

        /*
         * If tracing is turned on, write out this call (calls only, no
         * returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_aggregate ([%d:%d] to [%d:%d])\n",
                   __FILE__,
                   __LINE__,
                   loc->first_line,
                   loc->first_column,
                   loc->last_line,
                   loc->last_column);
        }

        if (myAggr != NULL)
        {
            myAggr->id = name;
            myAggr->typeID = context->aggregates.nextID++;
            myAggr->_unsigned = sdl_isUnsigned(context, &datatype);
            myAggr->type = datatype;
            if ((datatype >= SDL_K_TYPE_BYTE) && (datatype <= SDL_K_TYPE_OCTA))
            {
                myAggr->aggType = SDL_K_TYPE_UNION;    /* implicit union */
            }
            else
            {
                myAggr->aggType = aggType;
            }
            myAggr->tag = _sdl_get_tag(context,
                                       NULL,
                                       aggType,
                                       sdl_all_lower(name));
            SDL_Q_INIT(&myAggr->members);
            SDL_INSQUE(&context->aggregates.header, &myAggr->header.queue);
            context->currentAggr = myAggr;
            context->aggregateDepth++;
        }
        else
        {
            retVal = SDL_ABORT;
            if (sdl_set_message(msgVec,
                                2,
                                retVal,
                                ENOMEM) != SDL_NORMAL)
            {
                retVal = SDL_ERREXIT;
            }
        }
    }

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
 *    A pointer to the context structure where we maintain information about
 *    the current parsing.
 *  name:
 *    A pointer to the string containing the name for the AGGREGATE being
 *    defined.
 *  datatype:
 *    An integer indicating either a base type, user type, or another
 *    aggregate.
 *  aggType:
 *    A value indicating the type of subaggregate that is being requested to
 *    be created.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_ABORT:      An unexpected error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
uint32_t sdl_aggregate_member(SDL_CONTEXT *context,
                              char *name,
                              int64_t datatype,
                              int aggType,
                              SDL_YYLTYPE *loc,
                              bool lineComment,
                              bool startComment,
                              bool middleComment,
                              bool endComment)
{
    SDL_MEMBERS *myMember = NULL;
    SDL_AGGREGATE *myAggr = (context->aggregateDepth > 1 ?
                                NULL :
                                (SDL_AGGREGATE *) context->currentAggr);
    SDL_SUBAGGR *mySubAggr = (context->aggregateDepth > 1 ?
                                (SDL_SUBAGGR *) context->currentAggr :
                                NULL);
    int64_t subType = SDL_K_TYPE_BYTE;
    int64_t length = 0;
    int64_t tmpDatatype = abs(datatype);
    uint32_t retVal = SDL_NORMAL;
    bool bitfieldSized = false;
    bool mask = false;
    bool _signed = false;

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL, then go ahead and perform the processing.
     */
    if (context->processingEnabled == true)
    {

        /*
         * If tracing is turned on, write out this call (calls only, no returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_aggregate_member ([%d:%d] to [%d:%d])\n",
                   __FILE__,
                   __LINE__,
                   loc->first_line,
                   loc->first_column,
                   loc->last_line,
                   loc->last_column);
        }

        /*
         * Before we go too far, there may have been one or more options
         * defined to the previous member/aggregate/subaggregate.
         */
        if (context->optionsIdx > 0)
        {
            int64_t dimension;
            int ii;

            /*
             * Determine if the previous item we worked on was an ITEM, an
             * AGGREGATE, or a subaggregate.  If ITEM, that is the one that
             * needs to be updated with any saved options.  Otherwise, we
             * already have the thing that needs to have these options applied
             * to it.
             */
            if (mySubAggr != NULL)
            {
                if (SDL_Q_EMPTY(&mySubAggr->members) == false)
                {
                    myMember = (SDL_MEMBERS *) mySubAggr->members.blink;
                }
            }
            else
            {
                if (SDL_Q_EMPTY(&myAggr->members) == false)
                {
                    myMember = (SDL_MEMBERS *) myAggr->members.blink;
                }
            }

            /*
             * If the current member is a STRUCTURE or [IMPLICIT] UNION, then
             * it is the current aggregate, of which we already have the
             * address.
             */
            if ((myMember != NULL) && (sdl_isItem(myMember) == false))
            {
                myMember = NULL;
            }

            /*
             * Go find our options
             */
            for (ii = 0; ii < context->optionsIdx; ii++)
            {
                switch (context->options[ii].option)
                {

                    /*
                     * Present only options.
                     */
                    case Align:
                        if ((myMember != NULL) &&
                            (myMember->item.alignment != SDL_K_ALIGN))
                        {
                            myMember->item.alignment = SDL_K_ALIGN;
                            myMember->item.parentAlignment = false;
                        }
                        else if ((mySubAggr != NULL) &&
                                 (mySubAggr->alignment != SDL_K_ALIGN))
                        {
                            mySubAggr->alignment = SDL_K_ALIGN;
                            mySubAggr->parentAlignment = false;
                        }
                        else
                        {
                            myAggr->alignment = SDL_K_ALIGN;
                            myAggr->alignmentPresent = true;
                        }
                        break;

                    case Common:
                        if ((myAggr != NULL) && (myMember == NULL))
                        {
                            myAggr->commonDef = true;
                        }
                        break;

                    case Fill:
                        if (myMember != NULL)
                        {
                            myMember->item.fill = true;
                        }
                        else if (mySubAggr != NULL)
                        {
                            mySubAggr->fill = true;
                        }
                        else
                        {
                            myAggr->fill = true;
                        }
                        break;

                    case Global:
                        if ((myAggr != NULL) && (myMember == NULL))
                        {
                            myAggr->globalDef = true;
                        }
                        break;

                    case Mask:
                        mask = true;
                        break;

                    case NoAlign:
                        if ((myMember != NULL) &&
                            (myMember->item.alignment != SDL_K_NOALIGN))
                        {
                            myMember->item.alignment = SDL_K_NOALIGN;
                            myMember->item.parentAlignment = false;
                        }
                        else if ((mySubAggr != NULL) &&
                             (mySubAggr->alignment != SDL_K_NOALIGN))
                        {
                            mySubAggr->alignment = SDL_K_NOALIGN;
                            mySubAggr->parentAlignment = false;
                        }
                        else
                        {
                            myAggr->alignment = SDL_K_NOALIGN;
                            myAggr->alignmentPresent = true;
                        }
                        break;

                    case Typedef:
                        if (myMember != NULL)
                        {
                            myMember->item.typeDef = true;
                        }
                        else if (mySubAggr != NULL)
                        {
                            mySubAggr->typeDef = true;
                        }
                        else
                        {
                            myAggr->typeDef = true;
                        }
                        break;

                    case Signed:
                        _signed = true;
                        break;

                    /*
                     * String options.
                     */
                    case Based:
                        if ((myAggr != NULL) && (myMember == NULL))
                        {
                            myAggr->basedPtrName = context->options[ii].string;
                        }
                        else
                        {
                            sdl_free(context->options[ii].string);
                        }
                        context->options[ii].string = NULL;
                        break;

                    case Marker:
                        if ((mySubAggr != NULL) && (myMember == NULL))
                        {
                            if (mySubAggr->marker != NULL)
                            {
                                sdl_free(mySubAggr->marker);
                            }
                            mySubAggr->marker = context->options[ii].string;
                        }
                        else if (myMember == NULL)
                        {
                            if (myAggr->marker != NULL)
                            {
                                sdl_free(myAggr->marker);
                            }
                            myAggr->marker = context->options[ii].string;
                        }
                        else
                        {
                            sdl_free(context->options[ii].string);
                        }
                        context->options[ii].string = NULL;
                        break;

                    case Origin:
                        if ((myAggr != NULL) && (myMember == NULL))
                        {
                            myAggr->origin.id = context->options[ii].string;
                        }
                        else
                        {
                            sdl_free(context->options[ii].string);
                        }
                        context->options[ii].string = NULL;
                        break;

                    case Prefix:
                        if (myMember != NULL)
                        {
                            if (myMember->item.prefix != NULL)
                            {
                                sdl_free(myMember->item.prefix);
                            }
                            myMember->item.prefix = context->options[ii].string;
                        }
                        else if (mySubAggr != NULL)
                        {
                            if (mySubAggr->prefix != NULL)
                            {
                                sdl_free(mySubAggr->prefix);
                            }
                            mySubAggr->prefix = context->options[ii].string;
                        }
                        else
                        {
                            if (myAggr->prefix != NULL)
                            {
                                sdl_free(myAggr->prefix);
                            }
                            myAggr->prefix = context->options[ii].string;
                        }
                        context->options[ii].string = NULL;
                        break;

                    case Tag:
                        if (myMember != NULL)
                        {
                            if (myMember->item.tag != NULL)
                            {
                                sdl_free(myMember->item.tag);
                            }
                            myMember->item.tag = context->options[ii].string;
                            myMember->item.tagSet = true;
                        }
                        else if (mySubAggr != NULL)
                        {
                            if (mySubAggr->tag != NULL)
                            {
                                sdl_free(mySubAggr->tag);
                            }
                            mySubAggr->tag = context->options[ii].string;
                        }
                        else
                        {
                            if (myAggr->tag != NULL)
                            {
                                sdl_free(myAggr->tag);
                            }
                            myAggr->tag = context->options[ii].string;
                        }
                        context->options[ii].string = NULL;
                        break;

                    /*
                     * Numeric options.
                     */
                    case BaseAlign:
                        if ((myMember != NULL) &&
                            (myMember->item.alignment !=
                            context->options[ii].value))
                        {
                            myMember->item.alignment =
                            context->options[ii].value;
                            myMember->item.parentAlignment = false;
                        }
                        else if ((mySubAggr != NULL) &&
                                 (mySubAggr->alignment !=
                                     context->options[ii].value))
                        {
                            mySubAggr->alignment = context->options[ii].value;
                            mySubAggr->parentAlignment = false;
                        }
                        else
                        {
                            myAggr->alignment = context->options[ii].value;
                            myAggr->alignmentPresent = true;
                        }
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
                            myAggr->lbound =
                                context->dimensions[dimension].lbound;
                            myAggr->hbound =
                                context->dimensions[dimension].hbound;
                            myAggr->dimension = true;
                        }
                        context->dimensions[dimension].inUse = false;
                        break;

                    case Length:
                        length = context->options[ii].value;
                        break;

                    case SubType:
                        subType = context->options[ii].value;
                        bitfieldSized = true;
                        break;

                    default:
                        break;
                }
            }
        }

        /*
         * If the name is NULL, then we were just called to process the options
         * data.  Otherwise, we need to allocate another member for the current
         * aggregate.
         */
        if (name != NULL)
        {
            /*
             * OK, we took care of adding the options to our predecessor, so
             * now we need to start a new member.
             */
            myMember = sdl_allocate_block(AggrMemberBlock,
                                          (myAggr != NULL ?
                                              &myAggr->header :
                                              (SDL_HEADER *) mySubAggr),
                                          loc);
            if (myMember != NULL)
            {

                /*
                 * If we are at the AGGREGATE level, then we need to indicate
                 * so for this member.
                 */
                if (myAggr != NULL)
                {
                    myMember->header.top = true;
                }

                /*
                 * Determine if the member we are creating is a structure or
                 * union, and if a datatype was supplied, then we have an
                 * implicit union.
                 */
                if ((aggType == SDL_K_TYPE_STRUCT) ||
                    (aggType == SDL_K_TYPE_UNION))
                {
                    if ((tmpDatatype >= SDL_K_TYPE_BYTE) &&
                        (tmpDatatype <= SDL_K_TYPE_OCTA))
                    {
                        myMember->type = SDL_K_TYPE_UNION; /* implicit union */
                    }
                    else
                    {
                        myMember->type = aggType;
                    }
                }
                else
                {
                    myMember->type = tmpDatatype;
                }

                /*
                 * Process the information based on the type of member we are
                 * creating.
                 */
                switch (aggType)
                {
                    case SDL_K_TYPE_STRUCT:
                    case SDL_K_TYPE_UNION:
                        myMember->subaggr.id = name;
                        myMember->subaggr.aggType = myMember->type;
                        myMember->subaggr._unsigned = sdl_isUnsigned(context,
                                                                     &datatype);
                        myMember->subaggr.type = datatype;
                        myMember->subaggr.parent = context->currentAggr;
                        myMember->subaggr.self = myMember;
                        if (myAggr != NULL)
                        {
                            if (myAggr->prefix != NULL)
                            {
                                myMember->subaggr.prefix =
                                    sdl_strdup(myAggr->prefix);
                            }
                            if (myAggr->marker != NULL)
                            {
                                myMember->subaggr.marker =
                                    sdl_strdup(myAggr->marker);
                            }
                        }
                        else
                        {
                            if (mySubAggr->prefix != NULL)
                            {
                                myMember->subaggr.prefix =
                                    sdl_strdup(mySubAggr->prefix);
                            }
                            if (mySubAggr->marker != NULL)
                            {
                                myMember->subaggr.marker =
                                    sdl_strdup(mySubAggr->marker);
                            }
                        }
                        myMember->subaggr.tag = _sdl_get_tag(context,
                                                             NULL,
                                                             myMember->type,
                                                             sdl_all_lower(name));
                        if (myAggr != NULL)
                        {
                            myMember->subaggr.alignment = myAggr->alignment;
                        }
                        else
                        {
                            myMember->subaggr.alignment = mySubAggr->alignment;
                        }
                        myMember->subaggr.parentAlignment = true;
                        SDL_Q_INIT(&myMember->subaggr.members);
                        context->aggregateDepth++;
                        context->currentAggr = &myMember->subaggr;
                        break;

                    default:
                        if (datatype == SDL_K_TYPE_COMMENT)
                        {
                            myMember->comment.comment = sdl_strdup(name);
                            myMember->comment.endComment = endComment;
                            myMember->comment.lineComment = lineComment;
                            myMember->comment.middleComment = middleComment;
                            myMember->comment.startComment = startComment;
                        }
                        else
                        {
                            myMember->item.id = name;
                            myMember->item._unsigned = sdl_isUnsigned(context,
                                                                      &datatype);
                            myMember->item.type = datatype;
                            SDL_COPY_LOC(myMember->item.loc, &myMember->loc);
                        }
                        switch (datatype)
                        {
                            case SDL_K_TYPE_DECIMAL:
                                myMember->item. precision = context->precision;
                                myMember->item.scale = context->scale;
                                break;

                            case SDL_K_TYPE_BITFLD: /* only value from parser */
                                myMember->item.length = (length == 0 ? 1 : length);
                                myMember->item.mask = mask;
                                myMember->item._unsigned = _signed == false;
                                myMember->item.subType = subType;
                                myMember->item.sizedBitfield = bitfieldSized;
                                switch (subType)
                                {
                                    case SDL_K_TYPE_BYTE:
                                        myMember->item.type = SDL_K_TYPE_BITFLD_B;
                                        datatype = SDL_K_TYPE_BITFLD_B;
                                        break;

                                    case SDL_K_TYPE_WORD:
                                        myMember->item.type = SDL_K_TYPE_BITFLD_W;
                                        datatype = SDL_K_TYPE_BITFLD_W;
                                        break;

                                    case SDL_K_TYPE_LONG:
                                        myMember->item.type = SDL_K_TYPE_BITFLD_L;
                                        datatype = SDL_K_TYPE_BITFLD_L;
                                        break;

                                    case SDL_K_TYPE_QUAD:
                                        myMember->item.type = SDL_K_TYPE_BITFLD_Q;
                                        datatype = SDL_K_TYPE_BITFLD_Q;
                                        break;

                                    case SDL_K_TYPE_OCTA:
                                        myMember->item.type = SDL_K_TYPE_BITFLD_O;
                                        datatype = SDL_K_TYPE_BITFLD_O;
                                        break;

                                    default:
                                        break;
                                }
                                if (myMember->item.length < 0)
                                {
                                    retVal = SDL_ZEROLEN;
                                    if (sdl_set_message(msgVec,
                                                        1,
                                                        retVal,
                                                        myMember->item.id,
                                                        loc->first_line) != SDL_NORMAL)
                                    {
                                        retVal = SDL_ABORT;
                                    }
                                }
                                break;

                            case SDL_K_TYPE_CHAR:
                            case SDL_K_TYPE_CHAR_VARY:
                                myMember->item.length = length;
                                break;

                            case SDL_K_TYPE_CHAR_STAR:
                                retVal = SDL_INVUNKLEN;
                                if (sdl_set_message(msgVec,
                                                    1,
                                                    retVal,
                                                    loc->first_line) != SDL_NORMAL)
                                {
                                    retVal = SDL_ERREXIT;
                                }
                                break;

                            case SDL_K_TYPE_ADDR:
                            case SDL_K_TYPE_ADDR_L:
                            case SDL_K_TYPE_ADDR_Q:
                            case SDL_K_TYPE_ADDR_HW:
                            case SDL_K_TYPE_HW_ADDR:
                            case SDL_K_TYPE_PTR:
                            case SDL_K_TYPE_PTR_L:
                            case SDL_K_TYPE_PTR_Q:
                            case SDL_K_TYPE_PTR_HW:
                                myMember->item.subType = subType;
                                if ((subType >= SDL_K_AGGREGATE_MIN) &&
                                    (subType <= SDL_K_AGGREGATE_MAX))
                                {
                                    SDL_AGGREGATE *lclAggr;

                                    lclAggr = sdl_get_aggregate(&context->aggregates,
                                                                subType);
                                    if ((lclAggr != NULL) &&
                                        (lclAggr->basedPtrName == NULL))
                                    {
                                        retVal = SDL_ADROBJBAS;
                                        if (sdl_set_message(msgVec,
                                                            1,
                                                            retVal,
                                                            lclAggr->id,
                                                            loc->first_line) != SDL_NORMAL)
                                        {
                                            retVal = SDL_ERREXIT;
                                        }
                                    }
                                }
                                break;
                        }
                        if (sdl_isComment(myMember) == false)
                        {
                            int tagDatatype = datatype;

                            if ((myAggr != NULL) && (myAggr->prefix != NULL))
                            {
                                myMember->item.prefix =
                                    sdl_strdup(myAggr->prefix);
                            }
                            else if ((mySubAggr != NULL) &&
                                     (mySubAggr->prefix != NULL))
                            {
                                myMember->item.prefix =
                                    sdl_strdup(mySubAggr->prefix);
                            }
                            switch (datatype)
                            {
                                case SDL_K_TYPE_BITFLD_B:
                                case SDL_K_TYPE_BITFLD_W:
                                case SDL_K_TYPE_BITFLD_L:
                                case SDL_K_TYPE_BITFLD_Q:
                                case SDL_K_TYPE_BITFLD_O:
                                    if (myMember->item.sizedBitfield == false)
                                    {
                                        tagDatatype = SDL_K_TYPE_BITFLD;
                                    }
                                    break;

                                default:
                                    break;
                            }
                            myMember->item.tag = _sdl_get_tag(context,
                                                              NULL,
                                                              tagDatatype,
                                                              sdl_all_lower(name));
                            myMember->item.size = sdl_sizeof(context, datatype);

                            /*
                             * Varying text is the maximum length of the string
                             * plus 2 bytes for the length fields.  We do this
                             * differently from other datatypes because of the
                             * way this is handled internally.
                             */
                            if (myMember->type == SDL_K_TYPE_CHAR_VARY)
                            {
                                myMember->item.size += myMember->item.length;
                            }
                            if (myAggr != NULL)
                            {
                                myMember->item.alignment = myAggr->alignment;
                            }
                            else
                            {
                                myMember->item.alignment = mySubAggr->alignment;
                            }
                            myMember->item.parentAlignment = true;
                        }
                    break;
                }
                if (sdl_isComment(myMember) == false)
                {
                    _sdl_checkAndSetOrigin(context, myMember);
                }
                if (mySubAggr != NULL)
                {
                    _sdl_determine_offsets(context,
                                           myMember,
                                           &mySubAggr->members,
                                           (mySubAggr->aggType == SDL_K_TYPE_UNION));
                    SDL_INSQUE(&mySubAggr->members, &myMember->header.queue);
                }
                else
                {
                    _sdl_determine_offsets(context,
                                           myMember,
                                           &myAggr->members,
                                           (myAggr->aggType == SDL_K_TYPE_UNION));
                    SDL_INSQUE(&myAggr->members, &myMember->header.queue);
                }
            }
            else
            {
                retVal = SDL_ABORT;
                if (sdl_set_message(msgVec,
                                    2,
                                    retVal,
                                    ENOMEM) != SDL_NORMAL)
                {
                    retVal = SDL_ERREXIT;
                }
            }
        }
        else
        {
            retVal = SDL_INVAGGRNAM;
            if (sdl_set_message(msgVec,
                                1,
                                retVal) != SDL_NORMAL)
            {
                retVal = SDL_ERREXIT;
            }
        }
    }

   /*
    * Return the results of this call back to the caller.
    */
    _sdl_reset_options(context);
   return (retVal);
}

/*
 * sdl_aggregate_compl
 *  This function is called to complete the definition of an aggregate
 *  definition.
 *
 * Input Parameters:
 *  context:
 *    A pointer to the context structure where we maintain information about
 *    the current parsing.
 *  name:
 *    A pointer to the string to be associated with this aggregate
 *    definition.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_MATCHEND:   Specified name does not match AGGREGATE name.
 *  SADL_NULLSTRUCT: An aggregate with no members was defined.
 *  SDL_ABORT:      An unexpected error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
uint32_t sdl_aggregate_compl(SDL_CONTEXT *context, char *name, SDL_YYLTYPE *loc)
{
    SDL_AGGREGATE *myAggr = (SDL_AGGREGATE *) context->currentAggr;
    SDL_SUBAGGR *mySubAggr = (SDL_SUBAGGR *) context->currentAggr;
    uint32_t retVal = SDL_NORMAL;

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL, then go ahead and perform the processing.
     */
    if (context->processingEnabled == true)
    {

        /*
         * If tracing is turned on, write out this call (calls only, no
         * returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_aggregate_compl ([%d:%d] to [%d:%d])\n",
                   __FILE__,
                   __LINE__,
                   loc->first_line,
                   loc->first_column,
                   loc->last_line,
                   loc->last_column);
        }

        /*
         * If we have any options that had been processed, they are for the
         * most recent ITEM member.
         */
        if (context->optionsIdx > 0)
        {
            SDL_MEMBERS *myMember = NULL;
            int64_t dimension;
            int ii;

            /*
             * OK, the issue is where is the most recent ITEM member.  It is in
             * the current aggregate.  Don't assume that the queues actually
             * contain anything, and make sure that the member is actually not
             * a subaggregate.
             */
            if (context->aggregateDepth == 1)
            {
                if (SDL_Q_EMPTY(&myAggr->members) == false)
                {
                    myMember = myAggr->members.blink;
                }
            }
            else
            {
                if (SDL_Q_EMPTY(&mySubAggr->members) == false)
                {
                    myMember = mySubAggr->members.blink;
                }
            }
            if ((myMember != NULL) && (sdl_isItem(myMember) == true))
            {
                myMember = NULL;
            }
            for (ii = 0; ii < context->optionsIdx; ii++)
            {
                switch (context->options[ii].option)
                {

                    /*
                     * Present only options.
                     */
                    case Align:
                        if (myMember != NULL)
                        {
                            myMember->item.alignment = SDL_K_ALIGN;
                        }
                        break;

                    case Fill:
                        if (myMember != NULL)
                        {
                            myMember->item.fill = true;
                        }
                        break;

                    case Mask:
                        if (myMember != NULL)
                        {
                            myMember->item.mask = true;
                        }
                        break;

                    case NoAlign:
                        if (myMember != NULL)
                        {
                            myMember->item.alignment = SDL_K_NOALIGN;
                        }
                        break;

                    case Signed:
                        if (myMember != NULL)
                        {
                            myMember->item._unsigned = false;
                        }
                        break;

                    case Typedef:
                        if (myMember != NULL)
                        {
                            myMember->item.typeDef = true;
                        }
                        break;

                    /*
                     * String options.
                     */
                    case Prefix:
                        if (myMember != NULL)
                        {
                            myMember->item.prefix = context->options[ii].string;
                        }
                        else
                        {
                            sdl_free(context->options[ii].string);
                        }
                        context->options[ii].string = NULL;
                        break;

                    case Tag:
                        if (myMember != NULL)
                        {
                            myMember->item.tag = context->options[ii].string;
                        }
                        else
                        {
                            sdl_free(context->options[ii].string);
                        }
                        context->options[ii].string = NULL;
                        break;

                    /*
                     * Numeric options.
                     */
                    case BaseAlign:
                        if (myMember != NULL)
                        {
                            myMember->item.alignment = context->options[ii].value;
                        }
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
                        context->dimensions[dimension].inUse = false;
                        break;

                    case Length:
                        if (myMember != NULL)
                        {
                            myMember->item.length = context->options[ii].value;
                        }
                        break;

                    default:
                        break;
                }
            }
        }

        /*
         * We are completing either a subaggregate or an AGGREGATE.  Decrement
         * the depth.
         */
        context->aggregateDepth--;

        /*
         * If we are at zero, then we have completed the entire AGGREGATE
         * definition and can now write out the results.
         */
        if (context->aggregateDepth == 0)
        {
            SDL_AGGREGATE *myAggr =
                (SDL_AGGREGATE *) context->aggregates.header.blink;

            /*
             * We no longer have a current aggregate.  Also, determine the
             * actual size of the aggregate.
             */
            context->currentAggr = NULL;
            myAggr->size = _sdl_aggregate_size(context, myAggr, NULL);
            if ((name != NULL) && (strcmp(myAggr->id, name) != 0))
            {
                retVal = SDL_MATCHEND;
                if (sdl_set_message(msgVec,
                                    1,
                                    retVal,
                                    myAggr->id,
                                    loc->first_line) != SDL_NORMAL)
                {
                    retVal = SDL_ERREXIT;
                }
            }
            else if (SDL_Q_EMPTY(&myAggr->members) == true)
            {
                retVal = SDL_NULLSTRUCT;
                if (sdl_set_message(msgVec,
                                    1,
                                    retVal,
                                    myAggr->id,
                                    myAggr->loc.first_line) != SDL_NORMAL)
                {
                    retVal = SDL_ERREXIT;
                }
            }

            if (retVal == SDL_NORMAL)
            {
                retVal = sdl_call_aggregate(context->langEnableVec,
                                            myAggr,
                                            LangAggregate,
                                            false,
                                            0,
                                            context);
            }
            if ((retVal == SDL_NORMAL) &&
                (SDL_Q_EMPTY(&myAggr->members) == false))
            {
                retVal = _sdl_iterate_members(context,
                                              myAggr->members.flink,
                                              &myAggr->members,
                                              (int (*)()) &_sdl_aggregate_callback,
                                              1,
                                              1);
            }

            if (retVal == SDL_NORMAL)
            {
                retVal = sdl_call_aggregate(context->langEnableVec,
                                            myAggr,
                                            LangAggregate,
                                            true,
                                            0,
                                            context);
            }
        }

        /*
         * We just closed a subaggregate.  Determine the size of the
         * subaggregate and make the previous AGGREGATE or subaggregate the
         * current one.
         */
        else
        {
            context->currentAggr = mySubAggr->parent;
            mySubAggr->size = _sdl_aggregate_size(context, NULL, mySubAggr);
            if ((name != NULL) && (strcmp(mySubAggr->id, name) != 0))
            {
                retVal = SDL_MATCHEND;
                if (sdl_set_message(msgVec,
                                    1,
                                    retVal,
                                    mySubAggr->id,
                                    loc->first_line) != SDL_NORMAL)
                {
                    retVal = SDL_ERREXIT;
                }
            }
            else if (SDL_Q_EMPTY(&mySubAggr->members) == true)
            {
                retVal = SDL_NULLSTRUCT;
                if (sdl_set_message(msgVec,
                                    1,
                                    retVal,
                                    mySubAggr->id,
                                    loc->first_line) != SDL_NORMAL)
                {
                    retVal = SDL_ERREXIT;
                }
            }
        }
    }

    /*
     * Return the results of this call back to the caller.
     */
    _sdl_reset_options(context);
    return (retVal);
}

/*
 * sdl_entry
 *  This function is called to create the ENTRY structure that will contain
 *  all the information about the function/procedure definition being defined.
 *
 * Input Parameters:
 *  context:
 *    A pointer to the context structure where we maintain information about
 *    the current parsing.
 *  name:
 *    A pointer to the string to be associated with this entry definition.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_ABORT:      An unexpected error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
uint32_t sdl_entry(SDL_CONTEXT *context, char *name, SDL_YYLTYPE *loc)
{
    SDL_ENTRY *myEntry = sdl_allocate_block(EntryBlock, NULL, loc);
    uint32_t retVal = SDL_NORMAL;
    int ii;

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL, then go ahead and perform the processing.
     */
    if (context->processingEnabled == true)
    {

        /*
         * If tracing is turned on, write out this call (calls only, no
         * returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_entry\n", __FILE__, __LINE__);
        }

        if (myEntry != NULL)
        {
            myEntry->id = name;
            SDL_Q_INIT(&myEntry->parameters);
            for (ii = 0; ii < context->optionsIdx; ii++)
            {
                switch(context->options[ii].option)
                {
                    case Alias:
                        myEntry->alias = context->options[ii].string;
                        context->options[ii].string = NULL;
                        break;

                    case Linkage:
                        myEntry->linkage = context->options[ii].string;
                        context->options[ii].string = NULL;
                        break;

                    case TypeName:
                        myEntry->typeName = context->options[ii].string;
                        context->options[ii].string = NULL;
                        break;

                    case Variable:
                        myEntry->variable = true;
                        break;

                    case ReturnsType:
                        myEntry->returns.type = context->options[ii].value;
                        myEntry->returns._unsigned = sdl_isUnsigned(context,
                                                                    &myEntry->returns.type);
                        break;

                    case ReturnsNamed:
                        myEntry->returns.name = context->options[ii].string;
                        context->options[ii].string = NULL;
                        break;

                    default:
                        break;
                }
            }
            for (ii = 0; ii < context->parameterIdx; ii++)
            {
                SDL_PARAMETER *myParam = context->parameters[ii];

                context->parameters[ii] = NULL;
                myParam->header.parent = &myEntry->header;
                SDL_INSQUE(&myEntry->parameters, &myParam->header.queue);
            }
            context->parameterIdx = 0;
            SDL_INSQUE(&context->entries, &myEntry->header.queue);

            if (retVal == SDL_NORMAL)
            {
                retVal = sdl_call_entry(context->langEnableVec,
                                        myEntry,
                                        context);
            }
        }
        else
        {
            retVal = SDL_ABORT;
            if (sdl_set_message(msgVec,
                                2,
                                retVal,
                                ENOMEM) != SDL_NORMAL)
            {
                retVal = SDL_ERREXIT;
            }
        }
    }

    /*
     * Return the results of this call back to the caller.
     */
    _sdl_reset_options(context);
    return (retVal);
}

/*
 * sdl_add_parameter
 *  This function is called when a parameter needs to be added to an entry.
 *  We do this in two passes.  The first creates the parameter records, then
 *  when creating the ENTRY record, these are copied into there and the array
 *  cleaned up.  The list in the context block is used as an array of pointers
 *  to PARAMETER definitions.
 *
 * Input Parameters:
 *  context:
 *    A pointer to the context structure where we maintain information about
 *    the current state of the parsing.
 *  datatype:
 *    An integer indicating either a base type or a user type.  If a base,
 *    get the default.  If a user, we may need to call ourselves again to
 *    get what we came to get.
 *  passing:
 *    A value indicating how a parameter is passed (by reference or value).
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_ABORT:      An unexpected error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
uint32_t sdl_add_parameter(
        SDL_CONTEXT *context,
        int64_t datatype,
        int passing,
        SDL_YYLTYPE *loc)
{
    uint32_t retVal = SDL_NORMAL;
    int ii;

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL, then go ahead and perform the processing.
     */
    if (context->processingEnabled == true)
    {

        /*
         * If tracing is turned on, write out this call (calls only, no
         * returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_add_parameter\n", __FILE__, __LINE__);
        }

        /*
         * If the stack is full, reallocate a larger stack.
         */
        if (context->parameterIdx >= context->parameterSize)
        {
            size_t    size;

            context->parameterSize += SDL_K_OPTIONS_INCR;
            size = context->parameterSize * sizeof(SDL_PARAMETER *);
            context->parameters = sdl_realloc(context->parameters, size);
        }

        if (context->parameters != NULL)
        {
            SDL_PARAMETER *param = sdl_allocate_block(ParameterBlock,
                                                      NULL,
                                                      loc);

            param->_unsigned = sdl_isUnsigned(context, &datatype);
            param->type = datatype;
            param->passingMech = passing;

            for (ii = 0; ii < context->optionsIdx; ii++)
            {
                switch (context->options[ii].option)
                {
                    case In:
                        param->in = true;
                        context->options[ii].option = None;;
                        break;

                    case Out:
                        param->out = true;
                        context->options[ii].option = None;;
                        break;

                    case Named:
                        param->name = context->options[ii].string;
                        context->options[ii].option = None;;
                        context->options[ii].string = NULL;
                        break;

                    case Dimension:
                        param->bound = context->options[ii].value;
                        param->dimension = true;
                        context->options[ii].option = None;;
                        break;

                    case Default:
                        param->defaultValue = context->options[ii].value;
                        param->defaultPresent = true;
                        context->options[ii].option = None;;
                        break;

                    case TypeName:
                        param->typeName = context->options[ii].string;
                        context->options[ii].option = None;;
                        context->options[ii].string = NULL;
                        break;

                    case Optional:
                        param->optional = true;
                        context->options[ii].option = None;;
                        break;

                    case List:
                        param->list = true;
                        context->options[ii].option = None;;
                        break;

                    default:
                        break;
                }
            }
            context->parameters[context->parameterIdx++] = param;
        }
        else
        {
            retVal = SDL_ABORT;
            if (sdl_set_message(msgVec,
                                2,
                                retVal,
                                ENOMEM) != SDL_NORMAL)
            {
                retVal = SDL_ERREXIT;
            }
        }
    }

    /*
     * Return the results of this call back to the caller.
     */
    return (retVal);
}

/*
 * sdl_conditional
 *  This function is called when one of the conditional statements has been
 *  parsed and needs to be processed.  The IFSYMBOL turns off and on processing
 *  of the file.  This will actually prevent the processing of anything coming
 *  from the parser.  IFLANGUAGE will turn off and on the outputting of
 *  definitions for a particular language.
 *
 *  There are only certain valid conditional state transitions.  They are as
 *  follows:
 *
 *    CondNone    --> CondIfLang      IFLANGUAGE
 *    CondNone    --> CondIfSymb      IFSYMBOL
 *    CondIfLang  --> CondIfSymb      IFSYMBOL after IFLANGUAGE
 *    CondIfLang  --> CondIfLang      IFLANGUAGE after IFLANGUAGE
 *    CondIfLang  --> CondElse        ELSE after IFLANGUAGE
 *    CondIfLang  --> CondNone        END_IFLANGUAGE after IFLANGUAGE
 *    CondIfSymb  --> CondIfLang      IFLANGUAGE after IFSYMBOL
 *    CondIfSymb  --> CondElseIf      ELSE_IFSYMBOL after IFSYMBOL
 *    CondIfSymb  --> CondElse        ELSE after IFSYMBOL
 *    CondIfSymb  --> CondNone        END_IFSYMBOL after IFSYMBOL
 *    CondElseIf  --> CondElse        ELSE after ELSE_IFSYMBOL
 *    CondElseIf  --> CondIfLang      IFLANGUAGE after ELSE_IFSYMBOL
 *    CondElseIf  --> CondNone        END_IFSYMBOL after ELSE_IFSYMBOL
 *    CondElse    --> CondIfLang      IFLANUGAGE after ELSE
 *    CondElse    --> CondIfSymb      IFSYMBOL after ELSE
 *    CondElse    --> CondNone        END_IFLANGUAGE or END_IFSYMBOL
 *
 *
 * Input Parameters:
 *  context:
 *    A pointer to the context structure where we maintain information about
 *    the current state of the parsing.
 *  conditional:
 *    An integer indicating the type of conditional.
 *  expr:
 *    A pointer to the actual conditions for the conditional.  For
 *    IFLANGUAGE, this will be a pointer to a structure that contains the
 *    list of languages specified.  For IFSYMBOL, this will be the singular
 *    string for the conditional.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_SYMNOTDEF:  The symbol specified has not been defined.
 *  SDL_INVCONDST:  Invalid condition state.
 *  SDL_ABORT:      An unexpected error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
uint32_t sdl_conditional(
        SDL_CONTEXT *context,
        int conditional,
        void *expr,
        SDL_YYLTYPE *loc)
{
    SDL_ARGUMENTS *args = context->argument;
    char *symbol = (char *) expr;
    SDL_LANGUAGE_LIST *langs = (SDL_LANGUAGE_LIST *) expr;
    uint32_t retVal = SDL_NORMAL;
    int ii, jj;
    bool done = false;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:sdl_conditional\n", __FILE__, __LINE__);
    }

    /*
     * Perform the processing based on the conditional provided.
     */
    switch(conditional)
    {

        /*
         * Process an IFSYMBOL <symbol> statement.
         */
        case SDL_K_COND_SYMB:

            /*
             * This state transition is only valid when we are in one of the
             * following conditional states:
             *
             *      CondNone
             *      CondIfLang
             *      CondElse
             */
            if ((SDL_CUR_COND_STATE(context) == CondNone) ||
                (SDL_CUR_COND_STATE(context) == CondIfLang) ||
                (SDL_CUR_COND_STATE(context) == CondElse))
            {
                SDL_PUSH_COND_STATE(context, CondIfSymb);

                /*
                 * Now we need to determine if the supplied symbol is turning
                 * on or off the continued processing of the parsed file.
                 */
                for (ii = 0;
                     ((ii < args[ArgSymbols].symbol->listUsed) && (done == false));
                     ii++)
                {
                    if (strcmp(symbol,
                               args[ArgSymbols].symbol->symbols[ii].symbol) == 0)
                    {
                        if (args[ArgSymbols].symbol->symbols[ii].value == 0)
                        {
                            if (context->processingEnabled == true)
                            {
                                context->processingEnabled = false;
                            }
                        }
                        else
                        {
                            if (context->processingEnabled == false)
                            {
                                context->processingEnabled = true;
                            }
                        }
                        done = true;
                    }
                }
                if (done == false)
                {
                    retVal = SDL_SYMNOTDEF;
                    if (sdl_set_message(msgVec,
                                        1,
                                        retVal,
                                        symbol,
                                        loc->first_line) != SDL_NORMAL)
                    retVal = SDL_ERREXIT;
                }
            }
            else
            {
                retVal = SDL_INVCONDST;
                if (sdl_set_message(msgVec,
                                    1,
                                    retVal,
                                    loc->first_line) != SDL_NORMAL)
                {
                    retVal = SDL_ERREXIT;
                }
            }
            break;

        /*
         * Process an IFLANGUAGE <language>[, <language>] statement.
         */
        case SDL_K_COND_LANG:

            /*
             * If processing is not turned off because of an IFSYMBOL..
             * ELSE_IFSYMBOL..ELSE..END_IFSYMBOL, then go ahead and perform the
             * processing.
             */
            if (context->processingEnabled == true)
            {

                /*
                 * This state transition is only valid when we are in one of
                 * the following conditional states:
                 *
                 *      CondNone
                 *      CondIfLang
                 *      CondIfSymb
                 *      ElseIf
                 *      Else
                 */
                if ((SDL_CUR_COND_STATE(context) == CondNone) ||
                    (SDL_CUR_COND_STATE(context) == CondIfLang) ||
                    (SDL_CUR_COND_STATE(context) == CondIfSymb) ||
                    (SDL_CUR_COND_STATE(context) == CondElseIf) ||
                    (SDL_CUR_COND_STATE(context) == CondElse))
                {
                    SDL_PUSH_COND_STATE(context, CondIfLang);

                    /*
                     * Now we need to determine if we are turning on or off any
                     * particular languages.  The first thing we need to do is
                     * turn all languages off and if one is specified in the
                     * langs list, then we'll turn it back on.
                     */
                    for (ii = 0; ii < context->languagesSpecified; ii++)
                    {
                        context->langEnableVec[ii] = false;
                    }
                    for (ii = 0; ii < langs->listUsed; ii++)
                    {
                        for (jj = 0; jj < context->languagesSpecified; jj++)
                        {
                            if (strcasecmp(langs->lang[ii],
                                           args[ArgLanguage].languages[jj].langStr) == 0)
                            {
                                context->langEnableVec[jj] = true;
                            }
                        }
                    }
                }
                else
                {
                    retVal = SDL_INVCONDST;
                    if (sdl_set_message(msgVec,
                                        1,
                                        retVal,
                                        loc->first_line) != SDL_NORMAL)
                    {
                        retVal = SDL_ERREXIT;
                    }
                }
            }
            langs->listUsed = 0;
            break;

        /*
         * Process an ELSE_IFSYMBOL <symbol> statement.
         */
        case SDL_K_COND_ELSEIF:

            /*
             * This state transition is only valid when we are in one of the
             * following conditional states:
             *
             *        CondIfSymb
             */
            if ((SDL_CUR_COND_STATE(context) == CondIfSymb))
            {
                SDL_POP_COND_STATE(context);
                SDL_PUSH_COND_STATE(context, CondElseIf);

                /*
                 * Now we need to determine if the supplied symbol is turning
                 * on or off the continued processing of the parsed file.
                 */
                for (ii = 0;
                     ((ii < args[ArgSymbols].symbol->listUsed) && (done == false));
                     ii++)
                {
                    if (strcmp(symbol,
                               args[ArgSymbols].symbol->symbols[ii].symbol) == 0)
                    {
                    if (args[ArgSymbols].symbol->symbols[ii].value == 0)
                    {
                        if (context->processingEnabled == true)
                        {
                            context->processingEnabled = false;
                        }
                    }
                    else
                    {
                        if (context->processingEnabled == false)
                        {
                            context->processingEnabled = true;
                        }
                    }
                    done = true;
                    }
                }
            }
            else
            {
                retVal = SDL_INVCONDST;
                if (sdl_set_message(msgVec,
                                    1,
                                    retVal,
                                    loc->first_line) != SDL_NORMAL)
                {
                    retVal = SDL_ERREXIT;
                }
            }
            break;

        /*
         * Process an ELSE statement.
         */
        case SDL_K_COND_ELSE:

            /*
             * This state transition is only valid when we are in one of the
             * following conditional states:
             *
             *      CondIfLang
             *      CondIfSymb
             *      CondElseIf
             */
            if ((SDL_CUR_COND_STATE(context) == CondIfLang) &&
                (context->processingEnabled == true))
            {
                SDL_POP_COND_STATE(context);
                SDL_PUSH_COND_STATE(context, CondElse);

                for (ii = 0; ii < context->languagesSpecified; ii++)
                {
                    if (context->langEnableVec[ii] == true)
                    {
                        context->langEnableVec[ii] = false;
                    }
                    else
                    {
                        context->langEnableVec[ii] = true;
                    }
                }
            }
            else if ((SDL_CUR_COND_STATE(context) == CondIfSymb) ||
                     (SDL_CUR_COND_STATE(context) == CondElseIf))
            {
                SDL_POP_COND_STATE(context);
                SDL_PUSH_COND_STATE(context, CondElse);
                context->processingEnabled = !context->processingEnabled;
            }
            else if (context->processingEnabled == true)
            {
                retVal = SDL_INVCONDST;
                if (sdl_set_message(msgVec,
                                    1,
                                    retVal,
                                    loc->first_line) != SDL_NORMAL)
                {
                    retVal = SDL_ERREXIT;
                }
            }
            break;

        /*
         * Process an END_IFSYMBOL statement.
         */
        case SDL_K_COND_END_SYMB:

            /*
             * This state transition is only valid when we are in one of the
             * following conditional states:
             *
             *      CondIfSymb
             *      CondElseIf
             *      CondElse
             */
            if ((SDL_CUR_COND_STATE(context) == CondIfSymb) ||
                (SDL_CUR_COND_STATE(context) == CondElseIf) ||
                (SDL_CUR_COND_STATE(context) == CondElse))
            {
                SDL_POP_COND_STATE(context);
                context->processingEnabled = true;
            }
            else
            {
                retVal = SDL_INVCONDST;
                if (sdl_set_message(msgVec,
                                    1,
                                    retVal,
                                    loc->first_line) != SDL_NORMAL)
                {
                    retVal = SDL_ERREXIT;
                }
            }
            break;

        /*
         * Process an END_IFSYMBOL statement.
         */
        case SDL_K_COND_END_LANG:

            /*
             * If processing is not turned off because of an IFSYMBOL..
             * ELSE_IFSYMBOL..ELSE..END_IFSYMBOL, then go ahead and perform the
             * processing.
             */
            if (context->processingEnabled == true)
            {

                /*
                 * This state transition is only valid when we are in one of
                 * the following conditional states:
                 *
                 *      CondIfLang
                 *      CondElse
                 */
                if ((SDL_CUR_COND_STATE(context) == CondIfLang) ||
                    (SDL_CUR_COND_STATE(context) == CondElse))
                {
                    SDL_POP_COND_STATE(context);
                    for (ii = 0; ii < context->languagesSpecified; ii++)
                    {
                        context->langEnableVec[ii] = true;
                    }
                }
                else
                {
                    retVal = SDL_INVCONDST;
                    if (sdl_set_message(msgVec,
                                        1,
                                        retVal,
                                        loc->first_line) != SDL_NORMAL)
                    {
                        retVal = SDL_ERREXIT;
                    }
                }
            }
            if (langs != NULL)
            {
                langs->listUsed = 0;
            }
            break;

        default:
            retVal = SDL_INVCONDST;
            if (sdl_set_message(msgVec,
                                1,
                                retVal,
                                loc->first_line) != SDL_NORMAL)
            {
                retVal = SDL_ERREXIT;
            }
            break;
    }

    /*
     * Return the results of this call back to the caller.
     */
    sdl_free(expr);
    return (retVal);
}

/*
 * sdl_add_language
 *  This function is called to add a language to the list of languages
 *  specified on an IFLANGUAGE or END_IFLANGUAGE statements.
 *
 * Input Parameters:
 *  context:
 *    A pointer to the context structure where we maintain information about
 *    the current state of the parsing.
 *  langStr:
 *    A pointer to a string containing the language specifier to be added to
 *    the list of language specifiers currently in process.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_ABORT:      An unexpected error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
uint32_t sdl_add_language(SDL_CONTEXT *context, char *langStr, SDL_YYLTYPE *loc)
{
    uint32_t    retVal = SDL_NORMAL;

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL, then go ahead and perform the processing.
     */
    if (context->processingEnabled == true)
    {

        /*
         * If tracing is turned on, write out this call (calls only, no
         * returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_add_language\n", __FILE__, __LINE__);
        }

        if (langStr != NULL)
        {
            if (context->langCondList.listUsed == context->langCondList.listSize)
            {
                context->langCondList.lang = sdl_realloc(context->langCondList.lang,
                                                         ++context->langCondList.listSize);
            }
            if (context->langCondList.lang != NULL)
            {
                strcpy(context->langCondList.lang[context->langCondList.listUsed++],
                       langStr);
            }
            else
            {
                retVal = SDL_ABORT;
                if (sdl_set_message(msgVec,
                                    2,
                                    retVal,
                                    ENOMEM) != SDL_NORMAL)
                {
                    retVal = SDL_ERREXIT;
                }
            }
        }
        else
        {
            retVal = SDL_ABORT;
            if (sdl_set_message(msgVec,
                                1,
                                retVal) != SDL_NORMAL)
            {
                retVal = SDL_ERREXIT;
            }
        }
    }

    /*
     * Return the results of this call back to the caller.
     */
    sdl_free(langStr);
    return (retVal);
}

/*
 * sdl_get_language
 *  This function is called to get all the languages specified on an IFLANGUAGE
 *  or END_IFLANGUAGE statements.
 *
 * Input Parameters:
 *  context:
 *    A pointer to the context structure where we maintain information about
 *    the current state of the parsing.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  !NULL:          Normal Successful Completion.
 *  NULL:           No languages specified.
 */
void *sdl_get_language(SDL_CONTEXT *context, SDL_YYLTYPE *loc)
{
    void    *retVal = NULL;

    /*
     * If processing is not turned off because of an IFSYMBOL..ELSE_IFSYMBOL..
     * ELSE..END_IFSYMBOL, then go ahead and perform the processing.
     */
    if (context->processingEnabled == true)
    {

        /*
         * If tracing is turned on, write out this call (calls only, no
         * returns).
         */
        if (trace == true)
        {
            printf("%s:%d:sdl_get_language\n", __FILE__, __LINE__);
        }

        if (context->langCondList.listUsed > 0)
        {
            retVal = &context->langCondList;
        }
    }

    /*
     * Return the results of this call back to the caller.
     */
    return (retVal);
}

/************************************************************************/
/* Local Functions                            */
/************************************************************************/

/*
 * _sdl_aggregate_callback
 *  This function is called to iterate through each of the languages for a
 *  particular member of an AGGREGATE.
 *
 * Input Parameters:
 *  context:
 *    A pointer to the context structure where we maintain information about
 *    the current state of the parsing.
 *  member:
 *    A pointer to the member item needing to be written out.
 *  ending:
 *      A boolean value indicating that we are ending a subaggregate.
 *  depth:
 *    A value indicating the depth of the member.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_ABORT:      An unexpected  error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
static uint32_t _sdl_aggregate_callback(
            SDL_CONTEXT *context,
            SDL_MEMBERS *member,
            bool ending,
            int depth)
{
    void *param;
    SDL_LANG_AGGR_TYPE type;
    uint32_t retVal = SDL_NORMAL;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:_sdl_aggregate_callback\n", __FILE__, __LINE__);
    }

    if (sdl_isItem(member) == false)
    {
        param = (void *) &member->subaggr;
        type = LangSubaggregate;
    }
    else if (sdl_isComment(member) == true)
    {
        param = (void *) &member->comment;
        type = LangComment;
    }
    else
    {
        param = (void *) &member->item;
        type = LangItem;
    }

    retVal = sdl_call_aggregate(context->langEnableVec,
                                 param,
                                 type,
                                 ending,
                                 depth,
                                 context);

    /*
     * Return the results of this call back to the caller.
     */
    return (retVal);
}

/*
 * _sdl_get_declare
 *  This function is called to get the record of a previously defined local
 *  variable.  If the local variable has not yet been declared, then a NULL
 *  will be returned.  Otherwise, the a pointer to the found DECLARE will be
 *  returned to the caller.
 *
 * Input Parameters:
 *  declare:
 *    A pointer to the list containing all currently defined DECLAREs.
 *  name:
 *    A pointer to the name of the declared user-type.
 *
 * Output Parameters:
 *  None.
 *
 * Return Value
 *  NULL:           Local variable not found.
 *  !NULL:          An existing local variable.
 */
static SDL_DECLARE *_sdl_get_declare(SDL_DECLARE_LIST *declare, char *name)
{
    SDL_DECLARE *retVal = (SDL_DECLARE *) declare->header.flink;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:_sdl_get_declare\n", __FILE__, __LINE__);
    }

    while (retVal != (SDL_DECLARE *) &declare->header)
    {
        if (strcmp(retVal->id, name) == 0)
            break;
        else
            retVal = (SDL_DECLARE *) retVal->header.queue.flink;
    }
    if (retVal == (SDL_DECLARE *) &declare->header)
    {
        retVal = NULL;
    }

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
 *    A pointer to the list containing all currently defined ITEMs.
 *  name:
 *    A pointer to the name of the declared item.
 *
 * Output Parameters:
 *  None.
 *
 * Return Value
 *  NULL:    ITEM not found.
 *  !NULL:    An existing ITEM.
 */
static SDL_ITEM *_sdl_get_item(SDL_ITEM_LIST *item, char *name)
{
    SDL_ITEM    *retVal = (SDL_ITEM *) item->header.flink;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:_sdl_get_item\n", __FILE__, __LINE__);
    }

    while (retVal != (SDL_ITEM *) &item->header)
    {
        if (strcmp(retVal->id, name) == 0)
            break;
        else
            retVal = (SDL_ITEM *) retVal->header.queue.flink;
    }
    if (retVal == (SDL_ITEM *) &item->header)
    {
        retVal = NULL;
    }

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
 *    A pointer to the context structure where we maintain information about
 *    the current state of the parsing.
 *  tag:
 *    A pointer to a string containing the user specified tag (or NULL).
 *  datatype:
 *    An integer indicating either a base type or a user type.  If a base,
 *    get the default.  If a user, we may need to call ourselves again to
 *    get what we came to get.
 *  lower:
 *    A boolean value to indicate that the defaulted tag should be lower
 *    case.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  A pointer to a user specified tag or a default tag.
 */
static char *_sdl_get_tag(
        SDL_CONTEXT *context,
        char *tag,
        int datatype,
        bool lower)
{
    char *retVal = tag;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:_sdl_get_tag\n", __FILE__, __LINE__);
    }

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
        {
            retVal = sdl_strdup(_defaultTag[SDL_K_TYPE_CONST]);
        }
        else if ((datatype >= SDL_K_BASE_TYPE_MIN) &&
                 (datatype <= SDL_K_BASE_TYPE_MAX))
        {
            retVal = sdl_strdup(_defaultTag[datatype]);
        }
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
                SDL_DECLARE *myDeclare = sdl_get_declare(&context->declares,
                                                         datatype);

                if (myDeclare != NULL)
                {
                    if (strlen(myDeclare->tag) > 0)
                    {
                        retVal = sdl_strdup(myDeclare->tag);
                    }
                    else
                    {
                        retVal = _sdl_get_tag(context,
                                              tag,
                                              myDeclare->typeID,
                                              lower);
                    }
                }
                else
                {
                    retVal = sdl_strdup(_defaultTag[SDL_K_TYPE_ANY]);
                }
            }
            else if ((datatype >= SDL_K_ITEM_MIN) &&
                     (datatype <= SDL_K_ITEM_MAX))
            {
                SDL_ITEM *myItem = sdl_get_item(&context->items, datatype);

                if (myItem != NULL)
                {
                    if (strlen(myItem->tag) > 0)
                    {
                        retVal = sdl_strdup(myItem->tag);
                    }
                    else
                    {
                        retVal = _sdl_get_tag(context,
                                              tag,
                                              myItem->typeID,
                                              lower);
                    }
                }
                else
                {
                    retVal = sdl_strdup(_defaultTag[SDL_K_TYPE_ANY]);
                }
            }
            else if ((datatype >= SDL_K_AGGREGATE_MIN) &&
                     (datatype <= SDL_K_AGGREGATE_MAX))
            {
                SDL_AGGREGATE *myAggregate = sdl_get_aggregate(
                                                    &context->aggregates,
                                                    datatype);

                if (myAggregate != NULL)
                {
                    if (strlen(myAggregate->tag) > 0)
                    {
                        retVal = sdl_strdup(myAggregate->tag);
                    }
                    else
                    {
                        retVal = _sdl_get_tag(context,
                                              tag,
                                              myAggregate->typeID,
                                              lower);
                    }
                }
                else
                {
                    retVal = sdl_strdup(_defaultTag[SDL_K_TYPE_ANY]);
                }
            }
        }
        if (lower == true)
        {
            int ii, len = strlen(retVal);

            for (ii = 0; ii < len; ii++)
            {
                retVal[ii] = tolower(retVal[ii]);
            }
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
            {
                retVal[ii] = '\0';
            }
            else
            {
                done = true;
            }
        }
    }

    /*
     * Return the results of this call back to the caller.
     */
    return (retVal);
}

/*
 * _sdl_create_constant
 *  This function is called to create a constant record and return it back to
 *  the caller.
 *
 * Input Parameters:
 *  id:
 *      A pointer to the constant identifier string.
 *  prefix:
 *    A pointer to the prefix to be prepended before the tag and id.
 *  tag:
 *    A pointer to the tag to be added between the prefix and the id.
 *  comment:
 *    A pointer to a comment string to be associated with this constant.
 *  typeName:
 *    A pointer to the type-name associated to this constant.
 *  radix:
 *    A value indicating the radix the value is to be displayed.
 *  value:
 *    A value for the actual constant.
 *  string:
 *    A pointer to a string value for the actual constant.
 *  size:
 *    The number of bytes that this constant represents (used for outputting
 *    MASK constants).
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  NULL:           Failed to allocate the memory required.
 *  !NULL:          Normal Successful Completion.
 */
static SDL_CONSTANT *_sdl_create_constant(
        char *id,
        char *prefix,
        char *tag,
        char *comment,
        char *typeName,
        int radix,
        int64_t value,
        char *string,
        int size,
        SDL_YYLTYPE *loc)
{
    SDL_CONSTANT *retVal = sdl_allocate_block(ConstantBlock, NULL, loc);

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:_sdl_create_constant\n", __FILE__, __LINE__);
    }

    /*
     * Initialize the constant information.
     */
    if (retVal != NULL)
    {
        retVal->id = sdl_strdup(id);
        if (prefix != NULL)
        {
            retVal->prefix = sdl_strdup(prefix);
        }
        else
        {
            retVal->prefix = NULL;
        }
        retVal->tag = sdl_strdup(tag);
        if (comment != NULL)
        {
            retVal->comment = sdl_strdup(comment);
        }
        else
        {
            retVal->comment = NULL;
        }
        if (typeName != NULL)
        {
            retVal->typeName = sdl_strdup(typeName);
        }
        else
        {
            retVal->typeName = NULL;
        }
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
        retVal->size = size;
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
 *    A pointer to the context structure where we maintain information about
 *    the current state of the parsing.
 *  myConst:
 *    A pointer to the constant structure to be queued up and put put.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 */
static uint32_t _sdl_queue_constant(
        SDL_CONTEXT *context,
        SDL_CONSTANT *myConst)
{
    uint32_t retVal = SDL_NORMAL;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:_sdl_queue_constant\n", __FILE__, __LINE__);
    }

    /*
     * Queue up the constant for later searches.
     */
    SDL_INSQUE(&context->constants, &myConst->header.queue);

    retVal = sdl_call_constant(context->langEnableVec,
                               myConst,
                               context);

    /*
     * Return the results back to the caller.
     */
    return (retVal);
}

/*
 * _sdl_create_enum
 *  This function is called to create an enumerate record and return it back to
 *  the caller.
 *
 * Input Parameters:
 *  context:
 *    A pointer to the context structure where we maintain information about
 *    the current state of the parsing.
 *  id:
 *      A pointer to the enum identifier string.
 *  prefix:
 *    A pointer to the prefix to be prepended before the tag and id.
 *  tag:
 *    A pointer to the tag to be added between the prefix and the id.
 *  typeDef:
 *    A boolean value indicating that this enumeration will be declared as a
 *    typedef.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  NULL:           Failed to allocate the memory required.
 *  !NULL:          Normal Successful Completion.
 */
static SDL_ENUMERATE *_sdl_create_enum(
                SDL_CONTEXT *context,
                char *id,
                char *prefix,
                char *tag,
                bool typeDef,
                SDL_YYLTYPE *loc)
{
    SDL_ENUMERATE *retVal = sdl_allocate_block(EnumerateBlock,
                                               NULL,
                                               loc);

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:_sdl_create_enum\n", __FILE__, __LINE__);
    }

    if (retVal != NULL)
    {
        SDL_Q_INIT(&retVal->members);
        retVal->id = sdl_strdup(id);
        if (prefix != NULL)
        {
            retVal->prefix = sdl_strdup(prefix);
        }
        else
        {
            retVal->prefix = NULL;
        }
        retVal->tag = sdl_strdup(tag);
        retVal->typeDef = typeDef;
        retVal->size = sdl_sizeof(context, SDL_K_TYPE_ENUM);
        retVal->typeID = context->enums.nextID++;
        SDL_INSQUE(&context->enums.header, &retVal->header.queue);
    }

    /*
     * Return the results back to the caller.
     */
    return (retVal);
}

/*
 * _sdl_enum_compl
 *  This function is called to call the language specific output routines to
 *  output and enumeration.
 *
 * Input Parameters:
 *  context:
 *    A pointer to the context structure where we maintain information about
 *    the current state of the parsing.
 *  myEnum:
 *    A pointer to the ENUMERATE structure to be output.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  0:              An error occurred.
 */
static uint32_t _sdl_enum_compl(SDL_CONTEXT *context, SDL_ENUMERATE *myEnum)
{
    uint32_t retVal = SDL_NORMAL;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:_sdl_enum_compl\n", __FILE__, __LINE__);
    }

    retVal = sdl_call_enumerate(context->langEnableVec,
                                myEnum,
                                context);

    /*
     * Return the results back to the caller.
     */
    return (retVal);
}

/*
 * _sdl_reset_options
 *  This function is called to reset the options array in the context block.
 *  If any of the saved options is a string and it is not NULL, then free it.
 *
 * Input Parameters:
 *  context:
 *    A pointer to the context structure where we maintain information about
 *    the current state of the parsing.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  None.
 */
static void _sdl_reset_options(SDL_CONTEXT *context)
{
    int    ii;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:_sdl_reset_options\n", __FILE__, __LINE__);
    }

    /*
     * Loop through each of the options, and if we have a string option and it
     * is not NULL, then free it.
     */
    for (ii = 0; ii < context->optionsIdx; ii++)
    {
        if (((context->options[ii].option == Alias) ||
             (context->options[ii].option == Based) ||
             (context->options[ii].option == Counter) ||
             (context->options[ii].option == Linkage) ||
             (context->options[ii].option == Marker) ||
             (context->options[ii].option == Named) ||
             (context->options[ii].option == Origin) ||
             (context->options[ii].option == Prefix) ||
             (context->options[ii].option == ReturnsNamed) ||
             (context->options[ii].option == Tag) ||
             (context->options[ii].option == TypeName)) &&
            (context->options[ii].string != NULL))
        {
            sdl_free(context->options[ii].string);
        }
    }

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
 *    A pointer to the context structure where we maintain information about
 *    the current state of the parsing.
 *  member:
 *    A pointer to the current member are are iterating through.
 *  qhead:
 *    The address of the queue header.  Used to determine when we have
 *    search through each of the member entries in the AGGREGATE or
 *    subaggregate.
 *  callback:
 *    A pointer to the entry point to call to do something with the member
 *    information.  If this is NULL, then we are just deallocating all the
 *    member memory.  If tracing is turned on, we will display the records
 *    as we iterate through.
 *  depth:
 *    A value indicating the aggregate depth we are currently iterating.
 *  count
 *    A value indicating the number of members at the current depth.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:        Normal Successful Completion.
 *  SDL_ABORT:        An unexpected  error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
static uint32_t _sdl_iterate_members(
        SDL_CONTEXT *context,
        SDL_MEMBERS *member,
        void *qhead,
        int (*callback)(),
        int depth,
        int count)
{
    uint32_t    retVal = SDL_NORMAL;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:_sdl_iterate_members\n", __FILE__, __LINE__);
    }

    if (sdl_isItem(member) == false)
    {
        SDL_SUBAGGR *subaggr = &member->subaggr;

        if ((trace == true) && (callback == NULL))
        {
            printf("\t%d: SUBAGGREGATE:\n\t    name: %s\n\t    prefix: %s\n"
                   "\t    tag: %s\n\t    marker: %s\n\t    arrgType: %s\n"
                   "\t    typeID: %d\n\t    alignment: %d\n\t    type: %d\n"
                   "\t    _unsigned: %s\n\t    size: %ld\n\t    offset: %ld (%ld)\n"
                   "\t    typeDef: %s\n\t    fill: %s\n\t    basedPtrName: %s\n"
                   "\t    currentBitOffset: %d\n",
                   count,
                   subaggr->id,
                   (subaggr->prefix != NULL ? subaggr->prefix : ""),
                   (subaggr->tag != NULL ? subaggr->tag : ""),
                   (subaggr->marker != NULL ? subaggr->marker : ""),
                   (subaggr->aggType == SDL_K_TYPE_STRUCT ? "STRUCTURE" : "UNION"),
                   subaggr->typeID,
                   subaggr->alignment,
                   subaggr->type,
                   (subaggr->_unsigned == true ? "True" : "False"),
                   subaggr->size,
                   subaggr->offset, member->offset,
                   (subaggr->typeDef == true ? "True" : "False"),
                   (subaggr->fill == true ? "True" : "False"),
                   (subaggr->basedPtrName!= NULL ? subaggr->basedPtrName: ""),
                   subaggr->currentBitOffset);
                if (subaggr->dimension == true)
                {
                    printf("\t    dimension[%ld:%ld]\n",
                           subaggr->lbound,
                           subaggr->hbound);
                }
        }
        if (callback != NULL)
        {
            (*callback)(context, member, false, depth);
        }
        if (SDL_Q_EMPTY(&subaggr->members) == false)
        {
            retVal = _sdl_iterate_members(context,
                                          (SDL_MEMBERS *) subaggr->members.flink,
                                          &subaggr->members,
                                          callback,
                                          depth + 1,
                                          1);
        }
        if (callback != NULL)
        {
            (*callback)(context, member, true, depth);
        }
        if (member->header.queue.flink != qhead)
        {
            retVal = _sdl_iterate_members(context,
                                          member->header.queue.flink,
                                          qhead,
                                          callback,
                                          depth,
                                          count + 1);
        }
    }
    else    /* Unknown */
    {
    if ((trace == true) && (callback == NULL))
    {
        if (sdl_isComment(member) == true)
        {
        printf(
            "\t%d: COMMENT:\n\t    comment: %s\n\t    endComment: %s\n"
            "\t    lineComment: %s\n\t    middleComment: %s\n"
            "\t    startComment: %s\n",
            count,
            member->comment.comment,
            (member->comment.endComment == true ? "True" : "False"),
            (member->comment.lineComment == true ? "True" : "False"),
            (member->comment.middleComment == true ? "True" : "False"),
            (member->comment.startComment == true ? "True" : "False"));
        }
        else
        {
            printf("\t%d: ITEM:\n\t    name: %s\n\t    prefix: %s\n"
                   "\t    tag: %s\n\t    typeID: %d\n\t    alignment: %d\n"
                   "\t    type: %d\n\t    _unsigned: %s\n\t    size: %ld\n"
                   "\t    typeDef: %s\n\t    fill: %s\n"
                   "\t    offset: %ld (%ld)\n\t    length: %ld\n"
                   "\t    mask: %s\n\t    bitfieldType: %ld\n"
                   "\t    bitOffset: %d\n",
                   count,
                   member->item.id,
                   (member->item.prefix != NULL ? member->item.prefix : ""),
                   (member->item.tag != NULL ? member->item.tag : ""),
                   member->item.typeID,
                   member->item.alignment,
                   member->item.type,
                   (member->item._unsigned == true ? "True" : "False"),
                   member->item.size,
                   (member->item.typeDef == true ? "True" : "False"),
                   (member->item.fill == true ? "True" : "False"),
                   member->item.offset, member->offset,
                   member->item.length,
                   (member->item.mask == true ? "True" : "False"),
                   member->item.subType,
                   member->item.bitOffset);
                if (member->item.dimension == true)
                {
                    printf("\t    dimension: [%ld:%ld]\n",
                           member->item.lbound,
                           member->item.hbound);
                }
            }
        }
        if (callback != NULL)
        {
            (*callback)(context, member, false, depth);
        }
        if (member->header.queue.flink != qhead)
        {
            retVal = _sdl_iterate_members(context,
                                          member->header.queue.flink,
                                          qhead,
                                          callback,
                                          depth,
                                          count + 1);
        }
    }

    /*
     * Return the results back to the caller.
     */
    return(retVal);
}

/*
 * _sdl_determine_offsets
 *   This function is called to determine the offsets (byte and bit) for the
 *   specified member.  Byte offsets at this point are relative to the parent
 *   AGGREGATE.
 *
 * Input Parameters:
 *  context:
 *    A pointer to the context structure where we maintain information about
 *    the current state of the parsing.
 *  member:
 *    A pointer to the member for which we are to determine the offset where
 *    this field will be stored.
 *  memberList:
 *    A pointer to the queue of members where the supplied member will be
 *    queued as a child.
 *  parentIsUnion:
 *    A boolean value that indicates if the parent AGGREGATE or subaggregate
 *    is a UNION or not.  We do not add padding for UNIONs.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  None.
 */
static void _sdl_determine_offsets(
            SDL_CONTEXT *context,
            SDL_MEMBERS *member,
            SDL_QUEUE *memberList,
            bool parentIsUnion)
{
    SDL_MEMBERS *prevMember;
    int64_t realSize;
    int64_t length = 1;
    int dimension = 1;
    bool memberItem = sdl_isItem(member);
    bool prevItem = false;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:_sdl_determine_offsets\n", __FILE__, __LINE__);
    }

    /*
     * If the member to be inserted is a COMMENT, then there is nothing else we
     * need to do.
     */
    if (sdl_isComment(member) == true)
    {
        return;
    }

    /*
     * Get the previous member, if there is one that is not a comment.
     */
    prevMember = (SDL_MEMBERS *) memberList->blink;
    while ((prevMember != (SDL_MEMBERS *) &memberList->flink) &&
           (sdl_isComment(prevMember) == true))
    {
        prevMember = (SDL_MEMBERS *)prevMember->header.queue.blink;
    }
    if (prevMember != (SDL_MEMBERS *) &memberList->flink)
    {
        prevItem = sdl_isItem(prevMember);
    }
    else
    {
        prevMember = NULL;
    }

    /*
     * If the new member is a BITFIELD and the previous one is as well, then
     * we need to do some checking to determine if we need to insert some a
     * filler bitfield, or are starting a new one, based on the number of bits
     * previously utilized.  We will also generate the appropriate MASK and
     * SIZE constants.  If the number of bits has been defaulted, then we can
     * resize it up to the next usable size.  Bitfields are defaulted to
     * UNSIGNED BYTE fields, but can be defined as an UNSIGNED QUADWORD field,
     * if so desired.
     */
    if (sdl_isBitfield(member) == true)
    {

        /*
         * If the previous member is not a BITFIELD, then we are starting a new
         * potential set of bits, and need to consider the offset for them.  It
         * is determined by the offset of the previous member, plus the size of
         * the previous member.
         */
        if ((prevMember == NULL) || (sdl_isBitfield(prevMember) == false))
        {
            member->item.bitOffset = 0;
            if (prevItem == true)
            {

                /*
                 * Get the length information, which can come from the
                 * length, or precision information, depending upon type.
                 */
                switch (prevMember->item.type)
                {
                    case SDL_K_TYPE_CHAR:
                    case SDL_K_TYPE_CHAR_VARY:
                        length = prevMember->item.length;
                        break;

                    case SDL_K_TYPE_DECIMAL:
                        length = prevMember->item.precision;
                        break;

                    default:
                        length = 1;
                        break;
                }
                if (length == 0)
                    length = 1;

                /*
                 * The real size if the item size times the length.
                 */
                realSize = prevMember->item.size * length;

                /*
                 * If the type is CHARACTER LENGTH VARYING or DECIMAL, then we
                 * need to add 2 bytes for the length field for the former and
                 * 1 byte for the latter, to the realSize.
                 */
                if (prevMember->item.type == SDL_K_TYPE_CHAR_VARY)
                {
                    realSize += sizeof(int16_t);
                }
                else if (prevMember->item.type == SDL_K_TYPE_DECIMAL)
                {
                    realSize++;
                }

                /*
                 * Now get the actual dimension, if specified, for the item.
                 */
                if (prevMember->item.dimension == true)
                {
                    dimension = prevMember->item.hbound -
                        prevMember->item.lbound + 1;
                }

                /*
                 * The offset for this member is the offset of the previous
                 * member plus the product of the real size and the dimension.
                 */
                member->offset = prevMember->offset + (realSize * dimension);
            }
            else if (prevMember != NULL)
            {
                int64_t    size = 0;

                if (prevMember->type != SDL_K_TYPE_UNION)
                {
                    if (prevMember->subaggr.dimension == true)
                    {
                        dimension = prevMember->subaggr.hbound -
                                prevMember->subaggr.lbound + 1;
                    }
                    size = prevMember->subaggr.size * dimension;
                }
                member->offset = prevMember->offset + size;
            }
            else if (member->header.top == false)
            {
                SDL_SUBAGGR *subagg = (SDL_SUBAGGR *) member->header.parent;

                member->offset = subagg->offset;
            }
            else
            {
                member->offset = 0;
            }

            /*
             * If the bitfield was not specifically sized and the number of
             * bits for this field is larger than the defaulted size, then we
             * need to resize the bitfield to account for this.
             */
            if (member->item.sizedBitfield == true)
            {

                /*
                 * If the length is greater than 8 and the bitfield is only a
                 * byte, then change the type to a bitfield of word size.
                 */
                if ((member->item.type == SDL_K_TYPE_BITFLD_B) &&
                    (member->item.length > 8))
                {
                    member->item.type = SDL_K_TYPE_BITFLD_W;
                }

                /*
                 * If the length is greater than 16 and the bitfield is only a
                 * word, then change the type to a bitfield of longword size.
                 */
                if ((member->item.type == SDL_K_TYPE_BITFLD_W) &&
                    (member->item.length > 16))
                {
                    member->item.type = SDL_K_TYPE_BITFLD_L;
                }

                /*
                 * If the length is greater than 32 and the bitfield is only a
                 * longword, then change the type to a bitfield of quadword
                 * size.
                 */
                if ((member->item.type == SDL_K_TYPE_BITFLD_L) &&
                    (member->item.length > 32))
                {
                    member->item.type = SDL_K_TYPE_BITFLD_Q;
                }

                /*
                 * If the length is greater than 64 and the bitfield is only a
                 * quadword, then change the type to a bitfield of octaword
                 * size.
                 */
                if ((member->item.type == SDL_K_TYPE_BITFLD_L) &&
                    (member->item.length > 32))
                {
                    member->item.type = SDL_K_TYPE_BITFLD_Q;
                }
                member->item.size = sdl_sizeof(context, member->item.type);
            }
        }
        else if (prevMember != NULL)
        {
            int availBits;

            /*
             * Before we go too far, see if the bitfields need to have their
             * sizes adjusted, but only if we are allowed to do so.
             */
            if (member->item.sizedBitfield == false)
            {
                _sdl_check_bitfieldSizes(context,
                                         memberList,
                                         NULL,
                                         member->item.length,
                                         member,
                                         NULL);
            }
            availBits = (prevMember->item.size * 8) -
                    prevMember->item.bitOffset -
                    prevMember->item.length;

            /*
             * The new member and the previous member are both BITFIELDs.
             */
            if (member->item.size == prevMember->item.size)
            {

                /*
                 * If there is enough room for the number of bits we need to
                 * add to this BITFIELD, then do so.  The field offset will not
                 * change.
                 */
                if (member->item.length <= availBits)
                {
                    member->item.bitOffset = prevMember->item.bitOffset +
                                 prevMember->item.length;
                    member->offset = prevMember->offset;
                }

                /*
                 * There was not enough room for the bits being requests.  This
                 * will use bit 0 as the initial offset, and the field offset
                 * will be the offset of the previous member, plus its length.
                 */
                else
                {
                    member->item.bitOffset = 0;
                    member->offset = prevMember->offset + prevMember->item.size;
                    if ((availBits > 0) && (parentIsUnion == false))
                    {
                        _sdl_fill_bitfield(memberList,
                                           prevMember,
                                           availBits,
                                           context->fillerCount++,
                                           &member->item.loc);
                    }
                }
            }
            else
            {
                member->item.bitOffset = 0;
                member->offset = prevMember->offset + prevMember->item.size;
                if ((availBits > 0) && (parentIsUnion == false))
                {
                    _sdl_fill_bitfield(memberList,
                                       prevMember,
                                       availBits,
                                       context->fillerCount++,
                                       &member->item.loc);
                }
            }
        }
        else
        {
            member->offset = 0;
        }
    }

    /*
     * We are not inserting a BITFIELD.
     */
    else
    {

        /*
         * We may be inserting after a BITFIELD.
         */
        if ((prevMember != NULL) && (sdl_isBitfield(prevMember) == true))
        {
            int availBits = (prevMember->item.size * 8) -
                    prevMember->item.bitOffset -
                    prevMember->item.length;
            if ((availBits > 0) && (parentIsUnion == false))
            {
                _sdl_fill_bitfield(
                        memberList,
                        prevMember,
                        availBits,
                        context->fillerCount++,
                        &member->item.loc);
            }
        }

        /*
         * We need to determine the raw offset from the previous member.
         */
        if (prevMember != NULL)
        {
            if ((prevItem == true) && (parentIsUnion == false))
            {

                /*
                 * Get the length information, which can come from the
                 * length, or precision information, depending upon type.
                 */
                switch (prevMember->item.type)
                {
                    case SDL_K_TYPE_CHAR:
                    case SDL_K_TYPE_CHAR_VARY:
                        length = prevMember->item.length;
                        break;

                    case SDL_K_TYPE_DECIMAL:
                        length = prevMember->item.precision;
                        break;

                    default:
                        length = 1;
                        break;
                }
                if (length == 0)
                {
                    length = 1;
                }

                /*
                 * The real size if the item size times the length.
                 */
                realSize = prevMember->item.size * length;

                /*
                 * If the type is CHARACTER LENGTH VARYING or DECIMAL, then we
                 * need to add 2 bytes for the length field for the former and
                 * 1 byte for the latter, to the realSize.
                 */
                if (prevMember->item.type == SDL_K_TYPE_CHAR_VARY)
                {
                    realSize += sizeof(int16_t);
                }
                else if (prevMember->item.type == SDL_K_TYPE_DECIMAL)
                {
                    realSize++;
                }

                /*
                 * Now get the actual dimension, if specified, for the item.
                 */
                if (prevMember->item.dimension == true)
                {
                    dimension = prevMember->item.hbound -
                            prevMember->item.lbound + 1;
                }
            }
            else if (parentIsUnion == false)
            {
                realSize = prevMember->subaggr.size;
                if (prevMember->subaggr.dimension == true)
                {
                    dimension = prevMember->subaggr.hbound -
                            prevMember->subaggr.lbound + 1;
                }
            }
            else
            {
                realSize = 0;    /* all the offsets are the same in unions */
            }

            /*
             * The offset for this member is the offset of the previous
             * member plus the product of the real size and the dimension.
             */
            member->offset = prevMember->offset + (realSize * dimension);
        }
        else
            member->offset = 0;
    }

    /*
     * Before we can get out of here, we need to potentially align the new
     * member, based on its data type, but only for ITEMs.
     */
    if (memberItem == true)
    {
        int    adjustment;

        switch (member->item.alignment)
        {
            case SDL_K_NOALIGN:
                adjustment = 0;
                break;

            case SDL_K_ALIGN:
                adjustment = member->offset % member->item.size;
                if (adjustment != 0)
                {
                    adjustment = member->item.size - adjustment;
                }
                break;

            default:
                adjustment = member->offset % member->item.alignment;
                if (adjustment != 0)
                {
                    adjustment = member->item.alignment - adjustment;
                }
                break;
        }
        member->offset += adjustment;
        member->item.offset = member->offset;
    }
    else
    {
        member->subaggr.offset = member->offset;
    }

    /*
     * Return back to the caller.
     */
    return;
}

/*
 * _sdl_fill_bitfield
 *  This function is called to create an AGGREGATE or subaggregate BITFIELD
 *  member to fill out the remaining bits in a bitfield.
 *
 * Input Parameters:
 *  memberList:
 *    A pointer to the queue where the new member will be added.
 *  member:
 *    A pointer to the member that will precede the one we are creating.
 *  bits:
 *    A value indicating the number of bits to be associated with this field.
 *  loc:
 *    A pointer to the start and end locations for this item in the input
 *    file.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  None.
 */
static void _sdl_fill_bitfield(
            SDL_QUEUE *memberList,
            SDL_MEMBERS *member,
            int bits,
            int number,
            SDL_YYLTYPE *loc)
{
    SDL_MEMBERS *filler = sdl_allocate_block(AggrMemberBlock,
                                             member->header.parent,
                                             loc);
    char    idBuf[32];

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:_sdl_fill_bitfield\n", __FILE__, __LINE__);
    }

    memcpy(filler, member, sizeof(SDL_MEMBERS));
    sprintf(idBuf, "filler_%03d", number);
    filler->item.id = sdl_strdup(idBuf);
    if (member->item.prefix != NULL)
    {
        filler->item.prefix = sdl_strdup(member->item.prefix);
    }
    filler->item.tag = sdl_strdup(member->item.tag);
    filler->item.length = bits;
    filler->item.mask = false;
    filler->item.bitOffset = member->item.bitOffset + 1;
    SDL_COPY_LOC(filler->item.loc, &member->item.loc);
    SDL_INSQUE(memberList, &filler->header.queue);

    /*
     * Return back to the caller.
     */
    return;
}

/*
 * _sdl_aggregate_size
 *  This function is called to determine an AGGREGATE's or subaggregate's
 *  actual size.  In addition to the size of an AGGGREGATE or subaggregate, for
 *  each subaggregate, the offset is checked against the first member.
 *  NOTE: aggr and subAggr parameters cannot be supplied or NULL at the same
 *  time.
 *
 * Input Parameters:
 *  aggr:
 *    A pointer to an SDL_AGGREGATE block that needs to have its size
 *    calculated, or NULL.
 *  subAggr:
 *    A pointer to an SDL_SUBAGGR block that needs to have its size
 *    calculated, or NULL.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  A value representing the size of the AGGREGATE or subaggregate.
 */
static int64_t _sdl_aggregate_size(
            SDL_CONTEXT *context,
            SDL_AGGREGATE *aggr,
            SDL_SUBAGGR *subAggr)
{
    SDL_MEMBERS *member = NULL;
    SDL_QUEUE *memberList = NULL;
    SDL_CONSTANT *constDef;
    char *name = NULL;
    char *prefix = NULL;
    SDL_YYLTYPE loc = {0, 0, 0, 0};
    int64_t retVal = 0;
    int64_t size = 0;
    int dimension = 1;
    int unionType;
    bool isUnion;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:_sdl_aggregate_size\n", __FILE__, __LINE__);
    }

    /*
     * Get the last member for either the AGGREGATE or subaggregate.  If we
     * have a subaggregate, then we also need to determine the current offset
     * (AGGREGATES always have an offset of 0).
     */
    if (aggr != NULL)
    {
        name = aggr->id;
        prefix = aggr->prefix;
        if (SDL_Q_EMPTY(&aggr->members) == false)
        {
            member = (SDL_MEMBERS *) aggr->members.blink;
            memberList = &aggr->members;
        }
        isUnion = aggr->aggType == SDL_K_TYPE_UNION;
        unionType = aggr->type;
    }
    else if (subAggr != NULL)
    {
        name = subAggr->id;
        prefix = subAggr->prefix;
        isUnion = subAggr->aggType == SDL_K_TYPE_UNION;
        unionType = subAggr->type;
        if (SDL_Q_EMPTY(&subAggr->members) == false)
        {
            int adjustment;
            int64_t alignSize = sdl_sizeof(context, unionType);

            /*
             * Before we go too far, check the first member in the list and
             * make sure that the offset for the subaggregate is properly
             * aligned for the first member.
             */
            member = (SDL_MEMBERS *) subAggr->members.flink;

            /*
             * If this is a union, we need the largest member in the union.
             * Otherwise, we need the first non-comment in the structure.
             */
            if (isUnion == true)
            {
                while (member != (SDL_MEMBERS *) &subAggr->members.flink)
                {
                    if (sdl_isComment(member) == false)
                    {
                        int64_t memberSize;

                        if (sdl_isItem(member) == true)
                        {
                            memberSize = member->item.size;
                        }
                        else
                        {
                            memberSize = member->subaggr.size;
                        }
                        if (alignSize < memberSize)
                        {
                            alignSize = memberSize;
                        }
                    }
                    member = member->header.queue.flink;
                }
            }
            else
            {
                while ((member != (SDL_MEMBERS *) &subAggr->members.flink) &&
                       (sdl_isComment(member) == true))
                {
                    member = member->header.queue.flink;
                }
                if (member != (SDL_MEMBERS *) &subAggr->members.flink)
                {
                    alignSize = member->item.size;
                }
            }

            /*
             * Determine what the adjustment needs to be for the alignment.
             */
            switch (subAggr->alignment)
            {
                case SDL_K_NOALIGN:
                    adjustment = 0;
                    break;

                case SDL_K_ALIGN:
                    adjustment = subAggr->offset % alignSize;
                    if (adjustment != 0)
                    {
                        adjustment = alignSize - adjustment;
                    }
                    break;

                default:
                    adjustment = subAggr->offset % subAggr->alignment;
                    if (adjustment != 0)
                    {
                        adjustment = subAggr->alignment - adjustment;
                    }
                    break;
            }

            /*
             * Update the offset so that the STRUCTURE/UNION are aligned.
             */
            subAggr->offset += adjustment;
            subAggr->self->offset = subAggr->offset;

            /*
             * Now change to the last member, which is needed for the remainder
             * of the code.
             */
            member = (SDL_MEMBERS *) subAggr->members.blink;
            memberList = &subAggr->members;
        }
    }

    /*
     * Before we can try and figure out the AGGREGATE or subaggregate size, we
     * may have just ended a BITFIELD, but without all the bits used.
     */
    if ((member != NULL) &&
        (sdl_isBitfield(member) == true) &&
        (isUnion == false))
    {
        int availBits = (member->item.size * 8) -
                member->item.bitOffset - member->item.length;

        if (availBits > 0)
        {
            _sdl_fill_bitfield(memberList,
                               member,
                               availBits,
                               context->fillerCount++,
                               &member->item.loc);
        }
    }

    /*
     * OK, we should have what we need to determine the size for this
     * AGGREGATE or subaggregate.  The size is the last member offset, minus
     * its own offset, plus the total size of the last member (which is its
     * individual size times its dimension).
     */
    if (member != NULL)
    {

        /*
         * The size of a UNION is the size of the largest size or the size
         * indicated on an IMPLICIT UNION definition, which ever is larger.
         * If the IMPLICIT UNION is supposed to be larger than the fields
         * defined to it, then insert an additional member of the proper size.
         */
        if (isUnion == true)
        {
            int64_t unionSize = sdl_sizeof(context, unionType);

            retVal = 0;
            member = memberList->flink;
            while (member != (SDL_MEMBERS *) &memberList->flink)
            {
                dimension = 1;
                if (sdl_isItem(member) == true)
                {
                    int64_t    length;

                    if (sdl_isComment(member) == true)
                    {
                        size = 0;
                    }
                    else
                    {
                        size = member->item.size;
                        switch (member->item.type)
                        {
                            case SDL_K_TYPE_CHAR:
                            case SDL_K_TYPE_CHAR_VARY:
                                length = member->item.length;
                                break;

                            case SDL_K_TYPE_DECIMAL:
                                length = member->item.precision;
                                break;

                            default:
                                length = 1;
                                break;
                        }
                        if (length == 0)
                        {
                            length = 1;
                        }
                        size *= length;
                        if (member->item.type == SDL_K_TYPE_CHAR_VARY)
                        {
                            size += sizeof(int16_t);
                        }
                        else if (member->item.type == SDL_K_TYPE_DECIMAL)
                        {
                            size++;
                        }
                        if (member->item.dimension == true)
                        {
                            dimension = member->item.hbound -
                                member->item.lbound + 1;
                        }
                        size *= dimension;
                    }
                }
                else
                {
                    size = member->subaggr.size;
                    if (member->subaggr.dimension == true)
                    {
                        dimension = member->subaggr.hbound -
                            member->subaggr.lbound + 1;
                    }
                    size *= dimension;
                }
                if (size > retVal)
                {
                    retVal = size;
                }
                member = (SDL_MEMBERS *) member->header.queue.flink;
            }

            /*
             * IMPLICIT UNIONs have a datatype that has a default size.  If the
             * largest member in the union is not sufficiently large, then we
             * need to add a filler to compensate.
             */
            if (retVal < unionSize)
            {
                SDL_MEMBERS *filler;
                void *parent;
                SDL_YYLTYPE loc = {0, 0, 0, 0};

                parent = aggr != NULL ? &aggr->header : (SDL_HEADER *) subAggr;
                filler = sdl_allocate_block(AggrMemberBlock, parent, &loc);
                if (filler != NULL)
                {
                    char *prefix;
                    char idBuf[32];
                    int datatype;
                    int alignment;

                    prefix = aggr != NULL ? aggr->prefix : subAggr->prefix;
                    datatype = aggr != NULL ? aggr->type : subAggr->type;
                    alignment = aggr != NULL ? aggr->alignment : subAggr->alignment;
                    filler->item.type = datatype;
                    filler->item._unsigned = false;
                    filler->item.size = sdl_sizeof(context, datatype);
                    filler->item.alignment = alignment;
                    filler->item.parentAlignment = true;
                    sprintf(idBuf, "filler_%03d", context->fillerCount++);
                    filler->item.id = sdl_strdup(idBuf);
                    if (prefix != NULL)
                    {
                        filler->item.prefix = sdl_strdup(prefix);
                    }
                    filler->item.tag = _sdl_get_tag(context,
                                                    NULL,
                                                    datatype,
                                                    sdl_all_lower(filler->item.id));
                    SDL_COPY_LOC(filler->item.loc, &filler->loc);
                    _sdl_determine_offsets(context, filler, memberList, true);
                    SDL_INSQUE(memberList, &filler->header.queue);
                }
                retVal = unionSize;
            }
        }
        else
        {
            retVal = member->offset;
            if (sdl_isItem(member) == true)
            {
                int64_t length;

                size = member->item.size;
                switch(member->item.type)
                {
                    case SDL_K_TYPE_CHAR:
                    case SDL_K_TYPE_CHAR_VARY:
                        length = member->item.length;
                        break;

                    case SDL_K_TYPE_DECIMAL:
                        length = member->item.precision;
                        break;

                    default:
                        length = 1;
                        break;
                }
                if (length == 0)
                {
                    length = 1;
                }
                size *= length;
                if (member->item.type == SDL_K_TYPE_CHAR_VARY)
                {
                    size += sizeof(int16_t);
                }
                else if (member->item.type == SDL_K_TYPE_DECIMAL)
                {
                    size++;
                }
                if (member->item.dimension == true)
                {
                    dimension = member->item.hbound - member->item.lbound + 1;
                }
            }
            else
            {
                size += member->subaggr.size;
                if (member->subaggr.dimension == true)
                {
                    dimension = member->subaggr.hbound -
                            member->subaggr.lbound + 1;
                }
            }
            retVal += (size * dimension);
        }

        /*
         * Go generate any and all CONSTANTs needed each of the BITFIELDs
         * defined within this AGGREGATE/subaggregate.  We ignore the return
         * parameter from the call.
         */
        (void) _sdl_create_bitfield_constants(context, memberList);
    }

    /*
     * Create the constant for the size of the AGGREGATE/subaggregate.
     */
    constDef = _sdl_create_constant(name,
                                    (prefix == NULL ? "" : prefix),
                                    (sdl_all_lower(name) ? "s" : "S"),
                                    NULL,
                                    NULL,
                                    SDL_K_RADIX_DEC,
                                    retVal,
                                    NULL,
                                    context->argument[ArgWordSize].value,
                                    &loc);
    if (constDef != NULL)
    {
        _sdl_queue_constant(context, constDef);
    }

    /*
     * Return the results back to the caller.
     */
    return(retVal);
}

/*
 * _sdl_checkAndSetOrigin
 *  This function is called to check the current top AGGREGATE for an ORIGIN
 *  reference.  If it was defined, and the supplied member's name matches the
 *  ORIGIN name, and the ORIGIN has not already been set, then set it now.
 *
 * Input Parameters:
 *  context:
 *    A pointer to the context structure where we maintain information about
 *    the current state of the parsing.
 *  member:
 *    A pointer to the member for which we are to determine the offset where
 *    this field will be stored.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  None.
 */
static void _sdl_checkAndSetOrigin(SDL_CONTEXT *context, SDL_MEMBERS *member)
{
    SDL_AGGREGATE *aggr = (SDL_AGGREGATE *) context->aggregates.header.blink;
    char *id = (sdl_isItem(member) == true ? member->item.id : member->subaggr.id);

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:_sdl_checkAndSetOrigin\n", __FILE__, __LINE__);
    }

    /*
     * If any aggregates have been defined, then check the member.id against
     * the ORIGIN id, and if they match, and we have not already saved a
     * member, then save a pointer to the member in the ORIGIN of the
     * AGGREGATE.
     */
    if (context->aggregates.nextID > SDL_K_AGGREGATE_MIN)
    {
        if ((aggr->origin.origin == NULL) &&
            (aggr->origin.id != NULL) &&
            (strcmp(aggr->origin.id, id) == 0))
        {
            aggr->origin.origin = member;
        }
    }

    /*
     * Return back to the caller.
     */
    return;
}

/*
 * _sdl_check_bitfieldSizes
 *  This function is called to scan backwards in a member list and determine if
 *  all the contiguous items in the list are BITFIELDs that can potentially be
 *  resized because the number of bits is getting larger than the defaulted
 *  size and should be resized.  This function is recursive.  NOTE: We only
 *  look at bitfields that can be resized (where the user did not specify a
 *  particular size).
 *
 * Input Parameters:
 *  context:
 *    A pointer to the context structure where we maintain information about
 *    the current state of the parsing.
 *  memberList:
 *    A pointer to the member list header.  This is used to determine when we
 *    have recursed back to the beginning of the list.
 *  member:
 *    A pointer to the member we are to be looking at in this call.  The
 *    first time we are called, this parameter is NULL.
 *  length:
 *    A value indicating the number of bits in use.
 *  newMember:
 *    A pointer to the member we are looking to add that may have cause the
 *    bitfield to overflow.
 *  updated:
 *    A pointer to a boolean value indicating that the BITFIELD type has been
 *    updated.  This is NULL on the first call.
 *
 * Output Parameters:
 *  None, but the members that are BITFIELDs may have all been updated.
 *
 * Return Values:
 *  None.
 */
static void _sdl_check_bitfieldSizes(
            SDL_CONTEXT *context,
            SDL_QUEUE *memberList,
            SDL_MEMBERS *member,
            int64_t length,
            SDL_MEMBERS *newMember,
            bool *updated)
{
    SDL_MEMBERS *prevMember;
    static bool myUpdated;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:_sdl_check_bitfieldSizes\n", __FILE__, __LINE__);
    }

    /*
     * If the member parameter has not been passed, then the previous member is
     * off the blink field of the member list.  Otherwise it is off the blink
     * field of the member itself.
     */
    if (member == NULL)
    {
        prevMember = (SDL_MEMBERS *) memberList->blink;
        myUpdated = false;
    }
    else
    {
        prevMember = (SDL_MEMBERS *) member->header.queue.blink;
    }

    /*
     * If the previous member is a COMMENT, then loop until we either run out
     * of member of the member is no longer a comment.
     */
    while ((prevMember != (SDL_MEMBERS *) &memberList->flink) &&
           (sdl_isComment(prevMember) == true))
    {
        prevMember = (SDL_MEMBERS *) prevMember->header.queue.blink;
    }

    /*
     * If the previous member is not actually the address of the member list,
     * then check to see if it is an item, and one of the BITFIELD types, and
     * one that was not specifically sized by the user.  If so, then call
     * ourselves again with an updated number of bits.  If not, this previous
     * member will not help us, so return to deal with the next member.
     */
    if (prevMember != (SDL_MEMBERS *) &memberList->flink)
    {
        if ((sdl_isItem(prevMember) == true) &&
            ((prevMember->item.type == SDL_K_TYPE_BITFLD_B) ||
             (prevMember->item.type == SDL_K_TYPE_BITFLD_W) ||
             (prevMember->item.type == SDL_K_TYPE_BITFLD_L) ||
             (prevMember->item.type == SDL_K_TYPE_BITFLD_Q) ||
             (prevMember->item.type == SDL_K_TYPE_BITFLD_O)) &&
             (prevMember->item.sizedBitfield == false))
        {
            length += prevMember->item.length;
            _sdl_check_bitfieldSizes(context,
                                     memberList,
                                     prevMember,
                                     length,
                                     NULL,
                                     &myUpdated);
        }
        else
        {
            return;
        }
    }

    /*
     * If member is not NULL, then we have one or more previous bitfields.  We
     * get here with the total length of bits required for this bitfield.
     */
    if (member != NULL)
    {
        if (myUpdated == false)
        {

            /*
             * If the length is greater than 8 and the bitfield is only a
             * byte, then change the type to a bitfield of word size.
             */
            if ((member->item.type == SDL_K_TYPE_BITFLD_B) && (length > 8))
            {
                member->item.type = SDL_K_TYPE_BITFLD_W;
            }

            /*
             * If the length is greater than 16 and the bitfield is only a
             * word, then change the type to a bitfield of longword size.
             */
            if ((member->item.type == SDL_K_TYPE_BITFLD_W) && (length > 16))
            {
                member->item.type = SDL_K_TYPE_BITFLD_L;
            }

            /*
             * If the length is greater than 32 and the bitfield is only a
             * longword, then change the type to a bitfield of quadword
             * size.
             */
            if ((member->item.type == SDL_K_TYPE_BITFLD_L) && (length > 32))
            {
                member->item.type = SDL_K_TYPE_BITFLD_Q;
            }

            /*
             * If the length is greater than 64 and the bitfield is only a
             * quadword, then change the type to a bitfield of octaword
             * size.
             */
            if ((member->item.type == SDL_K_TYPE_BITFLD_L) && (length > 64))
            {
                member->item.type = SDL_K_TYPE_BITFLD_Q;
            }
            member->item.size = sdl_sizeof(context, member->item.type);
            *updated = true;
        }
        else
        {
            member->item.type = prevMember->item.type;
            member->item.size = prevMember->item.size;
        }
    }
    else
    {
        newMember->item.type = prevMember->item.type;
        newMember->item.size = prevMember->item.size;
    }

    /*
     * Return back to the caller.
     */
    return;
}

/*
 * _sdl_create_bitfield_constants
 *  This function is called after an AGGREGATE or subaggregate is ENDed.  This
 *  function scans the provided member list and generates the constants needed
 *  by the BITFIELD definitions.  We do this at the end, because the actual
 *  size of a BITFIELD may not be fully known until the AGGREGATE or
 *  subaggregate is ENDed.
 *
 * Input Parameters:
 *  context:
 *    A pointer to the context structure where we maintain information about
 *    the current state of the parsing.
 *  memberList:
 *    A pointer to the member list header.  This is used to determine when we
 *    have recursed back to the beginning of the list.
 *
 * Output Parameters:
 *  None.
 *
 * Return Values:
 *  SDL_NORMAL:     Normal Successful Completion.
 *  SDL_ABORT:      An unexpected error occurred.
 *  SDL_ERREXIT:    Error exit.
 */
static uint32_t _sdl_create_bitfield_constants(
        SDL_CONTEXT *context,
        SDL_QUEUE *memberList)
{
    SDL_MEMBERS *member = (SDL_MEMBERS *) memberList->flink;
    SDL_CONSTANT *constDef;
    uint32_t retVal = SDL_NORMAL;

    /*
     * If tracing is turned on, write out this call (calls only, no returns).
     */
    if (trace == true)
    {
        printf("%s:%d:_sdl_check_bitfieldSizes\n", __FILE__, __LINE__);
    }

    /*
     * Loop through the members in the member list until we've processed all of
     * the BITFIELDS within the list and generated the CONSTANT definitions.
     */
    while ((member != (SDL_MEMBERS *) &memberList->flink) && (retVal == SDL_NORMAL))
    {
        if (sdl_isBitfield(member) == true)
        {

            /*
             * OK, we have everything we need.  We now need to create a SIZE
             * constant and potentially a MASK constant.
             */
            constDef = _sdl_create_constant(member->item.id,
                                            (member->item.prefix == NULL ?
                                                "" : member->item.prefix),
                                            (sdl_all_lower(member->item.id) ? "s" : "S"),
                                            NULL,
                                            NULL,
                                            SDL_K_RADIX_DEC,
                                            member->item.length,
                                            NULL,
                                            context->argument[ArgWordSize].value,
                                            &member->loc);
            if (constDef != NULL)
            {
                _sdl_queue_constant(context, constDef);
            }
            else
            {
                retVal = SDL_ABORT;
                if (sdl_set_message(msgVec,
                                    2,
                                    retVal,
                                    ENOMEM) != SDL_NORMAL)
                {
                    retVal = SDL_ERREXIT;
                }
            }

            if (member->item.mask == true)
            {
                uint64_t mask;

                /*
                 * Calculate the mask value.  It is 2 to the power of the
                 * number bits for the mask, minus one, and then left-shifted
                 * into the proper bit position.
                 */
                mask = ((uint64_t) pow((double) 2,
                                       (double) member->item.length) - 1) <<
                                               member->item.bitOffset;
                constDef = _sdl_create_constant(member->item.id,
                                                (member->item.prefix == NULL ?
                                                    "" : member->item.prefix),
                                                (sdl_all_lower(member->item.id) ? "m" : "M"),
                                                NULL,
                                                NULL,
                                                SDL_K_RADIX_HEX,
                                                mask,
                                                NULL,
                                                member->item.size,
                                                &member->loc);
                if (constDef != NULL)
                {
                    _sdl_queue_constant(context, constDef);
                }
                else
                {
                    retVal = SDL_ABORT;
                    if (sdl_set_message(msgVec,
                                        2,
                                        retVal,
                                        ENOMEM) != SDL_NORMAL)
                    {
                        retVal = SDL_ERREXIT;
                    }
                }
            }
        }

        /*
         * Move onto the next member in the list.
         */
        member = (SDL_MEMBERS *) member->header.queue.flink;
    }

    /*
     * Return the results of the call back to the caller.
     */
    return(retVal);
}




