/* GOB C Preprocessor
 * Copyright (C) 1999-2000 the Free Software Foundation.
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2001-2011 George (Jiri) Lebl
 *
 * Author: George Lebl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the  Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 */
%{

#include "config.h" 
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <glib.h>

#include "treefuncs.h"
#include "parse.h"
#include "main.h"
#include "util.h"

static int parenth_depth = 0;
static int before_comment
/* New flex is on drugs */
#if defined(FLEX_SCANNER) && ! defined(INITIAL)
	= 0;
#else
	= INITIAL;
#endif
static gboolean class_after_c = FALSE;
static int code_type = CCODE;
static int before_string;
static int property_paren_depth = 0;

/* GTK+ doc stuff */
static char *gtk_doc_func = NULL; /* current gtk-doc func */
GHashTable *gtk_doc_hash = NULL;

static GString *cbuf = NULL;
int ccode_line = 1;

GList *include_files = NULL;
/* 0 no, 1 means yes, 2+ means don't even start looking anymore */
static int look_for_includes = 0;

int line_no = 1;
/* last filename parsed from a #line directive */
char *hline_filename = NULL;

static void
clear_cbuf(void)
{
	if(!cbuf) {
		cbuf = g_string_new(NULL);
	} else {
		cbuf = g_string_assign(cbuf, "");
	}
}

static void
add_to_cbuf(char *s)
{
	if(!cbuf) {
		cbuf = g_string_new(s);
	} else {
		cbuf = g_string_append(cbuf,s);
	}
}

static void
add_gtk_doc_func(void)
{
	if(!gtk_doc_func)
		return;

	if(!gtk_doc_hash)
		gtk_doc_hash = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(gtk_doc_hash, gtk_doc_func, g_strdup(cbuf->str));
	clear_cbuf();

	gtk_doc_func = NULL;
}

/* Ugly warning / error avoidings */
#ifdef FLEX_SCANNER
int yylex(void);
int yywrap(void) { return 1; }
#endif

%}

%x COMMENT
%x C_CODE
%x CODE_STRING
%x CLASS_CODE
%x CLASS_STRING
%x CLASS_CODE_I
%x PROPERTY_CODE
%x PROPERTY_CODE_I
%x GTK_DOC_BEFORE_NAME
%x GTK_DOC
%x GTK_DOC_LINE



%%

%{
/* Avoid warning from flex, cuz flex sucks */
#ifdef FLEX_SCANNER
yy_current_state = 0;
#endif
%}


<*>\n			{ line_no++; REJECT; }

<COMMENT>^(I(S.RI).E\(([1-9][0-9]+|[2-9]))/(\))	{
/* Thy evil easter egg */
#define QQ(x) long x
#define KK(x) =atoi(__(&,,x,))
#define MM(x,a) {QQ(i);for(i=2;i<x;i++){a}}
#define PP(Q) (P%Q)==0
#define ___(x,y,z) if(x z y)
#define __(a,x,y,z) a(yytext[y] x z)
#define O__O(a,b) fprintf(stderr,"%s is %s!\n",a,b)
QQ(m)=1;___(__(,==,2,'P'),__(,==,5,'M'),&&
){QQ(P)KK(8);MM(P,___(PP(i),,)m=0;)}__(,=,
7,0);___(,,m){O__O(__( &,,8,),__(&,,2,));}
#undef QQ
#undef KK
#undef MM
#undef PP
#undef ___
#undef __
#undef O__O
REJECT;
}

\/\/.*$			{ ; /*comment, ignore*/ }

