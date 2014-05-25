/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2009,2010,2011,2012,2014, by the GROMACS development team, led by
 * Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
 * and including many others, as listed in the AUTHORS file in the
 * top-level source directory and at http://www.gromacs.org.
 *
 * GROMACS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * GROMACS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GROMACS; if not, see
 * http://www.gnu.org/licenses, or write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 * If you want to redistribute modifications to GROMACS, please
 * consider that scientific software is very special. Version
 * control is crucial - bugs must be traceable. We will be happy to
 * consider code for inclusion in the official distribution, but
 * derived work must not be called official GROMACS. Details are found
 * in the README & COPYING files - if they are missing, get the
 * official version at http://www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org.
 */
/*! \internal \file
 * \brief Helper functions for the selection tokenizer.
 *
 * This file implements the functions in the headers scanner.h and
 * scanner_internal.h.
 *
 * \author Teemu Murtola <teemu.murtola@gmail.com>
 * \ingroup module_selection
 */
/*! \cond
 * \internal \file scanner_flex.h
 * \brief Generated (from scanner.l) header file by Flex.
 *
 * This file contains definitions of functions that are needed in
 * scanner_internal.cpp.
 *
 * \ingroup module_selection
 * \endcond
 */
#include <stdlib.h>
#include <string.h>

#include <string>

#include "gromacs/utility/cstringutil.h"
#include "gromacs/utility/exceptions.h"
#include "gromacs/utility/gmxassert.h"
#include "gromacs/utility/messagestringcollector.h"
#include "gromacs/utility/smalloc.h"
#include "gromacs/utility/stringutil.h"

#include "parsetree.h"
#include "selectioncollection-impl.h"
#include "selelem.h"
#include "selmethod.h"
#include "symrec.h"

#include "parser.h"
#include "scanner.h"
#include "scanner_internal.h"

/*! \brief
 * Step in which the allocated memory for pretty-printed input is incremented.
 */
#define STRSTORE_ALLOCSTEP 1000

/* These are defined as macros in the generated scanner_flex.h.
 * We undefine them here to have them as variable names in the subroutines.
 * There are other ways of doing this, but this is probably the easiest. */
#undef yylval
#undef yytext
#undef yyleng

/*! \brief
 * Handles initialization of method parameter token.
 */
static int
init_param_token(YYSTYPE *yylval, gmx_ana_selparam_t *param, bool bBoolNo)
{
    if (bBoolNo)
    {
        GMX_RELEASE_ASSERT(param->name != NULL,
                           "bBoolNo should only be set for a parameters with a name");
        snew(yylval->str, strlen(param->name) + 3);
        yylval->str[0] = 'n';
        yylval->str[1] = 'o';
        strcpy(yylval->str+2, param->name);
    }
    else
    {
        yylval->str = param->name ? strdup(param->name) : NULL;
    }
    return PARAM;
}

/*! \brief
 * Processes a selection method token.
 */
static int
init_method_token(YYSTYPE *yylval, gmx_ana_selmethod_t *method, bool bPosMod,
                  gmx_sel_lexer_t *state)
{
    /* If the previous token was not KEYWORD_POS, return EMPTY_POSMOD
     * before the actual method to work around a limitation in Bison. */
    if (!bPosMod && method->type != POS_VALUE)
    {
        state->nextmethod = method;
        return EMPTY_POSMOD;
    }
    yylval->meth = method;
    if (!(method->flags & SMETH_MODIFIER) && method->nparams == 0)
    {
        /* Keyword */
        switch (method->type)
        {
            case INT_VALUE:   return KEYWORD_NUMERIC;
            case REAL_VALUE:  return KEYWORD_NUMERIC;
            case STR_VALUE:   return KEYWORD_STR;
            case GROUP_VALUE: return KEYWORD_GROUP;
            default:
                GMX_THROW(gmx::InternalError("Unsupported keyword type"));
        }
    }
    else
    {
        /* Method with parameters or a modifier */
        if (method->flags & SMETH_MODIFIER)
        {
            /* Remove all methods from the stack */
            state->msp = -1;
            if (method->param[1].name == NULL)
            {
                state->nextparam = &method->param[1];
            }
        }
        else
        {
            if (method->param[0].name == NULL)
            {
                state->nextparam = &method->param[0];
            }
        }
        ++state->msp;
        if (state->msp >= state->mstack_alloc)
        {
            state->mstack_alloc += 10;
            srenew(state->mstack, state->mstack_alloc);
        }
        state->mstack[state->msp] = method;
        if (method->flags & SMETH_MODIFIER)
        {
            return MODIFIER;
        }
        switch (method->type)
        {
            case INT_VALUE:   return METHOD_NUMERIC;
            case REAL_VALUE:  return METHOD_NUMERIC;
            case POS_VALUE:   return METHOD_POS;
            case GROUP_VALUE: return METHOD_GROUP;
            default:
                --state->msp;
                GMX_THROW(gmx::InternalError("Unsupported method type"));
        }
    }
    return INVALID; /* Should not be reached */
}

int
_gmx_sel_lexer_process_pending(YYSTYPE *yylval, gmx_sel_lexer_t *state)
{
    if (state->nextparam)
    {
        gmx_ana_selparam_t *param   = state->nextparam;
        bool                bBoolNo = state->bBoolNo;

        if (state->neom > 0)
        {
            --state->neom;
            return END_OF_METHOD;
        }
        state->nextparam = NULL;
        state->bBoolNo   = false;
        _gmx_sel_lexer_add_token(param->name, -1, state);
        return init_param_token(yylval, param, bBoolNo);
    }
    if (state->prev_pos_kw > 0)
    {
        --state->prev_pos_kw;
    }
    if (state->nextmethod)
    {
        gmx_ana_selmethod_t *method = state->nextmethod;

        state->nextmethod = NULL;
        return init_method_token(yylval, method, true, state);
    }
    return 0;
}

int
_gmx_sel_lexer_process_identifier(YYSTYPE *yylval, char *yytext, size_t yyleng,
                                  gmx_sel_lexer_t *state)
{
    /* Check if the identifier matches with a parameter name */
    if (state->msp >= 0)
    {
        gmx_ana_selparam_t *param   = NULL;
        bool                bBoolNo = false;
        int                 sp      = state->msp;
        while (!param && sp >= 0)
        {
            int             i;
            for (i = 0; i < state->mstack[sp]->nparams; ++i)
            {
                /* Skip NULL parameters and too long parameters */
                if (state->mstack[sp]->param[i].name == NULL
                    || strlen(state->mstack[sp]->param[i].name) > yyleng)
                {
                    continue;
                }
                if (!strncmp(state->mstack[sp]->param[i].name, yytext, yyleng))
                {
                    param = &state->mstack[sp]->param[i];
                    break;
                }
                /* Check separately for a 'no' prefix on boolean parameters */
                if (state->mstack[sp]->param[i].val.type == NO_VALUE
                    && yyleng > 2 && yytext[0] == 'n' && yytext[1] == 'o'
                    && !strncmp(state->mstack[sp]->param[i].name, yytext+2, yyleng-2))
                {
                    param   = &state->mstack[sp]->param[i];
                    bBoolNo = true;
                    break;
                }
            }
            if (!param)
            {
                --sp;
            }
        }
        if (param)
        {
            if (param->val.type == NO_VALUE && !bBoolNo)
            {
                state->bMatchBool = true;
            }
            if (sp < state->msp)
            {
                state->neom      = state->msp - sp - 1;
                state->nextparam = param;
                state->bBoolNo   = bBoolNo;
                return END_OF_METHOD;
            }
            _gmx_sel_lexer_add_token(param->name, -1, state);
            return init_param_token(yylval, param, bBoolNo);
        }
    }

    /* Check if the identifier matches with a symbol */
    const gmx::SelectionParserSymbol *symbol
        = state->sc->symtab->findSymbol(std::string(yytext, yyleng), false);
    /* If there is no match, return the token as a string */
    if (!symbol)
    {
        yylval->str = gmx_strndup(yytext, yyleng);
        _gmx_sel_lexer_add_token(yytext, yyleng, state);
        return IDENTIFIER;
    }
    _gmx_sel_lexer_add_token(symbol->name().c_str(), -1, state);
    gmx::SelectionParserSymbol::SymbolType symtype = symbol->type();
    /* Reserved symbols should have been caught earlier */
    if (symtype == gmx::SelectionParserSymbol::ReservedSymbol)
    {
        GMX_THROW(gmx::InternalError(gmx::formatString(
                                             "Mismatch between tokenizer and reserved symbol table (for '%s')",
                                             symbol->name().c_str())));
    }
    /* For variable symbols, return the type of the variable value */
    if (symtype == gmx::SelectionParserSymbol::VariableSymbol)
    {
        gmx::SelectionTreeElementPointer var = symbol->variableValue();
        /* Return simple tokens for constant variables */
        if (var->type == SEL_CONST)
        {
            switch (var->v.type)
            {
                case INT_VALUE:
                    yylval->i = var->v.u.i[0];
                    return TOK_INT;
                case REAL_VALUE:
                    yylval->r = var->v.u.r[0];
                    return TOK_REAL;
                case POS_VALUE:
                    break;
                default:
                    GMX_THROW(gmx::InternalError("Unsupported variable type"));
            }
        }
        yylval->sel = new gmx::SelectionTreeElementPointer(var);
        switch (var->v.type)
        {
            case INT_VALUE:   return VARIABLE_NUMERIC;
            case REAL_VALUE:  return VARIABLE_NUMERIC;
            case POS_VALUE:   return VARIABLE_POS;
            case GROUP_VALUE: return VARIABLE_GROUP;
            default:
                delete yylval->sel;
                GMX_THROW(gmx::InternalError("Unsupported variable type"));
                return INVALID;
        }
        delete yylval->sel;
        return INVALID; /* Should not be reached. */
    }
    /* For method symbols, return the correct type */
    if (symtype == gmx::SelectionParserSymbol::MethodSymbol)
    {
        gmx_ana_selmethod_t *method = symbol->methodValue();
        return init_method_token(yylval, method, state->prev_pos_kw > 0, state);
    }
    /* For position symbols, we need to return KEYWORD_POS, but we also need
     * some additional handling. */
    if (symtype == gmx::SelectionParserSymbol::PositionSymbol)
    {
        state->bMatchOf    = true;
        yylval->str        = strdup(symbol->name().c_str());
        state->prev_pos_kw = 2;
        return KEYWORD_POS;
    }
    /* Should not be reached */
    return INVALID;
}