<*>^#[ \t]*line[ \t]+[0-9]+([ \t]\"[^\n\r\f\"]*\")? {
	char *p;
	char *number;
	char *filename;
	char *str=g_strdup(yytext);

	/* find first digit of line number */
	p=str;
	while(*p&&!isdigit(*p)) p++;
	number=p;
  
	/* find end of line number */
	while(*p&&isdigit(*p)) p++;
	if(*p) *p++=0;

	/* find beginning of filename */
	p=strchr(p,'"');
	if(p) p++;
	filename=p;

	/* find end of filename */
	if(p) p=strchr(p,'"');
	if(p) *p=0;

	/* stash number (minus one because we don't count this line) */  
	if(number) line_no=atoi(number)-1;

	/* stash filename */
	if(filename) {
		if(hline_filename) g_free(hline_filename);
		hline_filename=g_strdup(filename);
	}
  
	/* clean up */
	g_free(str);
}

<C_CODE>^#[ \t]*include[ \t][<"][^\n\r">]*[>"] {
	if(look_for_includes==1) {
		char *p;
		char *file;
		char *str = g_strdup(yytext);
		file = strchr(str,'"');
		if(!file) file = strchr(str,'<');
		file++;
		p = strchr(file,'"');
		if(!p) p = strchr(file,'>');
		if (p != NULL) {
			*p = '\0';
			include_files = g_list_prepend(include_files,g_strdup(file));
			g_free(str);
		}
	}
	REJECT;
}

<CLASS_CODE_I>\/\*\*[ \t]*$	{
			/* eat out gtk doc stuff */
			BEGIN(GTK_DOC_BEFORE_NAME);
			clear_cbuf();
		}
<GTK_DOC_BEFORE_NAME>^[ \t]*\*[ \t]*$ 	{
			/* empty doc lines */
			;
		}	
<GTK_DOC_BEFORE_NAME>^[ \t]*\*\ [_a-zA-Z][_a-zA-Z0-9]*:?[ \t]*$	{
			char *p;
			BEGIN(GTK_DOC);
			p = strchr(yytext, '*');
			g_free(gtk_doc_func);
			gtk_doc_func = g_strdup(p+2);
			p = strchr(gtk_doc_func, ':');
			if(p) *p='\0';
			g_strstrip(gtk_doc_func);
		}
<GTK_DOC_BEFORE_NAME>\*\/	{
			BEGIN(CLASS_CODE_I);
		}
<GTK_DOC_BEFORE_NAME>.	{
			BEGIN(COMMENT);
			before_comment = CLASS_CODE_I;
		}
<GTK_DOC>^[ \t]*\*[ \t]*$ 	{
			/* empty doc lines */
			add_to_cbuf(" *\n");
		}	
<GTK_DOC>^[ \t]*\*?\*\/	{
			BEGIN(CLASS_CODE_I);
			add_gtk_doc_func();
		}
<GTK_DOC>^[ \t]*\*[ \t]	{
			fflush(stdout);
			add_to_cbuf(" * ");
			BEGIN(GTK_DOC_LINE);
		}
<GTK_DOC>\*\/	{
			BEGIN(CLASS_CODE_I);
		}
<GTK_DOC>.	{
			BEGIN(COMMENT);
			before_comment = CLASS_CODE_I;
		}
<GTK_DOC_LINE>\*\/	{
			BEGIN(CLASS_CODE_I);
			add_to_cbuf("\n");
			add_gtk_doc_func();
		}
<GTK_DOC_LINE>.$	{
			BEGIN(GTK_DOC);
			add_to_cbuf(yytext);
			add_to_cbuf("\n");
		}
<GTK_DOC_LINE>.	{
			fflush(stdout);
			add_to_cbuf(yytext);
		}

<C_CODE>\/\/.*$		{ add_to_cbuf(yytext); /*comment, ignore*/ }
<CLASS_CODE>\/\/.*$	{ ; /*comment, ignore*/ }
<CLASS_CODE_I>\/\/.*$	{ ; /*comment, ignore*/ }
<PROPERTY_CODE_I>\/\/.*$	{ ; /*comment, ignore*/ }
\/\*		{BEGIN(COMMENT); before_comment = INITIAL; }
<C_CODE>\/\*	{
	add_to_cbuf(yytext);
	BEGIN(COMMENT);
	before_comment = C_CODE;
}
<CLASS_CODE>\/\*	{BEGIN(COMMENT); before_comment = CLASS_CODE; }
<CLASS_CODE_I>\/\*	{BEGIN(COMMENT); before_comment = CLASS_CODE_I; }
<PROPERTY_CODE_I>\/\*	{BEGIN(COMMENT); before_comment = PROPERTY_CODE_I; }
<COMMENT>\*\/	{
	if(before_comment == C_CODE) add_to_cbuf(yytext);
	BEGIN(before_comment);
		}