void
_gmx_sel_lexer_add_token(const char *str, int len, gmx_sel_lexer_t *state)
{
    /* Do nothing if the string is empty, or if it is a space and there is
     * no other text yet, or if there already is a space. */
    if (!str || len == 0 || strlen(str) == 0
        || (str[0] == ' ' && str[1] == 0
            && (state->pslen == 0 || state->pselstr[state->pslen - 1] == ' ')))
    {
        return;
    }
    if (len < 0)
    {
        len = strlen(str);
    }
    /* Allocate more memory if necessary */
    if (state->nalloc_psel - state->pslen < len)
    {
        int incr = STRSTORE_ALLOCSTEP < len ? len : STRSTORE_ALLOCSTEP;
        state->nalloc_psel += incr;
        srenew(state->pselstr, state->nalloc_psel);
    }
    /* Append the token to the stored string */
    strncpy(state->pselstr + state->pslen, str, len);
    state->pslen                += len;
    state->pselstr[state->pslen] = 0;
}

void
_gmx_sel_init_lexer(yyscan_t *scannerp, struct gmx_ana_selcollection_t *sc,
                    bool bInteractive, int maxnr, bool bGroups,
                    struct gmx_ana_indexgrps_t *grps)
{
    int rc = _gmx_sel_yylex_init(scannerp);
    if (rc != 0)
    {
        // TODO: Throw a more representative exception.
        GMX_THROW(gmx::InternalError("Lexer initialization failed"));
    }

    gmx_sel_lexer_t *state = new gmx_sel_lexer_t;

    state->sc        = sc;
    state->errors    = NULL;
    state->bGroups   = bGroups;
    state->grps      = grps;
    state->nexpsel   = (maxnr > 0 ? static_cast<int>(sc->sel.size()) + maxnr : -1);

    state->bInteractive = bInteractive;

    snew(state->pselstr, STRSTORE_ALLOCSTEP);
    state->pselstr[0]   = 0;
    state->pslen        = 0;
    state->nalloc_psel  = STRSTORE_ALLOCSTEP;

    snew(state->mstack, 20);
    state->mstack_alloc = 20;
    state->msp          = -1;
    state->neom         = 0;
    state->nextparam    = NULL;
    state->nextmethod   = NULL;
    state->prev_pos_kw  = 0;
    state->bBoolNo      = false;
    state->bMatchOf     = false;
    state->bMatchBool   = false;
    state->bCmdStart    = true;
    state->bBuffer      = false;

    _gmx_sel_yyset_extra(state, *scannerp);
}

void
_gmx_sel_free_lexer(yyscan_t scanner)
{
    gmx_sel_lexer_t *state = _gmx_sel_yyget_extra(scanner);

    sfree(state->pselstr);
    sfree(state->mstack);
    if (state->bBuffer)
    {
        _gmx_sel_yy_delete_buffer(state->buffer, scanner);
    }
    delete state;
    _gmx_sel_yylex_destroy(scanner);
}

void
_gmx_sel_set_lexer_error_reporter(yyscan_t                     scanner,
                                  gmx::MessageStringCollector *errors)
{
    gmx_sel_lexer_t *state = _gmx_sel_yyget_extra(scanner);
    state->errors = errors;
}

void
_gmx_sel_lexer_set_exception(yyscan_t                    scanner,
                             const boost::exception_ptr &ex)
{
    gmx_sel_lexer_t *state = _gmx_sel_yyget_extra(scanner);
    state->exception = ex;
}

void
_gmx_sel_lexer_rethrow_exception_if_occurred(yyscan_t scanner)
{
    gmx_sel_lexer_t *state = _gmx_sel_yyget_extra(scanner);
    if (state->exception)
    {
        boost::exception_ptr ex = state->exception;
        state->exception = boost::exception_ptr();
        rethrow_exception(ex);
    }
}

bool
_gmx_sel_is_lexer_interactive(yyscan_t scanner)
{
    gmx_sel_lexer_t *state = _gmx_sel_yyget_extra(scanner);
    return state->bInteractive;
}

struct gmx_ana_selcollection_t *
_gmx_sel_lexer_selcollection(yyscan_t scanner)
{
    gmx_sel_lexer_t *state = _gmx_sel_yyget_extra(scanner);
    return state->sc;
}

gmx::MessageStringCollector *
_gmx_sel_lexer_error_reporter(yyscan_t scanner)
{
    gmx_sel_lexer_t *state = _gmx_sel_yyget_extra(scanner);
    GMX_RELEASE_ASSERT(state->errors != NULL, "Error reporter not set");
    return state->errors;
}

bool
_gmx_sel_lexer_has_groups_set(yyscan_t scanner)
{
    gmx_sel_lexer_t *state = _gmx_sel_yyget_extra(scanner);
    return state->bGroups;
}

struct gmx_ana_indexgrps_t *
_gmx_sel_lexer_indexgrps(yyscan_t scanner)
{
    gmx_sel_lexer_t *state = _gmx_sel_yyget_extra(scanner);
    return state->grps;
}

int
_gmx_sel_lexer_exp_selcount(yyscan_t scanner)
{
    gmx_sel_lexer_t *state = _gmx_sel_yyget_extra(scanner);
    return state->nexpsel;
}

const char *
_gmx_sel_lexer_pselstr(yyscan_t scanner)
{
    gmx_sel_lexer_t *state = _gmx_sel_yyget_extra(scanner);
    return state->pselstr;
}

void
_gmx_sel_lexer_clear_pselstr(yyscan_t scanner)
{
    gmx_sel_lexer_t *state = _gmx_sel_yyget_extra(scanner);
    state->pselstr[0] = 0;
    state->pslen      = 0;
}

void
_gmx_sel_lexer_clear_method_stack(yyscan_t scanner)
{
    gmx_sel_lexer_t *state = _gmx_sel_yyget_extra(scanner);

    state->msp = -1;
}

void
_gmx_sel_finish_method(yyscan_t scanner)
{
    gmx_sel_lexer_t *state = _gmx_sel_yyget_extra(scanner);

    if (state->msp >= 0)
    {
        --state->msp;
    }
}

void
_gmx_sel_set_lex_input_file(yyscan_t scanner, FILE *fp)
{
    gmx_sel_lexer_t *state = _gmx_sel_yyget_extra(scanner);

    state->bBuffer = true;
    state->buffer  = _gmx_sel_yy_create_buffer(fp, YY_BUF_SIZE, scanner);
    _gmx_sel_yy_switch_to_buffer(state->buffer, scanner);
}

void
_gmx_sel_set_lex_input_str(yyscan_t scanner, const char *str)
{
    gmx_sel_lexer_t *state = _gmx_sel_yyget_extra(scanner);

    if (state->bBuffer)
    {
        _gmx_sel_yy_delete_buffer(state->buffer, scanner);
    }
    state->bBuffer = true;
    state->buffer  = _gmx_sel_yy_scan_string(str, scanner);
}