<COMMENT>.	{
	/* comment, ignore */
	if(before_comment == C_CODE) add_to_cbuf(yytext);
		}
<COMMENT>\n	{
	/* comment, ignore */
	if(before_comment == C_CODE) add_to_cbuf(yytext);
		}

^\%(a|all)\{		{
			BEGIN(C_CODE);
			parenth_depth = 1;
			class_after_c = FALSE;
			code_type = ACODE;
			clear_cbuf();
			ccode_line = line_no;
		}
^\%(at|alltop)\{		{
			BEGIN(C_CODE);
			parenth_depth = 1;
			class_after_c = FALSE;
			code_type = ATCODE;
			clear_cbuf();
			ccode_line = line_no;
		}

^\%(ht|headertop)\{		{
			BEGIN(C_CODE);
			parenth_depth = 1;
			class_after_c = FALSE;
			code_type = HTCODE;
			clear_cbuf();
			ccode_line = line_no;
		}
^\%(ph|privateheader)\{		{
			BEGIN(C_CODE);
			parenth_depth = 1;
			class_after_c = FALSE;
			code_type = PHCODE;
			clear_cbuf();
			ccode_line = line_no;
		}
^\%(h|header)\{		{
			BEGIN(C_CODE);
			parenth_depth = 1;
			class_after_c = FALSE;
			code_type = HCODE;
			clear_cbuf();
			ccode_line = line_no;
		}
^\%\{		{
			BEGIN(C_CODE);
			parenth_depth = 1;
			class_after_c = FALSE;
			code_type = CCODE;
			clear_cbuf();
			ccode_line = line_no;
			if(look_for_includes==0)
				look_for_includes=1;
		}
^\%(ct|ctop)\{		{
			BEGIN(C_CODE);
			parenth_depth = 1;
			class_after_c = FALSE;
			code_type = CTCODE;
			clear_cbuf();
			ccode_line = line_no;
			if(look_for_includes==0)
				look_for_includes=1;
		}
^\%(ad|afterdecls)\{		{
			BEGIN(C_CODE);
			parenth_depth = 1;
			class_after_c = FALSE;
			code_type = ADCODE;
			clear_cbuf();
			ccode_line = line_no;
		}
<C_CODE>^\%\}	{
			BEGIN(INITIAL);
			yylval.cbuf = cbuf;
			cbuf = NULL;
			if(look_for_includes==1)
				look_for_includes=0;
			return code_type;
		}

<C_CODE>\'\{\'		{ add_to_cbuf(yytext); }
<C_CODE>\'\\\{\'	{ add_to_cbuf(yytext); }
<C_CODE>\'\}\'		{ add_to_cbuf(yytext); }
<C_CODE>\'\\\}\'	{ add_to_cbuf(yytext); }
<C_CODE>\'\"\'		{ add_to_cbuf(yytext); }
<C_CODE>\'\\\"\'	{ add_to_cbuf(yytext); }
	
<C_CODE>\\.	{ add_to_cbuf(yytext); }


<C_CODE>\"	{
			BEGIN(CODE_STRING);
			before_string = C_CODE;
			add_to_cbuf(yytext);
		}
<PROPERTY_CODE_I>\"	{
			BEGIN(CODE_STRING);
			before_string = PROPERTY_CODE_I;
			add_to_cbuf(yytext);
		}
<CODE_STRING>\\.	{ add_to_cbuf(yytext); }
<CODE_STRING>\"	{
			BEGIN(before_string);
			add_to_cbuf(yytext);
			if (before_string == PROPERTY_CODE_I) {
				yylval.id = cbuf->str;
				g_string_free (cbuf, FALSE);
				cbuf = NULL;
				return STRING;
			}
		}
<CODE_STRING>.	{ add_to_cbuf(yytext); }
<CODE_STRING>\n	{ add_to_cbuf(yytext); }

<C_CODE>\{	{
			parenth_depth++;
			add_to_cbuf(yytext);
		}
<C_CODE>\}	{
			parenth_depth--;
			if(parenth_depth<0) {
				REJECT;
			} else if(parenth_depth==0 && class_after_c) {
				BEGIN(CLASS_CODE_I);
				yylval.cbuf = cbuf;
				cbuf = NULL;
				return CCODE;
			}
			add_to_cbuf(yytext);
		}

<C_CODE>.	{ add_to_cbuf(yytext); }
<C_CODE>\n	{ add_to_cbuf(yytext); }

class		{
			static int found_classes = 0;
			look_for_includes = 2;
			BEGIN(CLASS_CODE);

			if(++found_classes > 1) {
				error_print(GOB_ERROR, line_no,
					    "Only one class per file allowed");
			}

			return CLASS;
		}

error		{ return ERROR; }
enum		{ return ENUM; }
flags		{ return FLAGS; }

^[ \t]*requires[ \t]+[0-9]+\.[0-9]+\.[0-9]+[\t ]*$	{
			int maj = 0, min = 0, pl = 0;
			int rmaj = 0, rmin = 0, rpl = 0;
			int effective_maj = 0;
			int effective_rmaj = 0;
			char *p;
			
			sscanf (VERSION, "%d.%d.%d", &rmaj, &rmin, &rpl);
			effective_rmaj = rmaj;
			if (rmin >= 90)
				effective_rmaj = rmaj + 1;

			p = strchr (yytext,'r');
			g_assert (p); /* we MUST have found it */
			sscanf (p, "requires %d.%d.%d", &maj, &min, &pl);
			effective_maj = maj;
			if (min >= 90)
				effective_maj = maj + 1;

			if(rmaj < maj ||
			   (rmaj == maj && rmin < min) ||
			   (rmaj == maj && rmin == min && rpl < pl)) {
				error_printf (GOB_ERROR, line_no,
					      "GOB version at least %d.%d.%d required "
					      "(this is %s)\n"
					      "To upgrade your gob, see: "
					      "http://www.5z.com/jirka/gob.html",
					      maj, min, pl, VERSION);
			}

			if(effective_rmaj != effective_maj) {
				error_printf(GOB_ERROR, line_no,
					     "GOB major version %d required "
					     "(this is %s)\n"
					     "To upgrade your gob, see: "
					     "http://www.5z.com/jirka/gob.html",
					     effective_maj, VERSION);
			}

		}

<CLASS_CODE,CLASS_CODE_I>class|this	{
			if(for_cpp) {
				error_printf(GOB_WARN, line_no,
					     "'%s' keyword should not "
					     "be used when generating "
					     "C++ code", yytext);
			}
			REJECT;
		}

<CLASS_CODE>from	{return FROM;}
<CLASS_CODE>\"	{
			BEGIN(CLASS_STRING);
			before_string = CLASS_CODE;
			add_to_cbuf(yytext);
		}
<CLASS_STRING>\\.	{ add_to_cbuf(yytext); }
<CLASS_STRING>\"	{
			BEGIN(before_string);
			add_to_cbuf(yytext);
			yylval.id = cbuf->str;
			g_string_free (cbuf, FALSE);
			cbuf = NULL;
			return STRING;
		}
<CLASS_STRING>.	{ add_to_cbuf(yytext); }
<CLASS_STRING>\n	{ add_to_cbuf(yytext); }

<CLASS_CODE_I>\"	{
			BEGIN(CLASS_STRING);
			before_string = CLASS_CODE_I;
			add_to_cbuf(yytext);
		}

<CLASS_CODE_I,PROPERTY_CODE_I>void	{return VOID;}
<CLASS_CODE_I,PROPERTY_CODE_I>struct	{return STRUCT;}
<CLASS_CODE_I,PROPERTY_CODE_I>union	{return UNION;}
<CLASS_CODE_I,PROPERTY_CODE_I>enum	{return ENUM;}
<CLASS_CODE_I,PROPERTY_CODE_I>signed	{return SIGNED;}
<CLASS_CODE_I,PROPERTY_CODE_I>unsigned	{return UNSIGNED;}
<CLASS_CODE_I,PROPERTY_CODE_I>long	{return LONG;}
<CLASS_CODE_I,PROPERTY_CODE_I>short	{return SHORT;}
<CLASS_CODE_I,PROPERTY_CODE_I>int	{return INT;}
<CLASS_CODE_I,PROPERTY_CODE_I>float	{return FLOAT;}
<CLASS_CODE_I,PROPERTY_CODE_I>double	{return DOUBLE;}
<CLASS_CODE_I,PROPERTY_CODE_I>char	{return CHAR;}
<CLASS_CODE_I,PROPERTY_CODE_I>const	{return CONST;}

<CLASS_CODE_I>\.\.\.	{return THREEDOTS;}

<CLASS_CODE_I>public	{yylval.line = line_no; return PUBLIC;}
<CLASS_CODE_I>private	{yylval.line = line_no; return PRIVATE;}
<CLASS_CODE_I>protected	{yylval.line = line_no; return PROTECTED;}
<CLASS_CODE_I>classwide	{yylval.line = line_no; return CLASSWIDE;}
<CLASS_CODE_I>argument	{yylval.line = line_no; return ARGUMENT;}
<CLASS_CODE_I>virtual	{yylval.line = line_no; return VIRTUAL;}
<CLASS_CODE_I>signal	{yylval.line = line_no; return SIGNAL;}
<CLASS_CODE_I>override	{yylval.line = line_no; return OVERRIDE;}
<CLASS_CODE_I>property	{
				yylval.line = line_no;
				BEGIN(PROPERTY_CODE);
				return PROPERTY;
			}
<PROPERTY_CODE_I>nick	{ yylval.line = line_no; return NICK; }
<PROPERTY_CODE_I>name	{ yylval.line = line_no; return NAME; }
<PROPERTY_CODE_I>blurb	{ yylval.line = line_no; return BLURB; }
<PROPERTY_CODE_I>maximum	{ yylval.line = line_no; return MAXIMUM; }
<PROPERTY_CODE_I>minimum	{ yylval.line = line_no; return MINIMUM; }
<PROPERTY_CODE_I>default_value	{ yylval.line = line_no; return DEFAULT_VALUE; }
<PROPERTY_CODE_I>flags	{ yylval.line = line_no; return FLAGS; }
<PROPERTY_CODE_I>type	{ yylval.line = line_no; return TYPE; }
<PROPERTY_CODE_I>flags_type	{ yylval.line = line_no; return FLAGS_TYPE; }
<PROPERTY_CODE_I>enum_type	{ yylval.line = line_no; return ENUM_TYPE; }
<PROPERTY_CODE_I>param_type	{ yylval.line = line_no; return PARAM_TYPE; }
<PROPERTY_CODE_I>boxed_type	{ yylval.line = line_no; return BOXED_TYPE; }
<PROPERTY_CODE_I>object_type	{ yylval.line = line_no; return OBJECT_TYPE; }
<PROPERTY_CODE>[(]	{
		yylval.line = line_no;
		property_paren_depth = 1;
		BEGIN(PROPERTY_CODE_I);
		return '(';
			}
<PROPERTY_CODE_I>[(]	{
		yylval.line = line_no;
		property_paren_depth++;
		return '(';
			}
<PROPERTY_CODE_I>[)]	{
		yylval.line = line_no;
		property_paren_depth--;
		if (property_paren_depth == 0) {
			BEGIN(CLASS_CODE_I);
		}
		return ')';
			}

<CLASS_CODE,CLASS_CODE_I,PROPERTY_CODE,PROPERTY_CODE_I,INITIAL>0|[1-9][0-9]*|0x[0-9a-fA-F]+|0[0-7]+|[0-9]*\.[0-9]+|\.[0-9][0-9]*	{
			yylval.id = g_strdup(yytext);
			return NUMBER;
		}
<CLASS_CODE,CLASS_CODE_I,PROPERTY_CODE,PROPERTY_CODE_I,INITIAL>[A-Za-z_][A-Za-z0-9_]*(::[A-Za-z0-9_]*)+	{
			/* This is cpp kind of token thingie */
			if (for_cpp) {
				yylval.id = g_strdup(yytext);
				return TOKEN;
			} else {
				REJECT;
			}
		}
<CLASS_CODE,CLASS_CODE_I,PROPERTY_CODE,PROPERTY_CODE_I,INITIAL>[A-Za-z_][A-Za-z0-9_]*(:[A-Za-z_][A-Za-z0-9_]*)+	{
			/* this one is for a classname with a namespace */
			yylval.id = g_strdup(yytext);
			return TYPETOKEN;
		}
<CLASS_CODE,CLASS_CODE_I,PROPERTY_CODE,PROPERTY_CODE_I,INITIAL>:[A-Za-z_][A-Za-z0-9_]*(:[A-Za-z_][A-Za-z0-9_]*)*	{
			/* this is for a classname with an empty namespace */
			yylval.id = g_strdup(yytext);
			return TYPETOKEN;
		}
  <CLASS_CODE,CLASS_CODE_I,PROPERTY_CODE,PROPERTY_CODE_I,INITIAL>([A-Za-z0-9_]*::)?[A-Za-z_][A-Za-z0-9_]*(<[A-Za-z0-9<>,:]*>)?	{
			yylval.id = g_strdup(yytext);
			return TOKEN;
		}
<CLASS_CODE,CLASS_CODE_I,PROPERTY_CODE,PROPERTY_CODE_I,INITIAL>\'\\.\'|\'.\'	{
			yylval.id = g_strdup(yytext);
			return SINGLE_CHAR;
		}

<CLASS_CODE_I>(\[[0-9]*\]|\[[A-Za-z_][A-Za-z0-9_]*\])+	{
			yylval.id = g_strdup(yytext);
			return ARRAY_DIM;
		}
<CLASS_CODE_I>:[0-9]+ {
			/* cheat for bitfield */
			yylval.id = g_strdup(yytext);
			return ARRAY_DIM;
		}
<CLASS_CODE>\{	{
			BEGIN(CLASS_CODE_I);
			return '{';
		}
<CLASS_CODE_I,PROPERTY_CODE>\{	{
			BEGIN(C_CODE);
			parenth_depth=1;
			class_after_c = TRUE;
			yylval.line = line_no;
			clear_cbuf();
			ccode_line = line_no;
			return '{';
		}
<CLASS_CODE_I>\}	{
				BEGIN(INITIAL);
				return '}';
			}

<CLASS_CODE,CLASS_CODE_I,INITIAL,PROPERTY_CODE,PROPERTY_CODE_I>[\f\t ]	;  /*ignore*/

<*>[\n\r]	;  /*ignore*/


<*>.		{
			yylval.line = line_no;
			return yytext[0];
		}

^[ \t]*prealloc[ \t]+[0-9]+[ \t]*$ {
			char *p;
			int t;
			p = strchr (yytext,'p');
			g_assert (p); /* we MUST have found it */
			sscanf (p, "prealloc %d", &t);
			prealloc=t;
		}

%%
