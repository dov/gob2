/* A Bison parser, made by GNU Bison 2.7.  */

/* Bison implementation for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2012 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.7"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
/* Line 371 of yacc.c  */
#line 23 "parse.y"


#include "config.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "treefuncs.h"
#include "main.h"
#include "util.h"

/* FIXME: add gettext support */
#define _(x) (x)
	
GList *nodes = NULL;

static GList *class_nodes = NULL;
Node *class = NULL;
GList *enums = NULL;
static GList *enum_vals = NULL;
static GList *flag_vals = NULL;
static GList *error_vals = NULL;

static gboolean abstract = FALSE;
static char *chunk_size = NULL;
static char *bonobo_object_class = NULL;
static int glade_xml = FALSE;
static GList *interfaces = NULL;
static GList *typestack = NULL;
static GList *funcargs = NULL;
static GList *checks = NULL;
static int has_self = FALSE;
static int vararg = FALSE;
static Method *last_added_method = NULL;

/* destructor and initializer for variables */
static gboolean destructor_unref = FALSE;
static char *destructor = NULL;
static int destructor_line = 0;
static gboolean destructor_simple = TRUE;
static char *initializer = NULL;
static int initializer_line = 0;
static int glade_widget = FALSE;

static char *funcattrs = NULL;
static char *onerror = NULL;
static char *defreturn = NULL;

static GList *gtktypes = NULL;
static char *signal_name=NULL;

static Property *property = NULL;

/* this can be a global as we will only do one function at a time
   anyway */
static int the_scope = NO_SCOPE;

void free(void *ptr);
int yylex(void);

extern int ccode_line;
extern int line_no;
extern gboolean for_cpp;

extern char *yytext;

static void
yyerror(char *str)
{
	char *out=NULL;
	char *p;
	
	if (strcmp (yytext, "\n") == 0 ||
	    strcmp (yytext, "\r") == 0) {
		out = g_strconcat ("Error: ", str, " before end of line", NULL);
	} else if (yytext[0] == '\0') {
		out=g_strconcat("Error: ", str, " at end of input", NULL);
	} else {
		char *tmp = g_strdup(yytext);
		while((p=strchr(tmp, '\n')))
			*p='.';

		out=g_strconcat("Error: ", str, " before '", tmp, "'", NULL);
		g_free(tmp);
	}

	fprintf(stderr, "%s:%d: %s\n", filename, line_no, out);
	g_free(out);
	
	exit(1);
}

static Type *
pop_type(void)
{
	Type *type = typestack->data;
	typestack = g_list_remove(typestack,typestack->data);
	return type;
}

static void
push_variable (char *name, int scope, int line_no, char *postfix)
{
	Node *var;
	Type *type = pop_type ();

	type->postfix = postfix;
	
	var = node_new (VARIABLE_NODE,
			"scope", scope,
			"vtype:steal", type,
			"glade_widget", glade_widget,
			"id:steal", name,
			"line_no", line_no,
			"destructor_unref", destructor_unref,
			"destructor:steal", destructor,
			"destructor_line", destructor_line,
			"destructor_simple", destructor_simple,
			"initializer:steal", initializer,
			"initializer_line", initializer_line,
			"initializer_simple", TRUE,
			NULL);
	class_nodes = g_list_append(class_nodes, var);
	glade_widget = FALSE;
}

static void
push_function (int scope, int method, char *oid, char *id,
	       GString *cbuf, int line_no, int ccode_line,
	       gboolean vararg, GList *flags)
{
	Node *node;
	Type *type;
	char *c_cbuf;

	g_assert(scope != CLASS_SCOPE);
       
	if(method == INIT_METHOD ||
	   method == CLASS_INIT_METHOD ||
	   method == CONSTRUCTOR_METHOD ||
	   method == DISPOSE_METHOD ||
	   method == FINALIZE_METHOD) {
		type = (Type *)node_new (TYPE_NODE,
					 "name", "void",
					 NULL);
	} else {
		type = pop_type();
	}
	
	/* a complicated and ugly test to figure out if we have
	   the wrong number of types for a signal */
	if((method == SIGNAL_FIRST_METHOD ||
	    method == SIGNAL_LAST_METHOD) &&
	   g_list_length(gtktypes) != g_list_length(funcargs) &&
	   !(g_list_length(funcargs) == 1 &&
	     g_list_length(gtktypes) == 2 &&
	     strcmp(gtktypes->next->data, "NONE")==0)) {
		error_print(GOB_WARN, line_no,
			    _("The number of GTK arguments and "
			      "function arguments for a signal "
			      "don't seem to match"));
	}
	if(g_list_length(gtktypes) > 2) {
		GList *li;
		for(li = gtktypes->next; li; li = li->next) {
			if(strcmp(li->data, "NONE")==0) {
				error_print(GOB_ERROR, line_no,
					    _("NONE can only appear in an "
					      "argument list by itself"));
			}
		}
	}
	if(cbuf) {
		char *p;
		c_cbuf = p = cbuf->str;
		while(p && *p && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
			p++;
		if(!p || !*p)
			c_cbuf = NULL;
	} else
		c_cbuf = NULL;

	if (signal_name == NULL )
	{
		GString * buffer=g_string_new("");
		g_string_printf(buffer, "\"%s\"", id);
		signal_name = buffer->str;
		g_string_free(buffer, FALSE);
	}
	node = node_new (METHOD_NODE,
			 "scope", scope,
			 "method", method,
			 "mtype:steal", type,
			 "otype:steal", oid,
			 "gtktypes:steal", gtktypes,
			 "flags:steal", flags,
			 "id:steal", id,
			 "signal_name:steal", signal_name,
			 "args:steal", funcargs,
			 "funcattrs:steal", funcattrs,
			 "onerror:steal", onerror,
			 "defreturn:steal", defreturn,
			 "cbuf:steal", c_cbuf,
			 "line_no", line_no,
			 "ccode_line", ccode_line,
			 "vararg", vararg,
			 "unique_id", method_unique_id++,
			 NULL);

	last_added_method = (Method *)node;

	if(cbuf)
		g_string_free(cbuf,
			      /*only free segment if we haven't passed it
				above */
			      c_cbuf?FALSE:TRUE);
	gtktypes = NULL;
	signal_name = NULL;
	funcargs = NULL;
	funcattrs = NULL;
	onerror = NULL;
	defreturn = NULL;

	class_nodes = g_list_append(class_nodes, node);
}

static void
free_all_global_state(void)
{
	g_free(funcattrs);
	funcattrs = NULL;
	g_free(onerror);
	onerror = NULL;
	g_free(defreturn);
	defreturn = NULL;

	g_free(chunk_size);
	chunk_size = NULL;
	
	g_list_foreach(gtktypes, (GFunc)g_free, NULL);
	g_list_free(gtktypes);
	gtktypes = NULL;

	node_list_free (funcargs);
	funcargs = NULL;
}

static void
push_funcarg(char *name, char *postfix)
{
	Node *node;
	Type *type = pop_type();

	type->postfix = postfix;
	
	node = node_new (FUNCARG_NODE,
			 "atype:steal", type,
			 "name:steal", name,
			 "checks:steal", checks,
			 NULL);
	checks = NULL;
	
	funcargs = g_list_append(funcargs, node);
}

static void
push_init_arg(char *name, int is_class)
{
	Node *node;
	Node *type;
	char *tn;
	
	if(is_class)
		tn = g_strconcat(((Class *)class)->otype,":Class",NULL);
	else
		tn = g_strdup(((Class *)class)->otype);

	type = node_new (TYPE_NODE,
			 "name:steal", tn,
			 "pointer", "*",
			 NULL);
	node = node_new (FUNCARG_NODE,
			 "atype:steal", (Type *)type,
			 "name:steal", name,
			 NULL);
	funcargs = g_list_prepend(funcargs, node);
}

static void
push_self(char *id, gboolean constant)
{
	Node *node;
	Node *type;
	GList *ch = NULL;
	type = node_new (TYPE_NODE,
			 "name", ((Class *)class)->otype,
			 "pointer", constant ? "const *" : "*",
			 NULL);
	ch = g_list_append (ch, node_new (CHECK_NODE,
					  "chtype", NULL_CHECK,
					  NULL));
	ch = g_list_append (ch, node_new (CHECK_NODE,
					  "chtype", TYPE_CHECK,
					  NULL));
	node = node_new (FUNCARG_NODE,
			 "atype:steal", (Type *)type,
			 "name:steal", id,
			 "checks:steal", ch,
			 NULL);
	funcargs = g_list_prepend(funcargs, node);
}

static Variable *
find_var_or_die(const char *id, int line)
{
	GList *li;

	for(li = class_nodes; li != NULL; li = li->next) {
		Variable *var;
		Node *node = li->data;
		if(node->type != VARIABLE_NODE)
			continue;
		var = li->data;
		if(strcmp(var->id, id)==0)
			return var;
	}

	error_printf(GOB_ERROR, line, _("Variable %s not defined here"), id);

	g_assert_not_reached();
	return NULL;
}

static gboolean
set_attr_value(char *type, char *val)
{
	if(strcmp(type, "attr")==0) {
		if(!funcattrs) {
			funcattrs = val;
			return TRUE;
		} else
			return FALSE;
	} else if(strcmp(type, "onerror")==0) {
		if(!onerror) {
			onerror = val;
			return TRUE;
		} else
			return FALSE;
	} else if(strcmp(type, "defreturn")==0) {
		if(!defreturn) {
			defreturn = val;
			return TRUE;
		} else
			return FALSE;
	}
	return FALSE;
}

static void
export_accessors (const char *var_name,
		  gboolean do_get,
		  int get_lineno,
		  gboolean do_set,
		  int set_lineno,
		  Type *type,
		  const char *gtktype,
		  int lineno)
{	
	Type *the_type;

	if (type != NULL)
		the_type = (Type *)node_copy ((Node *)type);
	else
		the_type = get_tree_type (gtktype, TRUE);

	if (the_type == NULL) {
		error_print (GOB_ERROR, line_no,
			     _("Cannot determine type of property or argument"));
		return;
	}

	if (do_get) {
		char *get_id = g_strdup_printf ("get_%s", var_name);
		GString *get_cbuf = g_string_new (NULL);
		Node *node1 = node_new (TYPE_NODE,
					"name", the_type->name,
					"pointer", the_type->pointer,
					"postfix", the_type->postfix,
					NULL);
		Node *node3 = node_new (TYPE_NODE,
					"name", class->class.otype,
					"pointer", "*",
					NULL);

		g_string_sprintf (get_cbuf,
				  "\t%s%s val; "
				  "g_object_get (G_OBJECT (self), \"%s\", "
				  "&val, NULL); "
				  "return val;\n",
				  the_type->name, 
				  the_type->pointer ? the_type->pointer : "",
				  var_name);
		
		typestack = g_list_prepend (typestack, node1);
		typestack = g_list_prepend (typestack, node3);
		
		push_funcarg ("self", FALSE);
		
		push_function (PUBLIC_SCOPE, REGULAR_METHOD, NULL,
			       get_id, get_cbuf, get_lineno,
			       lineno, FALSE, NULL);
	}
	
	if (do_set) {
		char *set_id = g_strdup_printf ("set_%s", var_name);
		GString *set_cbuf = g_string_new (NULL);
		Node *node1 = node_new (TYPE_NODE, 
					"name", the_type->name,
					"pointer", the_type->pointer,
					"postfix", the_type->postfix,
					NULL);
		Node *node2 = node_new (TYPE_NODE, 
					"name", "void",
					NULL);
		Node *node3 = node_new (TYPE_NODE, 
					"name", class->class.otype,
					"pointer", "*",
					NULL);

		g_string_sprintf (set_cbuf,
				  "\tg_object_set (G_OBJECT (self), "
				  "\"%s\", val, NULL);\n",
				  var_name);

		typestack = g_list_prepend (typestack, node2);
		typestack = g_list_prepend (typestack, node1);
		typestack = g_list_prepend (typestack, node3);
		
		push_funcarg ("self", FALSE);
		push_funcarg ("val", FALSE);
	
		typestack = g_list_prepend (typestack, node2);
		push_function (PUBLIC_SCOPE, REGULAR_METHOD, NULL,
			       set_id, set_cbuf, set_lineno,
			       lineno, FALSE, NULL);
	}

	node_free ((Node *)the_type);
}

static char *
get_prop_enum_flag_cast (Property *prop)
{
	char *tmp, *ret;
	if (prop->extra_gtktype == NULL ||
	/* HACK!  just in case someone made this
	 * work with 2.0.0 by using the TYPE
	 * macro directly */
	    ((strstr (prop->extra_gtktype, "_TYPE_") != NULL ||
	      strstr (prop->extra_gtktype, "TYPE_") == prop->extra_gtktype) &&
	     strchr (prop->extra_gtktype, ':') == NULL)) {
		if (prop->ptype != NULL)
			return get_type (prop->ptype, TRUE);
		else
			return g_strdup ("");
	}
	tmp = remove_sep (prop->extra_gtktype);
	ret = g_strdup_printf ("(%s) ", tmp);
	g_free (tmp);
	return ret;
}

static void
add_construct_glade (char * file, char * root, char * domain)
{
	Node *var;
	Type * type;
	
	type = (Type *)node_new (TYPE_NODE,
				 "name", "GladeXML",
				 "pointer", "*",
				 NULL);
	initializer = g_strdup_printf("\t{\n"
				      "\tGtkWidget * root;\n"
				      "\t%%1$s->_priv->_glade_xml = glade_xml_new(%s, %s, %s);\n"
				      "\troot = glade_xml_get_widget(%%1$s->_priv->_glade_xml, %s);\n"
				      "\tgtk_widget_show(root);\n"
				      "\tgtk_container_add(GTK_CONTAINER(%%1$s), root);\n"
				      "\tglade_xml_signal_autoconnect_full(%%1$s->_priv->_glade_xml, (GladeXMLConnectFunc)___glade_xml_connect_foreach, (gpointer)%%1$s);\n"
				      "}\n", file, root, domain ? domain : "NULL", root);
	
	var = node_new (VARIABLE_NODE,
			"scope", PRIVATE_SCOPE,
			"vtype:steal", type,
			"glade_widget", FALSE,
			"id:steal", "_glade_xml",
			"destructor_unref", FALSE,
			"destructor", "g_object_unref", 
			"destructor_simple", TRUE,
			"initializer", initializer,
			"initializer_simple", FALSE,
			NULL);
	class_nodes = g_list_prepend(class_nodes, var);
}

static void
property_link_and_export (Node *node)
{
	Property *prop = (Property *)node;

	if (prop->link) {
		const char *root;
		char *get = NULL, *set = NULL;
		Variable *var;

		if (prop->set != NULL ||
		    prop->get != NULL) {	
			error_print (GOB_ERROR, prop->line_no,
				     _("Property linking requested, but "
				       "getters and setters exist"));
		}

		var = find_var_or_die (prop->name, prop->line_no);
		if(var->scope == PRIVATE_SCOPE) {
			root = "self->_priv";
		} else if (var->scope == CLASS_SCOPE) {
			root = "SELF_GET_CLASS(self)";
			if (no_self_alias)
				error_print (GOB_ERROR, prop->line_no,
					     _("Self aliases needed when autolinking to a classwide member"));
		} else {
			root = "self";
		}

		if (strcmp (prop->gtktype, "STRING") == 0) {
			set = g_strdup_printf("{ char *old = %s->%s; "
					      "%s->%s = g_value_dup_string (VAL); g_free (old); }",
					      root, prop->name,
					      root, prop->name);
			get = g_strdup_printf("g_value_set_string (VAL, %s->%s);",
					      root, prop->name);
		} else if (strcmp (prop->gtktype, "OBJECT") == 0) {
			char *cast;
			if (prop->extra_gtktype != NULL) {
				cast = remove_sep (prop->extra_gtktype);
			} else {
				cast = g_strdup ("void");
			}
			set = g_strdup_printf("{ GObject *___old = (GObject *)%s->%s; "
					      "%s->%s = (%s *)g_value_dup_object (VAL); "
					      "if (___old != NULL) { "
					        "g_object_unref (G_OBJECT (___old)); "
					      "} "
					      "}",
					      root, prop->name,
					      root, prop->name,
					      cast);
			get = g_strdup_printf ("g_value_set_object (VAL, "
					       "(gpointer)%s->%s);",
					       root, prop->name);
			g_free (cast);
		} else if (strcmp (prop->gtktype, "BOXED") == 0) {
			char *type = make_me_type (prop->extra_gtktype,
						   "G_TYPE_BOXED");
			if (prop->extra_gtktype == NULL) {
				error_print (GOB_ERROR, prop->line_no,
					     _("Property linking requested for BOXED, but "
					       "boxed_type not set"));
			}
			set = g_strdup_printf("{ gpointer ___old = (gpointer)%s->%s; "
					      "gpointer ___new = (gpointer)g_value_get_boxed (VAL); "
					      "if (___new != ___old) { "
					        "if (___old != NULL) g_boxed_free (%s, ___old); "
					        "if (___new != NULL) %s->%s = g_boxed_copy (%s, ___new); "
					        "else %s->%s = NULL;"
					      "} "
					      "}",
					      root, prop->name,
					      type,
					      root, prop->name,
					      type,
					      root, prop->name);
			get = g_strdup_printf("g_value_set_boxed (VAL, %s->%s);",
					      root, prop->name);
			g_free (type);
		} else {
			char *set_func;
			char *get_func;
			const char *getcast = "";
			const char *setcast = "";
			char *to_free = NULL;
			set_func = g_strdup_printf ("g_value_set_%s", prop->gtktype);
			g_strdown (set_func);
			get_func = g_strdup_printf ("g_value_get_%s", prop->gtktype);
			g_strdown (get_func);

			if (for_cpp) {
				if (strcmp (prop->gtktype, "FLAGS") == 0) {
					setcast = "(guint) ";
					getcast = to_free =
						get_prop_enum_flag_cast (prop);
				} else if (strcmp (prop->gtktype, "ENUM") == 0) {
					setcast = "(gint) ";
					getcast = to_free =
						get_prop_enum_flag_cast (prop);
                               }  else if (strcmp (prop->gtktype, "POINTER") == 0) {
                                       setcast = "(gpointer) ";
                                       getcast = g_strdup_printf ("(%s%s) ",
								  prop->ptype->name,
								  prop->ptype->pointer ? prop->ptype->pointer : "");
				}
			}

			set = g_strdup_printf("%s->%s = %s%s (VAL);",
					      root, prop->name,
					      getcast,
					      get_func);
			get = g_strdup_printf("%s (VAL, %s%s->%s);",
					      set_func,
					      setcast,	
					      root, prop->name);

			g_free (get_func);
			g_free (set_func);
			g_free (to_free);
		}

		node_set (node,
			  "get:steal", get,
			  "get_line", prop->line_no,
			  "set:steal", set,
			  "set_line", prop->line_no,
			  NULL);
	}

	if (prop->export) {
		export_accessors (prop->name,
				  prop->get != NULL, prop->get_line,
				  prop->set != NULL,  prop->set_line,
				  prop->ptype,
				  prop->gtktype,
				  prop->line_no);
	} 
}


static char *
debool (char *s)
{
	if (strcmp (s, "BOOL") == 0) {
		error_print (GOB_WARN, line_no,
			    _("BOOL type is deprecated, please use BOOLEAN"));
		g_free (s);
		return g_strdup ("BOOLEAN");
	} else {
		return s;
	}
}

static void
ensure_property (void)
{
	if (property == NULL)
		property = (Property *)node_new (PROPERTY_NODE, NULL);
}


/* Line 371 of yacc.c  */
#line 737 "parse.c"

# ifndef YY_NULL
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULL nullptr
#  else
#   define YY_NULL 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "y.tab.h".  */
#ifndef YY_YY_PARSE_H_INCLUDED
# define YY_YY_PARSE_H_INCLUDED
/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     CLASS = 258,
     FROM = 259,
     CONST = 260,
     VOID = 261,
     STRUCT = 262,
     UNION = 263,
     ENUM = 264,
     THREEDOTS = 265,
     SIGNED = 266,
     UNSIGNED = 267,
     LONG = 268,
     SHORT = 269,
     INT = 270,
     FLOAT = 271,
     DOUBLE = 272,
     CHAR = 273,
     TOKEN = 274,
     NUMBER = 275,
     TYPETOKEN = 276,
     ARRAY_DIM = 277,
     SINGLE_CHAR = 278,
     CCODE = 279,
     CTCODE = 280,
     ADCODE = 281,
     HTCODE = 282,
     PHCODE = 283,
     HCODE = 284,
     ACODE = 285,
     ATCODE = 286,
     STRING = 287,
     PUBLIC = 288,
     PRIVATE = 289,
     PROTECTED = 290,
     CLASSWIDE = 291,
     PROPERTY = 292,
     ARGUMENT = 293,
     VIRTUAL = 294,
     SIGNAL = 295,
     OVERRIDE = 296,
     NAME = 297,
     NICK = 298,
     BLURB = 299,
     MAXIMUM = 300,
     MINIMUM = 301,
     DEFAULT_VALUE = 302,
     ERROR = 303,
     FLAGS = 304,
     TYPE = 305,
     FLAGS_TYPE = 306,
     ENUM_TYPE = 307,
     PARAM_TYPE = 308,
     BOXED_TYPE = 309,
     OBJECT_TYPE = 310
   };
#endif
/* Tokens.  */
#define CLASS 258
#define FROM 259
#define CONST 260
#define VOID 261
#define STRUCT 262
#define UNION 263
#define ENUM 264
#define THREEDOTS 265
#define SIGNED 266
#define UNSIGNED 267
#define LONG 268
#define SHORT 269
#define INT 270
#define FLOAT 271
#define DOUBLE 272
#define CHAR 273
#define TOKEN 274
#define NUMBER 275
#define TYPETOKEN 276
#define ARRAY_DIM 277
#define SINGLE_CHAR 278
#define CCODE 279
#define CTCODE 280
#define ADCODE 281
#define HTCODE 282
#define PHCODE 283
#define HCODE 284
#define ACODE 285
#define ATCODE 286
#define STRING 287
#define PUBLIC 288
#define PRIVATE 289
#define PROTECTED 290
#define CLASSWIDE 291
#define PROPERTY 292
#define ARGUMENT 293
#define VIRTUAL 294
#define SIGNAL 295
#define OVERRIDE 296
#define NAME 297
#define NICK 298
#define BLURB 299
#define MAXIMUM 300
#define MINIMUM 301
#define DEFAULT_VALUE 302
#define ERROR 303
#define FLAGS 304
#define TYPE 305
#define FLAGS_TYPE 306
#define ENUM_TYPE 307
#define PARAM_TYPE 308
#define BOXED_TYPE 309
#define OBJECT_TYPE 310



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{
/* Line 387 of yacc.c  */
#line 692 "parse.y"

	char *id;
	GString *cbuf;
	GList *list;
	int line;
	int sigtype;


/* Line 387 of yacc.c  */
#line 899 "parse.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;

#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */

#endif /* !YY_YY_PARSE_H_INCLUDED  */

/* Copy the second part of user declarations.  */

/* Line 390 of yacc.c  */
#line 927 "parse.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(N) (N)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (YYID (0))
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  25
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   612

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  69
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  53
/* YYNRULES -- Number of rules.  */
#define YYNRULES  200
/* YYNRULES -- Number of states.  */
#define YYNSTATES  425

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   310

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    67,     2,     2,     2,     2,     2,     2,
      58,    59,    64,     2,    62,    68,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    60,
      66,    61,    65,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    56,    63,    57,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     7,    10,    13,    15,    17,    19,    21,
      23,    25,    27,    29,    31,    34,    37,    40,    43,    45,
      47,    49,    51,    56,    60,    66,    67,    72,    78,    84,
      90,    97,   105,   112,   120,   123,   125,   127,   130,   134,
     136,   138,   140,   142,   144,   146,   148,   150,   153,   157,
     160,   164,   167,   170,   172,   174,   176,   177,   183,   190,
     203,   213,   220,   224,   225,   237,   246,   252,   256,   257,
     261,   263,   265,   270,   272,   274,   278,   282,   286,   290,
     294,   298,   302,   306,   310,   314,   318,   322,   326,   330,
     334,   338,   342,   346,   348,   354,   356,   360,   361,   365,
     367,   370,   372,   374,   376,   379,   382,   385,   389,   393,
     396,   399,   402,   404,   407,   409,   412,   414,   416,   418,
     420,   422,   424,   426,   428,   430,   432,   434,   436,   438,
     441,   444,   448,   451,   453,   457,   461,   464,   466,   471,
     477,   481,   483,   486,   488,   499,   511,   521,   531,   540,
     552,   561,   567,   570,   575,   582,   583,   585,   588,   590,
     592,   595,   598,   602,   607,   612,   614,   618,   620,   624,
     626,   629,   633,   640,   648,   651,   653,   655,   658,   661,
     665,   669,   673,   677,   685,   694,   698,   700,   704,   706,
     714,   723,   727,   729,   737,   746,   750,   752,   754,   757,
     759
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      70,     0,    -1,    72,    73,    72,    -1,    73,    72,    -1,
      72,    73,    -1,    73,    -1,    24,    -1,    26,    -1,    29,
      -1,    27,    -1,    28,    -1,    30,    -1,    31,    -1,    25,
      -1,    72,    71,    -1,    72,   114,    -1,    72,   117,    -1,
      72,   119,    -1,    71,    -1,   114,    -1,   117,    -1,   119,
      -1,    74,    56,    76,    57,    -1,    74,    56,    57,    -1,
       3,    21,     4,    21,    75,    -1,    -1,    58,    19,    59,
      75,    -1,    58,    19,    19,    59,    75,    -1,    58,    19,
      21,    59,    75,    -1,    58,    19,    20,    59,    75,    -1,
      58,    19,    32,    32,    59,    75,    -1,    58,    19,    32,
      32,    32,    59,    75,    -1,    58,    19,    19,    32,    59,
      75,    -1,    58,    19,    19,    32,    32,    59,    75,    -1,
      76,    77,    -1,    77,    -1,   105,    -1,    19,   105,    -1,
      19,    21,   105,    -1,    82,    -1,    83,    -1,    85,    -1,
      60,    -1,    33,    -1,    34,    -1,    35,    -1,    36,    -1,
      19,    19,    -1,    19,    56,    24,    -1,    61,   121,    -1,
      61,    56,    24,    -1,    79,    80,    -1,    80,    79,    -1,
      80,    -1,    79,    -1,    19,    -1,    -1,    78,    94,    19,
      81,    60,    -1,    78,    94,    19,    22,    81,    60,    -1,
      38,    92,    91,    19,    84,    19,    56,    24,    19,    56,
      24,    60,    -1,    38,    92,    91,    19,    84,    19,    56,
      24,    60,    -1,    38,    92,    91,    19,    84,    19,    -1,
      58,    19,    59,    -1,    -1,    37,    19,    19,    86,    19,
      56,    24,    19,    56,    24,    60,    -1,    37,    19,    19,
      86,    19,    56,    24,    60,    -1,    37,    19,    19,    86,
      60,    -1,    58,    87,    59,    -1,    -1,    87,    62,    90,
      -1,    90,    -1,    32,    -1,    19,    58,    32,    59,    -1,
     121,    -1,    88,    -1,    42,    61,    88,    -1,    43,    61,
      88,    -1,    44,    61,    88,    -1,    45,    61,   121,    -1,
      46,    61,   121,    -1,    47,    61,    89,    -1,    49,    61,
      93,    -1,    50,    61,    94,    -1,    51,    61,    21,    -1,
      51,    61,    19,    -1,    52,    61,    21,    -1,    52,    61,
      19,    -1,    53,    61,    21,    -1,    53,    61,    19,    -1,
      54,    61,    21,    -1,    54,    61,    19,    -1,    55,    61,
      21,    -1,    55,    61,    19,    -1,    19,    -1,    19,    58,
      19,    94,    59,    -1,    19,    -1,    58,    93,    59,    -1,
      -1,    19,    63,    93,    -1,    19,    -1,    95,    99,    -1,
      95,    -1,    96,    -1,    19,    -1,     5,    19,    -1,    19,
       5,    -1,    98,    19,    -1,     5,    98,    19,    -1,    98,
      19,     5,    -1,    97,    96,    -1,    21,    96,    -1,     5,
      96,    -1,    21,    -1,    21,     5,    -1,    97,    -1,    97,
       5,    -1,     6,    -1,    18,    -1,    14,    -1,    15,    -1,
      13,    -1,    16,    -1,    17,    -1,    11,    -1,    12,    -1,
       7,    -1,     8,    -1,     9,    -1,    64,    -1,    64,     5,
      -1,    64,    99,    -1,    64,     5,    99,    -1,    19,   102,
      -1,   102,    -1,    78,    19,   102,    -1,    19,    78,   102,
      -1,    78,   102,    -1,   100,    -1,    19,    58,   103,    59,
      -1,    19,    32,    58,   103,    59,    -1,   103,    62,    19,
      -1,    19,    -1,    56,    24,    -1,    60,    -1,    40,    92,
     101,    94,    19,    58,   108,    59,   106,   104,    -1,    78,
      40,    92,   100,    94,    19,    58,   108,    59,   106,   104,
      -1,    39,    78,    94,    19,    58,   108,    59,   106,   104,
      -1,    78,    39,    94,    19,    58,   108,    59,   106,   104,
      -1,    39,    94,    19,    58,   108,    59,   106,   104,    -1,
      41,    58,    21,    59,    94,    19,    58,   108,    59,   106,
     104,    -1,    78,    94,    19,    58,   108,    59,   106,   104,
      -1,    19,    58,    19,    59,   104,    -1,    19,   107,    -1,
      19,   107,    19,   107,    -1,    19,   107,    19,   107,    19,
     107,    -1,    -1,   121,    -1,    56,    24,    -1,     6,    -1,
      19,    -1,    19,     5,    -1,     5,    19,    -1,    19,    62,
     109,    -1,    19,     5,    62,   109,    -1,     5,    19,    62,
     109,    -1,   109,    -1,   110,    62,    10,    -1,   110,    -1,
     110,    62,   111,    -1,   111,    -1,    94,    19,    -1,    94,
      19,    22,    -1,    94,    19,    58,    19,   112,    59,    -1,
      94,    19,    22,    58,    19,   112,    59,    -1,   112,   113,
      -1,   113,    -1,    19,    -1,    65,   121,    -1,    66,   121,
      -1,    65,    61,   121,    -1,    66,    61,   121,    -1,    61,
      61,   121,    -1,    67,    61,   121,    -1,     9,    19,    56,
     115,    57,    21,    60,    -1,     9,    19,    56,   115,    62,
      57,    21,    60,    -1,   115,    62,   116,    -1,   116,    -1,
      19,    61,   121,    -1,    19,    -1,    49,    19,    56,   118,
      57,    21,    60,    -1,    49,    19,    56,   118,    62,    57,
      21,    60,    -1,   118,    62,    19,    -1,    19,    -1,    48,
      19,    56,   120,    57,    21,    60,    -1,    48,    19,    56,
     120,    62,    57,    21,    60,    -1,   120,    62,    19,    -1,
      19,    -1,    20,    -1,    68,    20,    -1,    23,    -1,    19,
      -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   713,   713,   714,   715,   716,   719,   728,   737,   746,
     755,   764,   773,   782,   793,   794,   795,   796,   797,   798,
     799,   800,   803,   808,   815,   832,   833,   841,   853,   862,
     874,   883,   892,   901,   912,   913,   916,   917,   926,   938,
     939,   940,   941,   944,   945,   946,   947,   950,   970,   994,
     998,  1006,  1007,  1008,  1009,  1010,  1018,  1024,  1027,  1032,
    1100,  1154,  1243,  1251,  1256,  1304,  1340,  1356,  1357,  1360,
    1361,  1364,  1365,  1377,  1378,  1381,  1387,  1393,  1399,  1405,
    1411,  1417,  1423,  1430,  1436,  1442,  1448,  1454,  1460,  1466,
    1472,  1478,  1484,  1490,  1515,  1524,  1530,  1531,  1534,  1537,
    1543,  1550,  1559,  1562,  1565,  1569,  1573,  1577,  1582,  1590,
    1594,  1599,  1603,  1606,  1610,  1613,  1618,  1619,  1620,  1621,
    1622,  1623,  1624,  1625,  1626,  1629,  1630,  1631,  1634,  1635,
    1636,  1640,  1647,  1659,  1665,  1677,  1689,  1692,  1698,  1701,
    1707,  1710,  1715,  1716,  1720,  1743,  1766,  1789,  1812,  1830,
    1843,  1853,  1893,  1905,  1925,  1956,  1963,  1964,  1970,  1971,
    1982,  1993,  2004,  2014,  2024,  2034,  2037,  2038,  2041,  2042,
    2045,  2048,  2051,  2059,  2069,  2070,  2073,  2090,  2097,  2104,
    2111,  2118,  2125,  2134,  2143,  2154,  2155,  2158,  2178,  2188,
    2197,  2208,  2211,  2216,  2225,  2236,  2239,  2245,  2246,  2250,
    2251
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "CLASS", "FROM", "CONST", "VOID",
  "STRUCT", "UNION", "ENUM", "THREEDOTS", "SIGNED", "UNSIGNED", "LONG",
  "SHORT", "INT", "FLOAT", "DOUBLE", "CHAR", "TOKEN", "NUMBER",
  "TYPETOKEN", "ARRAY_DIM", "SINGLE_CHAR", "CCODE", "CTCODE", "ADCODE",
  "HTCODE", "PHCODE", "HCODE", "ACODE", "ATCODE", "STRING", "PUBLIC",
  "PRIVATE", "PROTECTED", "CLASSWIDE", "PROPERTY", "ARGUMENT", "VIRTUAL",
  "SIGNAL", "OVERRIDE", "NAME", "NICK", "BLURB", "MAXIMUM", "MINIMUM",
  "DEFAULT_VALUE", "ERROR", "FLAGS", "TYPE", "FLAGS_TYPE", "ENUM_TYPE",
  "PARAM_TYPE", "BOXED_TYPE", "OBJECT_TYPE", "'{'", "'}'", "'('", "')'",
  "';'", "'='", "','", "'|'", "'*'", "'>'", "'<'", "'!'", "'-'", "$accept",
  "prog", "ccode", "ccodes", "class", "classdec", "classflags",
  "classcode", "thing", "scope", "destructor", "initializer", "varoptions",
  "variable", "argument", "export", "property", "param_spec",
  "param_spec_list", "string", "anyval", "param_spec_value", "argtype",
  "flags", "flaglist", "type", "specifier_list", "spec_list", "specifier",
  "strunionenum", "pointer", "simplesigtype", "fullsigtype", "sigtype",
  "tokenlist", "codenocode", "method", "methodmods", "retcode", "funcargs",
  "arglist", "arglist1", "arg", "checklist", "check", "enumcode",
  "enumvals", "enumval", "flagcode", "flagvals", "errorcode", "errorvals",
  "numtok", YY_NULL
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   123,   125,    40,    41,
      59,    61,    44,   124,    42,    62,    60,    33,    45
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    69,    70,    70,    70,    70,    71,    71,    71,    71,
      71,    71,    71,    71,    72,    72,    72,    72,    72,    72,
      72,    72,    73,    73,    74,    75,    75,    75,    75,    75,
      75,    75,    75,    75,    76,    76,    77,    77,    77,    77,
      77,    77,    77,    78,    78,    78,    78,    79,    79,    80,
      80,    81,    81,    81,    81,    81,    81,    82,    82,    83,
      83,    83,    84,    84,    85,    85,    85,    86,    86,    87,
      87,    88,    88,    89,    89,    90,    90,    90,    90,    90,
      90,    90,    90,    90,    90,    90,    90,    90,    90,    90,
      90,    90,    90,    90,    91,    91,    92,    92,    93,    93,
      94,    94,    95,    95,    95,    95,    95,    95,    95,    96,
      96,    96,    96,    96,    96,    96,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    98,    98,    98,    99,    99,
      99,    99,   100,   100,   101,   101,   101,   101,   102,   102,
     103,   103,   104,   104,   105,   105,   105,   105,   105,   105,
     105,   105,   106,   106,   106,   106,   107,   107,   108,   108,
     108,   108,   108,   108,   108,   108,   109,   109,   110,   110,
     111,   111,   111,   111,   112,   112,   113,   113,   113,   113,
     113,   113,   113,   114,   114,   115,   115,   116,   116,   117,
     117,   118,   118,   119,   119,   120,   120,   121,   121,   121,
     121
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     3,     2,     2,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     2,     2,     2,     2,     1,     1,
       1,     1,     4,     3,     5,     0,     4,     5,     5,     5,
       6,     7,     6,     7,     2,     1,     1,     2,     3,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     3,     2,
       3,     2,     2,     1,     1,     1,     0,     5,     6,    12,
       9,     6,     3,     0,    11,     8,     5,     3,     0,     3,
       1,     1,     4,     1,     1,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     1,     5,     1,     3,     0,     3,     1,
       2,     1,     1,     1,     2,     2,     2,     3,     3,     2,
       2,     2,     1,     2,     1,     2,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     3,     2,     1,     3,     3,     2,     1,     4,     5,
       3,     1,     2,     1,    10,    11,     9,     9,     8,    11,
       8,     5,     2,     4,     6,     0,     1,     2,     1,     1,
       2,     2,     3,     4,     4,     1,     3,     1,     3,     1,
       2,     3,     6,     7,     2,     1,     1,     2,     2,     3,
       3,     3,     3,     7,     8,     3,     1,     3,     1,     7,
       8,     3,     1,     7,     8,     3,     1,     1,     2,     1,
       1
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,     0,     6,    13,     7,     9,    10,     8,    11,
      12,     0,     0,     0,    18,     0,     5,     0,    19,    20,
      21,     0,     0,     0,     0,     1,    14,     4,    15,    16,
      17,     3,     0,     0,     0,     0,     0,     2,     0,    43,
      44,    45,    46,     0,    97,     0,    97,     0,    23,    42,
       0,    35,     0,    39,    40,    41,    36,    25,   188,     0,
     186,   196,     0,   192,     0,     0,     0,     0,     0,    37,
       0,     0,     0,     0,   116,   125,   126,   127,   123,   124,
     120,   118,   119,   121,   122,   117,   103,   112,     0,     0,
     101,   102,   114,     0,     0,     0,    22,    34,     0,    97,
       0,     0,    24,     0,     0,     0,     0,     0,     0,     0,
      38,     0,     0,    68,    99,     0,    95,     0,     0,   104,
     111,     0,   105,   113,   110,     0,     0,   128,   100,   115,
     109,   106,     0,     0,   137,     0,   133,     0,     0,     0,
      56,     0,   200,   197,   199,     0,   187,     0,     0,   185,
       0,   195,     0,     0,   191,     0,     0,     0,     0,     0,
       0,    96,     0,    63,   107,     0,     0,   129,   130,   108,
       0,     0,     0,     0,   132,     0,   136,     0,     0,     0,
       0,     0,    55,    56,     0,     0,    54,    53,     0,     0,
       0,     0,     0,    25,   198,   183,     0,   193,     0,   189,
       0,     0,   143,   151,    93,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    70,
       0,    66,    98,     0,     0,     0,     0,     0,   116,   103,
       0,     0,   165,   167,   169,   131,     0,   141,     0,   135,
     134,     0,     0,     0,     0,    47,     0,     0,     0,     0,
      49,    51,     0,    52,    57,     0,    25,    25,    25,     0,
      26,   184,   194,   190,   142,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    67,     0,
       0,     0,     0,    61,     0,   104,   105,     0,   170,   155,
       0,     0,   138,     0,     0,     0,     0,     0,    48,    58,
     155,    50,     0,    25,    27,    29,    28,     0,    25,     0,
      71,    75,    76,    77,    78,    79,   200,    74,    80,    73,
      81,    82,    84,    83,    86,    85,    88,    87,    90,    89,
      92,    91,    69,     0,    94,    62,     0,   155,     0,     0,
     162,   171,     0,     0,     0,   166,   168,   139,   140,     0,
       0,   155,     0,     0,    25,    32,    25,    30,     0,     0,
      65,     0,     0,   164,   163,     0,     0,     0,   152,   156,
     148,   155,     0,     0,     0,   150,    33,    31,     0,     0,
       0,    60,   146,     0,   176,     0,     0,     0,     0,     0,
     175,   157,     0,     0,   155,   147,   155,    72,     0,     0,
       0,     0,     0,   177,     0,   178,     0,   172,   174,   153,
     144,     0,     0,    64,     0,   173,   181,   179,   180,   182,
       0,   149,   145,    59,   154
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    13,    14,    15,    16,    17,   102,    50,    51,    52,
     186,   187,   188,    53,    54,   225,    55,   159,   218,   311,
     318,   219,   117,    72,   115,   230,    90,    91,    92,    93,
     128,   134,   135,   136,   238,   203,    56,   344,   368,   231,
     232,   233,   234,   389,   390,    18,    59,    60,    19,    64,
      20,    62,   369
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -334
static const yytype_int16 yypact[] =
{
     169,    10,    22,  -334,  -334,  -334,  -334,  -334,  -334,  -334,
    -334,    82,    99,   133,  -334,   169,   220,    96,  -334,  -334,
    -334,   181,   134,   135,   145,  -334,  -334,   220,  -334,  -334,
    -334,   220,   170,   205,   183,   212,   213,   220,   237,  -334,
    -334,  -334,  -334,   223,   196,   341,   196,   203,  -334,  -334,
     200,  -334,   324,  -334,  -334,  -334,  -334,   204,   192,   -30,
    -334,  -334,   102,  -334,   125,   208,   332,   256,   324,  -334,
     262,   263,   278,   390,  -334,  -334,  -334,  -334,  -334,  -334,
    -334,  -334,  -334,  -334,  -334,  -334,   300,   458,   407,   292,
     257,  -334,   472,   315,   274,   246,  -334,  -334,   407,   196,
     325,   342,  -334,    76,   348,    68,   349,    69,   372,    73,
    -334,   351,   381,   359,   364,   375,   386,   432,   486,  -334,
    -334,   442,  -334,   486,  -334,   446,   408,     4,  -334,   486,
    -334,   462,   255,   449,  -334,   407,  -334,   421,   463,   475,
       3,   154,  -334,  -334,  -334,   461,  -334,   435,   484,  -334,
     436,  -334,   485,   448,  -334,   488,   128,   452,   273,     7,
     263,  -334,   492,   454,  -334,   455,   424,   257,  -334,  -334,
      -2,   456,   496,   497,  -334,    27,  -334,   498,   407,   460,
      27,   407,    92,    -6,   424,    -5,   459,   500,   464,    65,
     466,   467,   489,   204,  -334,  -334,   468,  -334,   469,  -334,
     470,   499,  -334,  -334,  -334,   471,   473,   474,   476,   477,
     478,   479,   480,   481,   482,   483,   487,   490,   153,  -334,
     491,  -334,  -334,   407,   503,   508,   424,   441,   493,    14,
     512,   494,  -334,   495,  -334,  -334,   496,  -334,   161,  -334,
    -334,   501,   514,   424,   517,  -334,   521,   502,   504,   522,
    -334,  -334,    92,  -334,  -334,    80,   204,   204,   204,   121,
    -334,  -334,  -334,  -334,  -334,   144,   144,   144,    76,    76,
      16,   263,   407,   195,   275,   281,   285,   293,  -334,   273,
     525,   505,   506,   510,   509,   162,   163,   407,    88,   531,
     373,   193,  -334,   535,   424,   511,   513,   515,  -334,  -334,
     531,  -334,   516,   204,  -334,  -334,  -334,   518,   204,   520,
    -334,  -334,  -334,  -334,  -334,  -334,   520,  -334,  -334,  -334,
    -334,  -334,  -334,  -334,  -334,  -334,  -334,  -334,  -334,  -334,
    -334,  -334,  -334,     9,  -334,  -334,   532,   531,   407,   407,
    -334,   523,   536,    30,   128,  -334,  -334,  -334,  -334,   524,
     424,   531,   424,   128,   204,  -334,   204,  -334,   526,   528,
    -334,    23,   128,  -334,  -334,   541,    75,   537,   548,  -334,
    -334,   531,   527,   128,   529,  -334,  -334,  -334,   530,   546,
     534,  -334,  -334,    75,  -334,   519,    70,    86,   533,    62,
    -334,  -334,    30,   128,   531,  -334,   531,  -334,   538,   547,
      84,    76,    76,  -334,    76,  -334,    76,  -334,  -334,   555,
    -334,   128,   128,  -334,   539,  -334,  -334,  -334,  -334,  -334,
      30,  -334,  -334,  -334,  -334
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -334,  -334,   146,   165,   561,  -334,  -186,  -334,   542,   -28,
     392,   396,   402,  -334,  -334,  -334,  -334,  -334,  -334,  -233,
    -334,   308,  -334,   -34,  -155,   -44,  -334,   -71,  -334,   -69,
     -47,   457,  -334,   -18,   355,  -333,    94,  -294,  -285,  -181,
    -264,  -334,   303,   214,  -281,   228,  -334,   507,   248,  -334,
     249,  -334,  -103
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -162
static const yytype_int16 yytable[] =
{
     146,    89,   120,   248,   121,   222,   353,   260,   100,   167,
      68,   370,    94,   182,   142,   143,   124,    88,   144,   286,
     375,   130,   182,   340,   112,   183,   220,   104,   359,   382,
     171,    21,   105,   312,   313,   316,   143,   317,    68,   144,
     395,    22,   380,   362,   125,   284,   170,   120,   310,   142,
     143,   249,   120,   144,   138,   185,   172,   373,   120,   171,
     410,   184,   296,   145,   185,   139,   133,   221,   127,   360,
     304,   305,   306,  -159,   363,   364,   287,   393,   421,   422,
     168,   384,   250,   381,   145,   172,   367,    58,   151,   142,
     143,   177,   154,   144,   384,   142,   143,   255,   145,   144,
     411,    23,   412,   384,   173,   142,   143,   409,   408,   144,
     341,   245,   302,   349,   174,   176,   320,   355,    24,   408,
     235,   407,   357,   385,   256,   148,   152,   386,   387,   388,
     155,   402,    69,    25,   242,   424,   385,   244,   145,   303,
     386,   387,   388,   415,   145,   385,   342,   404,   246,   386,
     387,   388,    32,   307,   145,   239,   120,   240,   121,   106,
     110,    26,   174,   309,   107,   314,   315,   319,   376,   372,
     377,   374,     1,   189,   190,   191,   310,    26,     2,   281,
     308,    31,   108,    26,   201,    33,   192,   109,   202,    38,
      34,    35,    37,     3,     4,     5,     6,     7,     8,     9,
      10,    36,    58,    39,    40,    41,    42,    43,    44,    45,
      46,    47,   278,   193,   322,   279,   323,    11,    12,    38,
     292,  -161,  -160,   293,   338,   339,    57,    48,   321,     2,
      49,    61,    63,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    70,    28,     3,     4,     5,     6,     7,     8,
       9,    10,   347,   103,    71,   293,    65,    96,    66,    28,
      49,    95,   101,    29,    30,    28,    67,   137,    11,    12,
      39,    40,    41,    42,   170,   111,    45,    46,    47,    29,
      30,   113,   114,   403,   405,    29,    30,   171,    39,    40,
      41,    42,   204,   132,   324,    67,   325,   116,   416,   417,
     326,   418,   327,   419,   328,   122,   329,    39,    40,    41,
      42,   126,   330,   172,   331,   205,   206,   207,   208,   209,
     210,   127,   211,   212,   213,   214,   215,   216,   217,    73,
      74,    75,    76,    77,   131,    78,    79,    80,    81,    82,
      83,    84,    85,    86,   140,    87,    73,    74,    75,    76,
      77,    65,    78,    79,    80,    81,    82,    83,    84,    85,
      86,   141,    87,    98,    99,    39,    40,    41,    42,   147,
     150,    45,    46,    47,    39,    40,    41,    42,    73,    74,
      75,    76,    77,   345,    78,    79,    80,    81,    82,    83,
      84,    85,    86,   153,    87,   118,    74,    75,    76,    77,
     157,    78,    79,    80,    81,    82,    83,    84,    85,   119,
     156,    87,    73,    74,    75,    76,    77,   158,    78,    79,
      80,    81,    82,    83,    84,    85,    86,   160,    87,   227,
     228,    75,    76,    77,   161,    78,    79,    80,    81,    82,
      83,    84,    85,   229,   162,    87,   118,    74,    75,    76,
      77,   163,    78,    79,    80,    81,    82,    83,    84,    85,
     285,   164,    87,   123,    74,   165,   166,   169,   175,    78,
      79,    80,    81,    82,    83,    84,    85,   129,    74,    87,
     178,   194,   179,    78,    79,    80,    81,    82,    83,    84,
      85,   118,    74,    87,   180,   195,   197,    78,    79,    80,
      81,    82,    83,    84,    85,   196,   198,    87,   199,   200,
     184,   223,   224,   226,   236,   237,   170,   241,   243,   252,
     185,   259,   282,   264,   254,   257,   258,   283,   261,   262,
     263,   288,   265,   295,   266,   267,   297,   268,   269,   270,
     271,   272,   273,   274,   275,   298,   301,   280,   276,   333,
     343,   277,  -158,   289,   348,   366,   361,   290,   378,   294,
     383,   391,   299,   300,   334,   335,   336,   392,   337,   350,
     398,   414,   351,   352,   420,   354,    27,   356,   358,   253,
     401,   365,   251,   371,   379,   247,   394,   332,   396,   397,
     399,   291,    97,   346,   406,     0,   181,   400,   413,   423,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   149
};

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-334)))

#define yytable_value_is_error(Yytable_value) \
  YYID (0)

static const yytype_int16 yycheck[] =
{
     103,    45,    73,   184,    73,   160,   300,   193,    52,     5,
      38,   344,    46,    19,    19,    20,    87,    45,    23,     5,
     353,    92,    19,   287,    68,    22,    19,    57,    19,   362,
      32,    21,    62,   266,   267,    19,    20,   270,    66,    23,
     373,    19,    19,   337,    88,   226,    19,   118,    32,    19,
      20,    56,   123,    23,    98,    61,    58,   351,   129,    32,
     393,    58,   243,    68,    61,    99,    94,    60,    64,    60,
     256,   257,   258,    59,   338,   339,    62,   371,   411,   412,
     127,    19,   185,    60,    68,    58,    56,    19,    19,    19,
      20,   135,    19,    23,    19,    19,    20,    32,    68,    23,
     394,    19,   396,    19,   132,    19,    20,   392,   389,    23,
      22,    19,    32,   294,   132,   133,   271,   303,    19,   400,
     167,    59,   308,    61,    59,    57,    57,    65,    66,    67,
      57,    61,    38,     0,   178,   420,    61,   181,    68,    59,
      65,    66,    67,    59,    68,    61,    58,    61,    56,    65,
      66,    67,    56,    32,    68,   173,   227,   175,   227,    57,
      66,    15,   180,    19,    62,   268,   269,   270,   354,   350,
     356,   352,     3,    19,    20,    21,    32,    31,     9,   223,
      59,    16,    57,    37,    56,     4,    32,    62,    60,    19,
      56,    56,    27,    24,    25,    26,    27,    28,    29,    30,
      31,    56,    19,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    59,    59,    19,    62,    21,    48,    49,    19,
      59,    59,    59,    62,    62,    62,    21,    57,   272,     9,
      60,    19,    19,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    19,    15,    24,    25,    26,    27,    28,    29,
      30,    31,    59,    61,    58,    62,    19,    57,    21,    31,
      60,    58,    58,    15,    15,    37,    58,    21,    48,    49,
      33,    34,    35,    36,    19,    19,    39,    40,    41,    31,
      31,    19,    19,   386,   387,    37,    37,    32,    33,    34,
      35,    36,    19,    19,    19,    58,    21,    19,   401,   402,
      19,   404,    21,   406,    19,     5,    21,    33,    34,    35,
      36,    19,    19,    58,    21,    42,    43,    44,    45,    46,
      47,    64,    49,    50,    51,    52,    53,    54,    55,     5,
       6,     7,     8,     9,    19,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    19,    21,     5,     6,     7,     8,
       9,    19,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    19,    21,    39,    40,    33,    34,    35,    36,    21,
      21,    39,    40,    41,    33,    34,    35,    36,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    21,    21,     5,     6,     7,     8,     9,
      19,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      59,    21,     5,     6,     7,     8,     9,    58,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    63,    21,     5,
       6,     7,     8,     9,    59,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    58,    21,     5,     6,     7,     8,
       9,    19,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    19,    21,     5,     6,    19,    58,     5,    19,    11,
      12,    13,    14,    15,    16,    17,    18,     5,     6,    21,
      59,    20,    19,    11,    12,    13,    14,    15,    16,    17,
      18,     5,     6,    21,    19,    60,    60,    11,    12,    13,
      14,    15,    16,    17,    18,    21,    21,    21,    60,    21,
      58,    19,    58,    58,    58,    19,    19,    19,    58,    19,
      61,    32,    19,    24,    60,    59,    59,    19,    60,    60,
      60,    19,    61,    19,    61,    61,    19,    61,    61,    61,
      61,    61,    61,    61,    61,    24,    24,    56,    61,    24,
      19,    61,    59,    59,    19,    19,    24,    62,    32,    58,
      19,    24,    60,    59,    59,    59,    56,    19,    59,    58,
      24,    24,    59,    58,    19,    59,    15,    59,    58,   187,
      61,    58,   186,    59,    56,   183,    59,   279,    59,    59,
      56,   236,    50,   290,    61,    -1,   139,   383,    60,    60,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   105
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     3,     9,    24,    25,    26,    27,    28,    29,    30,
      31,    48,    49,    70,    71,    72,    73,    74,   114,   117,
     119,    21,    19,    19,    19,     0,    71,    73,   114,   117,
     119,    72,    56,     4,    56,    56,    56,    72,    19,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    57,    60,
      76,    77,    78,    82,    83,    85,   105,    21,    19,   115,
     116,    19,   120,    19,   118,    19,    21,    58,    78,   105,
      19,    58,    92,     5,     6,     7,     8,     9,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    21,    78,    94,
      95,    96,    97,    98,    92,    58,    57,    77,    39,    40,
      94,    58,    75,    61,    57,    62,    57,    62,    57,    62,
     105,    19,    94,    19,    19,    93,    19,    91,     5,    19,
      96,    98,     5,     5,    96,    94,    19,    64,    99,     5,
      96,    19,    19,    78,   100,   101,   102,    21,    94,    92,
      19,    19,    19,    20,    23,    68,   121,    21,    57,   116,
      21,    19,    57,    21,    19,    57,    59,    19,    58,    86,
      63,    59,    58,    19,    19,    19,    58,     5,    99,     5,
      19,    32,    58,    78,   102,    19,   102,    94,    59,    19,
      19,   100,    19,    22,    58,    61,    79,    80,    81,    19,
      20,    21,    32,    59,    20,    60,    21,    60,    21,    60,
      21,    56,    60,   104,    19,    42,    43,    44,    45,    46,
      47,    49,    50,    51,    52,    53,    54,    55,    87,    90,
      19,    60,    93,    19,    58,    84,    58,     5,     6,    19,
      94,   108,   109,   110,   111,    99,    58,    19,   103,   102,
     102,    19,    94,    58,    94,    19,    56,    81,   108,    56,
     121,    80,    19,    79,    60,    32,    59,    59,    59,    32,
      75,    60,    60,    60,    24,    61,    61,    61,    61,    61,
      61,    61,    61,    61,    61,    61,    61,    61,    59,    62,
      56,    94,    19,    19,   108,    19,     5,    62,    19,    59,
      62,   103,    59,    62,    58,    19,   108,    19,    24,    60,
      59,    24,    32,    59,    75,    75,    75,    32,    59,    19,
      32,    88,    88,    88,   121,   121,    19,    88,    89,   121,
      93,    94,    19,    21,    19,    21,    19,    21,    19,    21,
      19,    21,    90,    24,    59,    59,    56,    59,    62,    62,
     109,    22,    58,    19,   106,    10,   111,    59,    19,   108,
      58,    59,    58,   106,    59,    75,    59,    75,    58,    19,
      60,    24,   106,   109,   109,    58,    19,    56,   107,   121,
     104,    59,   108,   106,   108,   104,    75,    75,    32,    56,
      19,    60,   104,    19,    19,    61,    65,    66,    67,   112,
     113,    24,    19,   106,    59,   104,    59,    59,    24,    56,
     112,    61,    61,   121,    61,   121,    61,    59,   113,   107,
     104,   106,   106,    60,    24,    59,   121,   121,   121,   121,
      19,   104,   104,    60,   107
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))

/* Error token number */
#define YYTERROR	1
#define YYERRCODE	256


/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */
#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
        break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULL, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULL;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULL, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
        break;
    }
}




/* The lookahead symbol.  */
int yychar;


#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval YY_INITIAL_VALUE(yyval_default);

/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
/* Line 1792 of yacc.c  */
#line 713 "parse.y"
    { ; }
    break;

  case 3:
/* Line 1792 of yacc.c  */
#line 714 "parse.y"
    { ; }
    break;

  case 4:
/* Line 1792 of yacc.c  */
#line 715 "parse.y"
    { ; }
    break;

  case 5:
/* Line 1792 of yacc.c  */
#line 716 "parse.y"
    { ; }
    break;

  case 6:
/* Line 1792 of yacc.c  */
#line 719 "parse.y"
    {
			Node *node = node_new (CCODE_NODE,
					       "cctype", C_CCODE,
					       "cbuf:steal", ((yyvsp[(1) - (1)].cbuf))->str,
					       "line_no", ccode_line,
					       NULL);
			nodes = g_list_append(nodes,node);
			g_string_free((yyvsp[(1) - (1)].cbuf),FALSE);
					}
    break;

  case 7:
/* Line 1792 of yacc.c  */
#line 728 "parse.y"
    {
			Node *node = node_new (CCODE_NODE,
					       "cctype", AD_CCODE,
					       "cbuf:steal", ((yyvsp[(1) - (1)].cbuf))->str,
					       "line_no", ccode_line,
					       NULL);
			nodes = g_list_append(nodes,node);
			g_string_free((yyvsp[(1) - (1)].cbuf),FALSE);
					}
    break;

  case 8:
/* Line 1792 of yacc.c  */
#line 737 "parse.y"
    {
			Node *node = node_new (CCODE_NODE,
					       "cctype", H_CCODE,
					       "cbuf:steal", ((yyvsp[(1) - (1)].cbuf))->str,
					       "line_no", ccode_line,
					       NULL);
			nodes = g_list_append(nodes,node);
			g_string_free((yyvsp[(1) - (1)].cbuf),FALSE);
					}
    break;

  case 9:
/* Line 1792 of yacc.c  */
#line 746 "parse.y"
    {
			Node *node = node_new (CCODE_NODE,
					       "cctype", HT_CCODE,
					       "cbuf:steal", ((yyvsp[(1) - (1)].cbuf))->str,
					       "line_no", ccode_line,
					       NULL);
			nodes = g_list_append(nodes,node);
			g_string_free((yyvsp[(1) - (1)].cbuf),FALSE);
					}
    break;

  case 10:
/* Line 1792 of yacc.c  */
#line 755 "parse.y"
    {
			Node *node = node_new (CCODE_NODE,
					       "cctype", PH_CCODE,
					       "cbuf:steal", ((yyvsp[(1) - (1)].cbuf))->str,
					       "line_no", ccode_line,
					       NULL);
			nodes = g_list_append(nodes,node);
			g_string_free((yyvsp[(1) - (1)].cbuf),FALSE);
					}
    break;

  case 11:
/* Line 1792 of yacc.c  */
#line 764 "parse.y"
    {
			Node *node = node_new (CCODE_NODE,
					       "cctype", A_CCODE,
					       "cbuf:steal", ((yyvsp[(1) - (1)].cbuf))->str,
					       "line_no", ccode_line,
					       NULL);
			nodes = g_list_append(nodes,node);
			g_string_free((yyvsp[(1) - (1)].cbuf),FALSE);
					}
    break;

  case 12:
/* Line 1792 of yacc.c  */
#line 773 "parse.y"
    {
			Node *node = node_new (CCODE_NODE,
					       "cctype", AT_CCODE,
					       "cbuf:steal", ((yyvsp[(1) - (1)].cbuf))->str,
					       "line_no", ccode_line,
					       NULL);
			nodes = g_list_append(nodes,node);
			g_string_free((yyvsp[(1) - (1)].cbuf),FALSE);
					}
    break;

  case 13:
/* Line 1792 of yacc.c  */
#line 782 "parse.y"
    {
			Node *node = node_new (CCODE_NODE,
					       "cctype", CT_CCODE,
					       "cbuf:steal", ((yyvsp[(1) - (1)].cbuf))->str,
					       "line_no", ccode_line,
					       NULL);
			nodes = g_list_append(nodes,node);
			g_string_free((yyvsp[(1) - (1)].cbuf),FALSE);
					}
    break;

  case 14:
/* Line 1792 of yacc.c  */
#line 793 "parse.y"
    { ; }
    break;

  case 15:
/* Line 1792 of yacc.c  */
#line 794 "parse.y"
    { ; }
    break;

  case 16:
/* Line 1792 of yacc.c  */
#line 795 "parse.y"
    { ; }
    break;

  case 17:
/* Line 1792 of yacc.c  */
#line 796 "parse.y"
    { ; }
    break;

  case 18:
/* Line 1792 of yacc.c  */
#line 797 "parse.y"
    { ; }
    break;

  case 19:
/* Line 1792 of yacc.c  */
#line 798 "parse.y"
    { ; }
    break;

  case 20:
/* Line 1792 of yacc.c  */
#line 799 "parse.y"
    { ; }
    break;

  case 21:
/* Line 1792 of yacc.c  */
#line 800 "parse.y"
    { ; }
    break;

  case 22:
/* Line 1792 of yacc.c  */
#line 803 "parse.y"
    {
			((Class *)class)->nodes = class_nodes;
			class_nodes = NULL;
			nodes = g_list_append(nodes,class);
						}
    break;

  case 23:
/* Line 1792 of yacc.c  */
#line 808 "parse.y"
    {
			((Class *)class)->nodes = NULL;
			class_nodes = NULL;
			nodes = g_list_append(nodes,class);
						}
    break;

  case 24:
/* Line 1792 of yacc.c  */
#line 815 "parse.y"
    {
			class = node_new (CLASS_NODE,
					  "otype:steal", (yyvsp[(2) - (5)].id),
					  "ptype:steal", (yyvsp[(4) - (5)].id),
					  "bonobo_object_class:steal", bonobo_object_class,
					  "glade_xml", glade_xml,
					  "interfaces:steal", interfaces,
					  "chunk_size:steal", chunk_size,
					  "abstract", abstract,
					  NULL);
			bonobo_object_class = NULL;
			glade_xml = FALSE;
			chunk_size = NULL;
			interfaces = NULL;
						}
    break;

  case 26:
/* Line 1792 of yacc.c  */
#line 833 "parse.y"
    {
			if(strcmp((yyvsp[(2) - (4)].id),"abstract") == 0) {
				abstract = TRUE;
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
		}
    break;

  case 27:
/* Line 1792 of yacc.c  */
#line 841 "parse.y"
    {
			if(strcmp((yyvsp[(2) - (5)].id),"chunks") == 0) {
				g_free (chunk_size);
				chunk_size = g_strdup((yyvsp[(3) - (5)].id));
			} else if(strcmp((yyvsp[(2) - (5)].id),"BonoboObject") == 0) {
				g_free (bonobo_object_class);
				bonobo_object_class = g_strdup((yyvsp[(3) - (5)].id));
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
		}
    break;

  case 28:
/* Line 1792 of yacc.c  */
#line 853 "parse.y"
    {
			if (strcmp ((yyvsp[(2) - (5)].id), "interface") == 0) {
				interfaces = g_list_append (interfaces,
							    g_strdup ((yyvsp[(3) - (5)].id)));
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
		}
    break;

  case 29:
/* Line 1792 of yacc.c  */
#line 862 "parse.y"
    {
			if(strcmp((yyvsp[(2) - (5)].id),"chunks") == 0) {
				g_free (chunk_size);
				if(atoi((yyvsp[(3) - (5)].id)) != 0)
					chunk_size = g_strdup((yyvsp[(3) - (5)].id));
				else
					chunk_size = NULL;
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
		}
    break;

  case 30:
/* Line 1792 of yacc.c  */
#line 874 "parse.y"
    {
			if (strcmp ((yyvsp[(2) - (6)].id), "GladeXML") == 0) {
				glade_xml = TRUE;
				add_construct_glade((yyvsp[(3) - (6)].id), (yyvsp[(4) - (6)].id), NULL);
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
		}
    break;

  case 31:
/* Line 1792 of yacc.c  */
#line 883 "parse.y"
    {
			if (strcmp ((yyvsp[(2) - (7)].id), "GladeXML") == 0) {
				glade_xml = TRUE;
				add_construct_glade((yyvsp[(3) - (7)].id), (yyvsp[(4) - (7)].id), (yyvsp[(5) - (7)].id));
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
		}
    break;

  case 32:
/* Line 1792 of yacc.c  */
#line 892 "parse.y"
    {
			if (strcmp ((yyvsp[(2) - (6)].id), "GladeXML") == 0) {
				glade_xml = TRUE;
				add_construct_glade((yyvsp[(3) - (6)].id), (yyvsp[(4) - (6)].id), NULL);
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
		}
    break;

  case 33:
/* Line 1792 of yacc.c  */
#line 901 "parse.y"
    {
			if (strcmp ((yyvsp[(2) - (7)].id), "GladeXML") == 0) {
				glade_xml = TRUE;
				add_construct_glade((yyvsp[(3) - (7)].id), (yyvsp[(4) - (7)].id), (yyvsp[(5) - (7)].id));
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
		}
    break;

  case 34:
/* Line 1792 of yacc.c  */
#line 912 "parse.y"
    { ; }
    break;

  case 35:
/* Line 1792 of yacc.c  */
#line 913 "parse.y"
    { ; }
    break;

  case 36:
/* Line 1792 of yacc.c  */
#line 916 "parse.y"
    { ; }
    break;

  case 37:
/* Line 1792 of yacc.c  */
#line 917 "parse.y"
    {
			if (strcmp ((yyvsp[(1) - (2)].id), "BonoboObject") != 0) {
				g_free ((yyvsp[(1) - (2)].id));
				yyerror (_("parse error"));
				YYERROR;
			}
			g_free ((yyvsp[(1) - (2)].id));
			last_added_method->bonobo_object_func = TRUE;
						}
    break;

  case 38:
/* Line 1792 of yacc.c  */
#line 926 "parse.y"
    {
			if (strcmp ((yyvsp[(1) - (3)].id), "interface") != 0) {
				g_free ((yyvsp[(1) - (3)].id));
				g_free ((yyvsp[(2) - (3)].id));
				yyerror (_("parse error"));
				YYERROR;
			}
			g_free ((yyvsp[(1) - (3)].id));
			node_set ((Node *)last_added_method,
				  "interface:steal", (yyvsp[(2) - (3)].id),
				  NULL);
						}
    break;

  case 39:
/* Line 1792 of yacc.c  */
#line 938 "parse.y"
    { ; }
    break;

  case 40:
/* Line 1792 of yacc.c  */
#line 939 "parse.y"
    { ; }
    break;

  case 41:
/* Line 1792 of yacc.c  */
#line 940 "parse.y"
    { ; }
    break;

  case 42:
/* Line 1792 of yacc.c  */
#line 941 "parse.y"
    { ; }
    break;

  case 43:
/* Line 1792 of yacc.c  */
#line 944 "parse.y"
    { the_scope = PUBLIC_SCOPE; }
    break;

  case 44:
/* Line 1792 of yacc.c  */
#line 945 "parse.y"
    { the_scope = PRIVATE_SCOPE; }
    break;

  case 45:
/* Line 1792 of yacc.c  */
#line 946 "parse.y"
    { the_scope = PROTECTED_SCOPE; }
    break;

  case 46:
/* Line 1792 of yacc.c  */
#line 947 "parse.y"
    { the_scope = CLASS_SCOPE; }
    break;

  case 47:
/* Line 1792 of yacc.c  */
#line 950 "parse.y"
    {
			if (strcmp ((yyvsp[(1) - (2)].id), "destroywith") == 0) {
				g_free ((yyvsp[(1) - (2)].id));
				destructor_unref = FALSE;
				destructor = (yyvsp[(2) - (2)].id);
				destructor_line = line_no;
				destructor_simple = TRUE;
			} else if (strcmp ((yyvsp[(1) - (2)].id), "unrefwith") == 0) {
				g_free ((yyvsp[(1) - (2)].id));
				destructor_unref = TRUE;
				destructor = (yyvsp[(2) - (2)].id);
				destructor_line = line_no;
				destructor_simple = TRUE;
			} else {
				g_free ((yyvsp[(1) - (2)].id));
				g_free ((yyvsp[(2) - (2)].id));
				yyerror (_("parse error"));
				YYERROR;
			}
				}
    break;

  case 48:
/* Line 1792 of yacc.c  */
#line 970 "parse.y"
    {
			if (strcmp ((yyvsp[(1) - (3)].id), "destroy") == 0) {
				g_free((yyvsp[(1) - (3)].id));
				destructor_unref = FALSE;
				destructor = ((yyvsp[(3) - (3)].cbuf))->str;
				g_string_free((yyvsp[(3) - (3)].cbuf), FALSE);
				destructor_line = ccode_line;
				destructor_simple = FALSE;
			} else if (strcmp ((yyvsp[(1) - (3)].id), "unref") == 0) {
				g_free ((yyvsp[(1) - (3)].id));
				destructor_unref = TRUE;
				destructor = ((yyvsp[(3) - (3)].cbuf))->str;
				g_string_free ((yyvsp[(3) - (3)].cbuf), FALSE);
				destructor_line = ccode_line;
				destructor_simple = FALSE;
			} else {
				g_free ((yyvsp[(1) - (3)].id));
				g_string_free ((yyvsp[(3) - (3)].cbuf), TRUE);
				yyerror (_("parse error"));
				YYERROR;
			}
					}
    break;

  case 49:
/* Line 1792 of yacc.c  */
#line 994 "parse.y"
    {
			initializer = (yyvsp[(2) - (2)].id);
			initializer_line = ccode_line;
				}
    break;

  case 50:
/* Line 1792 of yacc.c  */
#line 998 "parse.y"
    {
			initializer = ((yyvsp[(3) - (3)].cbuf))->str;
			initializer_line = ccode_line;
			g_string_free((yyvsp[(3) - (3)].cbuf), FALSE);
				}
    break;

  case 51:
/* Line 1792 of yacc.c  */
#line 1006 "parse.y"
    { ; }
    break;

  case 52:
/* Line 1792 of yacc.c  */
#line 1007 "parse.y"
    { ; }
    break;

  case 53:
/* Line 1792 of yacc.c  */
#line 1008 "parse.y"
    { destructor = NULL; }
    break;

  case 54:
/* Line 1792 of yacc.c  */
#line 1009 "parse.y"
    { initializer = NULL; }
    break;

  case 55:
/* Line 1792 of yacc.c  */
#line 1010 "parse.y"
    {
			if (strcmp ((yyvsp[(1) - (1)].id), "GladeXML") == 0) {
				glade_widget = TRUE;
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
					}
    break;

  case 56:
/* Line 1792 of yacc.c  */
#line 1018 "parse.y"
    {
			destructor = NULL;
			initializer = NULL;
					}
    break;

  case 57:
/* Line 1792 of yacc.c  */
#line 1024 "parse.y"
    {
			push_variable((yyvsp[(3) - (5)].id), the_scope,(yyvsp[(1) - (5)].line), NULL);
						}
    break;

  case 58:
/* Line 1792 of yacc.c  */
#line 1027 "parse.y"
    {
			push_variable((yyvsp[(3) - (6)].id), the_scope, (yyvsp[(1) - (6)].line), (yyvsp[(4) - (6)].id));
						}
    break;

  case 59:
/* Line 1792 of yacc.c  */
#line 1032 "parse.y"
    {
			Node *node = NULL;
			if(strcmp((yyvsp[(6) - (12)].id),"get")==0 &&
			   strcmp((yyvsp[(9) - (12)].id),"set")==0) {
				Type *type = pop_type();
				g_free ((yyvsp[(6) - (12)].id)); 
				g_free ((yyvsp[(9) - (12)].id));
				node = node_new (ARGUMENT_NODE,
						 "gtktype:steal", (yyvsp[(3) - (12)].id),
						 "atype:steal", type,
						 "flags:steal", (yyvsp[(2) - (12)].list),
						 "name:steal", (yyvsp[(4) - (12)].id),
						 "get:steal", ((yyvsp[(8) - (12)].cbuf))->str,
						 "get_line", (yyvsp[(7) - (12)].line),
						 "set:steal", ((yyvsp[(11) - (12)].cbuf))->str,
						 "set_line", (yyvsp[(10) - (12)].line),
						 "line_no", (yyvsp[(1) - (12)].line),
						 NULL);

				class_nodes = g_list_append(class_nodes,node);

				g_string_free ((yyvsp[(8) - (12)].cbuf), FALSE);
				g_string_free ((yyvsp[(11) - (12)].cbuf), FALSE);

			} else if(strcmp((yyvsp[(6) - (12)].id),"set")==0 &&
				strcmp((yyvsp[(9) - (12)].id),"get")==0) {
				Type *type = pop_type();
				g_free ((yyvsp[(6) - (12)].id)); 
				g_free ((yyvsp[(9) - (12)].id));
				node = node_new (ARGUMENT_NODE,
						 "gtktype:steal", (yyvsp[(3) - (12)].id),
						 "atype:steal", type,
						 "flags:steal", (yyvsp[(2) - (12)].list),
						 "name:steal", (yyvsp[(4) - (12)].id),
						 "get:steal", ((yyvsp[(11) - (12)].cbuf))->str,
						 "get_line", (yyvsp[(10) - (12)].line),
						 "set:steal", ((yyvsp[(8) - (12)].cbuf))->str,
						 "set_line", (yyvsp[(7) - (12)].line),
						 "line_no", (yyvsp[(1) - (12)].line),
						 NULL);
				g_string_free ((yyvsp[(11) - (12)].cbuf), FALSE);
				g_string_free ((yyvsp[(8) - (12)].cbuf), FALSE);
				class_nodes = g_list_append(class_nodes,node);
			} else {
				g_free ((yyvsp[(3) - (12)].id)); 
				g_free ((yyvsp[(4) - (12)].id));
				g_free ((yyvsp[(6) - (12)].id)); 
				g_free ((yyvsp[(9) - (12)].id));
				g_list_foreach ((yyvsp[(2) - (12)].list), (GFunc)g_free, NULL);
				g_list_free ((yyvsp[(2) - (12)].list));
				g_string_free ((yyvsp[(11) - (12)].cbuf), TRUE);
				g_string_free ((yyvsp[(8) - (12)].cbuf), TRUE);
				yyerror (_("parse error"));
				YYERROR;
			}

			if ((yyvsp[(5) - (12)].id) != NULL) {
				Argument *arg = (Argument *)node;
				export_accessors (arg->name,
						  arg->get != NULL, arg->get_line,
						  arg->set != NULL, arg->set_line,
						  arg->atype,
						  arg->gtktype,
						  arg->line_no);
				g_free ((yyvsp[(5) - (12)].id));
			} 

						}
    break;

  case 60:
/* Line 1792 of yacc.c  */
#line 1100 "parse.y"
    {
			Node *node = NULL;
			if(strcmp((yyvsp[(6) - (9)].id), "get") == 0) {
				Type *type = pop_type();
				g_free ((yyvsp[(6) - (9)].id));
				node = node_new (ARGUMENT_NODE,
						 "gtktype:steal", (yyvsp[(3) - (9)].id),
						 "atype:steal", type,
						 "flags:steal", (yyvsp[(2) - (9)].list),
						 "name:steal", (yyvsp[(4) - (9)].id),
						 "get:steal", ((yyvsp[(8) - (9)].cbuf))->str,
						 "get_line", (yyvsp[(7) - (9)].line),
						 "line_no", (yyvsp[(1) - (9)].line),
						 NULL);

				g_string_free ((yyvsp[(8) - (9)].cbuf), FALSE);
				class_nodes = g_list_append(class_nodes, node);
			} else if(strcmp((yyvsp[(6) - (9)].id), "set") == 0) {
				Type *type = pop_type();
				g_free ((yyvsp[(6) - (9)].id));
				node = node_new (ARGUMENT_NODE,
						 "gtktype:steal", (yyvsp[(3) - (9)].id),
						 "atype:steal", type,
						 "flags:steal", (yyvsp[(2) - (9)].list),
						 "name:steal", (yyvsp[(4) - (9)].id),
						 "set:steal", ((yyvsp[(8) - (9)].cbuf))->str,
						 "set_line", (yyvsp[(7) - (9)].line),
						 "line_no", (yyvsp[(1) - (9)].line),
						 NULL);

				g_string_free ((yyvsp[(8) - (9)].cbuf), FALSE);
				class_nodes = g_list_append (class_nodes, node);
			} else {
				g_free ((yyvsp[(6) - (9)].id)); 
				g_free ((yyvsp[(3) - (9)].id));
				g_free ((yyvsp[(4) - (9)].id));
				g_list_foreach ((yyvsp[(2) - (9)].list), (GFunc)g_free, NULL);
				g_list_free ((yyvsp[(2) - (9)].list));
				g_string_free ((yyvsp[(8) - (9)].cbuf), TRUE);
				yyerror(_("parse error"));
				YYERROR;
			}

			if ((yyvsp[(5) - (9)].id) != NULL) {
				Argument *arg = (Argument *)node;
				export_accessors (arg->name,
						  arg->get != NULL, arg->get_line,
						  arg->set != NULL, arg->set_line,
						  arg->atype,
						  arg->gtktype,
						  arg->line_no);
				g_free ((yyvsp[(5) - (9)].id));
			} 
						}
    break;

  case 61:
/* Line 1792 of yacc.c  */
#line 1154 "parse.y"
    {
			Node *node;
			char *get, *set = NULL;
			Variable *var;
			Type *type;
			const char *root;
			
			if(strcmp((yyvsp[(6) - (6)].id), "link")!=0 &&
			   strcmp((yyvsp[(6) - (6)].id), "stringlink")!=0 && 
			   strcmp((yyvsp[(6) - (6)].id), "objectlink")!=0) {
				g_free((yyvsp[(6) - (6)].id)); 
				g_free((yyvsp[(3) - (6)].id));
				g_free((yyvsp[(4) - (6)].id));
				g_list_foreach((yyvsp[(2) - (6)].list),(GFunc)g_free,NULL);
				g_list_free((yyvsp[(2) - (6)].list));
				yyerror(_("parse error"));
				YYERROR;
			}

			type = pop_type();

			var = find_var_or_die((yyvsp[(4) - (6)].id), (yyvsp[(1) - (6)].line));
			if(var->scope == PRIVATE_SCOPE) {
				root = "self->_priv";
			} else if(var->scope == CLASS_SCOPE) {
				root = "SELF_GET_CLASS(self)";
				if(no_self_alias)
					error_print(GOB_ERROR, (yyvsp[(1) - (6)].line),
						    _("Self aliases needed when autolinking to a classwide member"));
			} else {
				root = "self";
			}

			if(strcmp((yyvsp[(6) - (6)].id), "link")==0) {
				set = g_strdup_printf("%s->%s = ARG;",
						      root, (yyvsp[(4) - (6)].id));
			} else if(strcmp((yyvsp[(6) - (6)].id), "stringlink")==0) {
				set = g_strdup_printf("g_free (%s->%s); "
						      "%s->%s = g_strdup (ARG);",
						      root, (yyvsp[(4) - (6)].id),
						      root, (yyvsp[(4) - (6)].id));
			} else if(strcmp((yyvsp[(6) - (6)].id), "objectlink")==0) {
				set = g_strdup_printf(
				  "if (ARG != NULL) "
				   "g_object_ref (G_OBJECT (ARG)); "
				  "if (%s->%s != NULL) "
				   "g_object_unref (G_OBJECT (%s->%s)); "
				  "%s->%s = ARG;",
				  root, (yyvsp[(4) - (6)].id),
				  root, (yyvsp[(4) - (6)].id),
				  root, (yyvsp[(4) - (6)].id));
			} else {
				g_assert_not_reached();
			}

			get = g_strdup_printf("ARG = %s->%s;", root, (yyvsp[(4) - (6)].id));
  
			g_free ((yyvsp[(6) - (6)].id));

			if (type == NULL)
				type = (Type *)node_copy ((Node *)var->vtype);

			node = node_new (ARGUMENT_NODE,
					 "gtktype:steal", (yyvsp[(3) - (6)].id),
					 "atype:steal", type,
					 "flags:steal", (yyvsp[(2) - (6)].list),
					 "name:steal", (yyvsp[(4) - (6)].id),
					 "get:steal", get,
					 "get_line", (yyvsp[(1) - (6)].line),
					 "set:steal", set,
					 "set_line", (yyvsp[(1) - (6)].line),
					 "line_no", (yyvsp[(1) - (6)].line),
					 NULL);

			if ((yyvsp[(5) - (6)].id) != NULL) {
				Argument *arg = (Argument *)node;
				export_accessors (arg->name,
						  arg->get != NULL, arg->get_line,
						  arg->set != NULL, arg->set_line,
						  arg->atype,
						  arg->gtktype,
						  arg->line_no);
				g_free ((yyvsp[(5) - (6)].id));
			} 

			class_nodes = g_list_append (class_nodes, node);
						}
    break;

  case 62:
/* Line 1792 of yacc.c  */
#line 1243 "parse.y"
    {
			if (strcmp ((yyvsp[(2) - (3)].id), "export")!=0) {
				g_free ((yyvsp[(2) - (3)].id)); 
				yyerror (_("parse error"));
				YYERROR;
			}
			(yyval.id) = (yyvsp[(2) - (3)].id);
						}
    break;

  case 63:
/* Line 1792 of yacc.c  */
#line 1251 "parse.y"
    {
			(yyval.id) = NULL;
						}
    break;

  case 64:
/* Line 1792 of yacc.c  */
#line 1256 "parse.y"
    {
			ensure_property ();
			node_set ((Node *)property,
				  "line_no", (yyvsp[(1) - (11)].line),
				  "gtktype:steal", debool ((yyvsp[(2) - (11)].id)),
				  "name:steal", (yyvsp[(3) - (11)].id),
				  NULL);
			if (strcmp ((yyvsp[(5) - (11)].id), "get") == 0 &&
			    strcmp ((yyvsp[(8) - (11)].id), "set") == 0) {
				node_set ((Node *)property,
					  "get:steal", ((yyvsp[(7) - (11)].cbuf))->str,
					  "get_line", (yyvsp[(6) - (11)].line),
					  "set:steal", ((yyvsp[(10) - (11)].cbuf))->str,
					  "set_line", (yyvsp[(9) - (11)].line),
					  NULL);
				g_string_free ((yyvsp[(7) - (11)].cbuf), FALSE);
				g_string_free ((yyvsp[(10) - (11)].cbuf), FALSE);
				g_free ((yyvsp[(5) - (11)].id)); 
				g_free ((yyvsp[(8) - (11)].id));
			} else if (strcmp ((yyvsp[(5) - (11)].id), "set") == 0 &&
				   strcmp ((yyvsp[(8) - (11)].id), "get") == 0) {
				node_set ((Node *)property,
					  "get:steal", ((yyvsp[(10) - (11)].cbuf))->str,
					  "get_line", (yyvsp[(9) - (11)].line),
					  "set:steal", ((yyvsp[(7) - (11)].cbuf))->str,
					  "set_line", (yyvsp[(6) - (11)].line),
					  NULL);
				g_string_free ((yyvsp[(7) - (11)].cbuf), FALSE);
				g_string_free ((yyvsp[(10) - (11)].cbuf), FALSE);
				g_free ((yyvsp[(5) - (11)].id)); 
				g_free ((yyvsp[(8) - (11)].id));
			} else {
				g_string_free ((yyvsp[(7) - (11)].cbuf), TRUE);
				g_string_free ((yyvsp[(10) - (11)].cbuf), TRUE);
				g_free ((yyvsp[(5) - (11)].id)); 
				g_free ((yyvsp[(8) - (11)].id));
				node_free ((Node *)property);
				property = NULL;
				yyerror (_("parse error"));
				YYERROR;
			}
			property_link_and_export ((Node *)property);
			if (property != NULL) {
				class_nodes = g_list_append (class_nodes,
							     property);
				property = NULL;
			}
		}
    break;

  case 65:
/* Line 1792 of yacc.c  */
#line 1304 "parse.y"
    {
			ensure_property ();
			node_set ((Node *)property,
				  "line_no", (yyvsp[(1) - (8)].line),
				  "gtktype:steal", debool ((yyvsp[(2) - (8)].id)),
				  "name:steal", (yyvsp[(3) - (8)].id),
				  NULL);
			if (strcmp ((yyvsp[(5) - (8)].id), "get") == 0) {
				node_set ((Node *)property,
					  "get:steal", ((yyvsp[(7) - (8)].cbuf))->str,
					  "get_line", (yyvsp[(6) - (8)].line),
					  NULL);
				g_string_free ((yyvsp[(7) - (8)].cbuf), FALSE);
				g_free ((yyvsp[(5) - (8)].id)); 
			} else if (strcmp ((yyvsp[(5) - (8)].id), "set") == 0) {
				node_set ((Node *)property,
					  "set:steal", ((yyvsp[(7) - (8)].cbuf))->str,
					  "set_line", (yyvsp[(6) - (8)].line),
					  NULL);
				g_string_free ((yyvsp[(7) - (8)].cbuf), FALSE);
				g_free ((yyvsp[(5) - (8)].id)); 
			} else {
				g_string_free ((yyvsp[(7) - (8)].cbuf), TRUE);
				g_free ((yyvsp[(5) - (8)].id)); 
				node_free ((Node *)property);
				property = NULL;
				yyerror (_("parse error"));
				YYERROR;
			}
			property_link_and_export ((Node *)property);
			if (property != NULL) {
				class_nodes = g_list_append (class_nodes,
							     property);
				property = NULL;
			}
		}
    break;

  case 66:
/* Line 1792 of yacc.c  */
#line 1340 "parse.y"
    {
			ensure_property ();
			node_set ((Node *)property,
				  "line_no", (yyvsp[(1) - (5)].line),
				  "gtktype:steal", debool ((yyvsp[(2) - (5)].id)),
				  "name:steal", (yyvsp[(3) - (5)].id),
				  NULL);
			property_link_and_export ((Node *)property);
			if (property != NULL) {
				class_nodes = g_list_append (class_nodes,
							     property);
				property = NULL;
			}
		}
    break;

  case 67:
/* Line 1792 of yacc.c  */
#line 1356 "parse.y"
    { ; }
    break;

  case 68:
/* Line 1792 of yacc.c  */
#line 1357 "parse.y"
    { ; }
    break;

  case 69:
/* Line 1792 of yacc.c  */
#line 1360 "parse.y"
    { ; }
    break;

  case 70:
/* Line 1792 of yacc.c  */
#line 1361 "parse.y"
    { ; }
    break;

  case 71:
/* Line 1792 of yacc.c  */
#line 1364 "parse.y"
    { (yyval.id) = (yyvsp[(1) - (1)].id); }
    break;

  case 72:
/* Line 1792 of yacc.c  */
#line 1365 "parse.y"
    {
			if (strcmp ((yyvsp[(1) - (4)].id), "_") != 0) {
				g_free ((yyvsp[(1) - (4)].id));
				yyerror(_("parse error"));
				YYERROR;
			}
			g_free ((yyvsp[(1) - (4)].id));
			(yyval.id) = g_strconcat ("_(", (yyvsp[(3) - (4)].id), ")", NULL);
			g_free ((yyvsp[(3) - (4)].id));
		}
    break;

  case 73:
/* Line 1792 of yacc.c  */
#line 1377 "parse.y"
    { (yyval.id) = (yyvsp[(1) - (1)].id); }
    break;

  case 74:
/* Line 1792 of yacc.c  */
#line 1378 "parse.y"
    { (yyval.id) = (yyvsp[(1) - (1)].id); }
    break;

  case 75:
/* Line 1792 of yacc.c  */
#line 1381 "parse.y"
    {
		ensure_property ();
		node_set ((Node *)property,
			  "canonical_name:steal", gob_str_delete_quotes((yyvsp[(3) - (3)].id)),
			  NULL);
		  }
    break;

  case 76:
/* Line 1792 of yacc.c  */
#line 1387 "parse.y"
    {
		ensure_property ();
		node_set ((Node *)property,
			  "nick:steal", (yyvsp[(3) - (3)].id),
			  NULL);
		  }
    break;

  case 77:
/* Line 1792 of yacc.c  */
#line 1393 "parse.y"
    {
		ensure_property ();
		node_set ((Node *)property,
			  "blurb:steal", (yyvsp[(3) - (3)].id),
			  NULL);
		  }
    break;

  case 78:
/* Line 1792 of yacc.c  */
#line 1399 "parse.y"
    {
		ensure_property ();
		node_set ((Node *)property,
			  "maximum:steal", (yyvsp[(3) - (3)].id),
			  NULL);
		  }
    break;

  case 79:
/* Line 1792 of yacc.c  */
#line 1405 "parse.y"
    {
		ensure_property ();
		node_set ((Node *)property,
			  "minimum:steal", (yyvsp[(3) - (3)].id),
			  NULL);
		  }
    break;

  case 80:
/* Line 1792 of yacc.c  */
#line 1411 "parse.y"
    {
		ensure_property ();
		node_set ((Node *)property,
			  "default_value:steal", (yyvsp[(3) - (3)].id),
			  NULL);
		  }
    break;

  case 81:
/* Line 1792 of yacc.c  */
#line 1417 "parse.y"
    {
		ensure_property ();
		node_set ((Node *)property,
			  "flags:steal", (yyvsp[(3) - (3)].list),
			  NULL);
		  }
    break;

  case 82:
/* Line 1792 of yacc.c  */
#line 1423 "parse.y"
    {
		Type *type = pop_type ();
		ensure_property ();
		node_set ((Node *)property,
			  "ptype:steal", type,
			  NULL);
		  }
    break;

  case 83:
/* Line 1792 of yacc.c  */
#line 1430 "parse.y"
    {
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", (yyvsp[(3) - (3)].id),
			  NULL);
		  }
    break;

  case 84:
/* Line 1792 of yacc.c  */
#line 1436 "parse.y"
    {
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", (yyvsp[(3) - (3)].id),
			  NULL);
		  }
    break;

  case 85:
/* Line 1792 of yacc.c  */
#line 1442 "parse.y"
    {
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", (yyvsp[(3) - (3)].id),
			  NULL);
		  }
    break;

  case 86:
/* Line 1792 of yacc.c  */
#line 1448 "parse.y"
    {
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", (yyvsp[(3) - (3)].id),
			  NULL);
		  }
    break;

  case 87:
/* Line 1792 of yacc.c  */
#line 1454 "parse.y"
    {
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", (yyvsp[(3) - (3)].id),
			  NULL);
		  }
    break;

  case 88:
/* Line 1792 of yacc.c  */
#line 1460 "parse.y"
    {
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", (yyvsp[(3) - (3)].id),
			  NULL);
		  }
    break;

  case 89:
/* Line 1792 of yacc.c  */
#line 1466 "parse.y"
    {
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", (yyvsp[(3) - (3)].id),
			  NULL);
		  }
    break;

  case 90:
/* Line 1792 of yacc.c  */
#line 1472 "parse.y"
    {
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", (yyvsp[(3) - (3)].id),
			  NULL);
		  }
    break;

  case 91:
/* Line 1792 of yacc.c  */
#line 1478 "parse.y"
    {
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", (yyvsp[(3) - (3)].id),
			  NULL);
		  }
    break;

  case 92:
/* Line 1792 of yacc.c  */
#line 1484 "parse.y"
    {
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", (yyvsp[(3) - (3)].id),
			  NULL);
		  }
    break;

  case 93:
/* Line 1792 of yacc.c  */
#line 1490 "parse.y"
    {
		ensure_property ();
		if (strcmp ((yyvsp[(1) - (1)].id), "override") == 0) {
			g_free((yyvsp[(1) - (1)].id));
			node_set ((Node *)property,
				  "override", TRUE,
				  NULL);
		} else if (strcmp ((yyvsp[(1) - (1)].id), "link") == 0) {
			g_free((yyvsp[(1) - (1)].id));
			node_set ((Node *)property,
				  "link", TRUE,
				  NULL);
		} else if (strcmp ((yyvsp[(1) - (1)].id), "export") == 0) {
			g_free((yyvsp[(1) - (1)].id));
			node_set ((Node *)property,
				  "export", TRUE,
				  NULL);
		} else {
			g_free((yyvsp[(1) - (1)].id));
			yyerror(_("parse error"));
			YYERROR;
		}
		  }
    break;

  case 94:
/* Line 1792 of yacc.c  */
#line 1515 "parse.y"
    {
			if(strcmp((yyvsp[(3) - (5)].id),"type")!=0) {
				g_free((yyvsp[(1) - (5)].id));
				g_free((yyvsp[(3) - (5)].id));
				yyerror(_("parse error"));
				YYERROR;
			}
			(yyval.id) = debool ((yyvsp[(1) - (5)].id));
						}
    break;

  case 95:
/* Line 1792 of yacc.c  */
#line 1524 "parse.y"
    {
			(yyval.id) = debool ((yyvsp[(1) - (1)].id));
			typestack = g_list_prepend(typestack,NULL);
						}
    break;

  case 96:
/* Line 1792 of yacc.c  */
#line 1530 "parse.y"
    { (yyval.list) = (yyvsp[(2) - (3)].list); }
    break;

  case 97:
/* Line 1792 of yacc.c  */
#line 1531 "parse.y"
    { (yyval.list) = NULL; }
    break;

  case 98:
/* Line 1792 of yacc.c  */
#line 1534 "parse.y"
    {
			(yyval.list) = g_list_append((yyvsp[(3) - (3)].list),(yyvsp[(1) - (3)].id));
						}
    break;

  case 99:
/* Line 1792 of yacc.c  */
#line 1537 "parse.y"
    {
			(yyval.list) = g_list_append(NULL,(yyvsp[(1) - (1)].id));
						}
    break;

  case 100:
/* Line 1792 of yacc.c  */
#line 1543 "parse.y"
    {
			Node *node = node_new (TYPE_NODE, 
					       "name:steal", (yyvsp[(1) - (2)].id),
					       "pointer:steal", (yyvsp[(2) - (2)].id),
					       NULL);
			typestack = g_list_prepend(typestack,node);
							}
    break;

  case 101:
/* Line 1792 of yacc.c  */
#line 1550 "parse.y"
    {
			Node *node = node_new (TYPE_NODE, 
					       "name:steal", (yyvsp[(1) - (1)].id),
					       NULL);
			typestack = g_list_prepend(typestack,node);
							}
    break;

  case 102:
/* Line 1792 of yacc.c  */
#line 1559 "parse.y"
    {
			(yyval.id) = (yyvsp[(1) - (1)].id);
							}
    break;

  case 103:
/* Line 1792 of yacc.c  */
#line 1562 "parse.y"
    {
			(yyval.id) = (yyvsp[(1) - (1)].id);
							}
    break;

  case 104:
/* Line 1792 of yacc.c  */
#line 1565 "parse.y"
    {
			(yyval.id) = g_strconcat("const ", (yyvsp[(2) - (2)].id), NULL);
			g_free((yyvsp[(2) - (2)].id));
							}
    break;

  case 105:
/* Line 1792 of yacc.c  */
#line 1569 "parse.y"
    {
			(yyval.id) = g_strconcat((yyvsp[(1) - (2)].id), " const", NULL);
			g_free((yyvsp[(1) - (2)].id));
							}
    break;

  case 106:
/* Line 1792 of yacc.c  */
#line 1573 "parse.y"
    {
			(yyval.id) = g_strconcat((yyvsp[(1) - (2)].id), " ", (yyvsp[(2) - (2)].id), NULL);
			g_free((yyvsp[(2) - (2)].id));
							}
    break;

  case 107:
/* Line 1792 of yacc.c  */
#line 1577 "parse.y"
    {
			(yyval.id) = g_strconcat("const ", (yyvsp[(2) - (3)].id), " ",
					     (yyvsp[(3) - (3)].id), NULL);
			g_free((yyvsp[(3) - (3)].id));
							}
    break;

  case 108:
/* Line 1792 of yacc.c  */
#line 1582 "parse.y"
    {
			(yyval.id) = g_strconcat((yyvsp[(1) - (3)].id), " ",
					     (yyvsp[(2) - (3)].id), " const", NULL);
			g_free((yyvsp[(2) - (3)].id));
							}
    break;

  case 109:
/* Line 1792 of yacc.c  */
#line 1590 "parse.y"
    {
			(yyval.id) = g_strconcat((yyvsp[(1) - (2)].id), " ", (yyvsp[(2) - (2)].id), NULL);
			g_free((yyvsp[(2) - (2)].id));
							}
    break;

  case 110:
/* Line 1792 of yacc.c  */
#line 1594 "parse.y"
    {
			(yyval.id) = g_strconcat((yyvsp[(1) - (2)].id), " ", (yyvsp[(2) - (2)].id), NULL);
			g_free((yyvsp[(1) - (2)].id));
			g_free((yyvsp[(2) - (2)].id));
							}
    break;

  case 111:
/* Line 1792 of yacc.c  */
#line 1599 "parse.y"
    {
			(yyval.id) = g_strconcat("const ", (yyvsp[(2) - (2)].id), NULL);
			g_free((yyvsp[(2) - (2)].id));
							}
    break;

  case 112:
/* Line 1792 of yacc.c  */
#line 1603 "parse.y"
    {
			(yyval.id) = (yyvsp[(1) - (1)].id);
							}
    break;

  case 113:
/* Line 1792 of yacc.c  */
#line 1606 "parse.y"
    {
			(yyval.id) = g_strconcat((yyvsp[(1) - (2)].id), " const", NULL);
			g_free((yyvsp[(1) - (2)].id));
							}
    break;

  case 114:
/* Line 1792 of yacc.c  */
#line 1610 "parse.y"
    {
			(yyval.id) = g_strdup((yyvsp[(1) - (1)].id));
							}
    break;

  case 115:
/* Line 1792 of yacc.c  */
#line 1613 "parse.y"
    {
			(yyval.id) = g_strconcat((yyvsp[(1) - (2)].id), " const", NULL);
							}
    break;

  case 116:
/* Line 1792 of yacc.c  */
#line 1618 "parse.y"
    { (yyval.id) = "void"; }
    break;

  case 117:
/* Line 1792 of yacc.c  */
#line 1619 "parse.y"
    { (yyval.id) = "char"; }
    break;

  case 118:
/* Line 1792 of yacc.c  */
#line 1620 "parse.y"
    { (yyval.id) = "short"; }
    break;

  case 119:
/* Line 1792 of yacc.c  */
#line 1621 "parse.y"
    { (yyval.id) = "int"; }
    break;

  case 120:
/* Line 1792 of yacc.c  */
#line 1622 "parse.y"
    { (yyval.id) = "long"; }
    break;

  case 121:
/* Line 1792 of yacc.c  */
#line 1623 "parse.y"
    { (yyval.id) = "float"; }
    break;

  case 122:
/* Line 1792 of yacc.c  */
#line 1624 "parse.y"
    { (yyval.id) = "double"; }
    break;

  case 123:
/* Line 1792 of yacc.c  */
#line 1625 "parse.y"
    { (yyval.id) = "signed"; }
    break;

  case 124:
/* Line 1792 of yacc.c  */
#line 1626 "parse.y"
    { (yyval.id) = "unsigned"; }
    break;

  case 125:
/* Line 1792 of yacc.c  */
#line 1629 "parse.y"
    { (yyval.id) = "struct"; }
    break;

  case 126:
/* Line 1792 of yacc.c  */
#line 1630 "parse.y"
    { (yyval.id) = "union"; }
    break;

  case 127:
/* Line 1792 of yacc.c  */
#line 1631 "parse.y"
    { (yyval.id) = "enum"; }
    break;

  case 128:
/* Line 1792 of yacc.c  */
#line 1634 "parse.y"
    { (yyval.id) = g_strdup("*"); }
    break;

  case 129:
/* Line 1792 of yacc.c  */
#line 1635 "parse.y"
    { (yyval.id) = g_strdup("* const"); }
    break;

  case 130:
/* Line 1792 of yacc.c  */
#line 1636 "parse.y"
    {
				(yyval.id) = g_strconcat("*", (yyvsp[(2) - (2)].id), NULL);
				g_free((yyvsp[(2) - (2)].id));
					}
    break;

  case 131:
/* Line 1792 of yacc.c  */
#line 1640 "parse.y"
    {
				(yyval.id) = g_strconcat("* const", (yyvsp[(3) - (3)].id), NULL);
				g_free((yyvsp[(3) - (3)].id));
					}
    break;

  case 132:
/* Line 1792 of yacc.c  */
#line 1647 "parse.y"
    {
			if(strcmp((yyvsp[(1) - (2)].id), "first")==0)
				(yyval.sigtype) = SIGNAL_FIRST_METHOD;
			else if(strcmp((yyvsp[(1) - (2)].id), "last")==0)
				(yyval.sigtype) = SIGNAL_LAST_METHOD;
			else {
				yyerror(_("signal must be 'first' or 'last'"));
				g_free((yyvsp[(1) - (2)].id));
				YYERROR;
			}
			g_free((yyvsp[(1) - (2)].id));
					}
    break;

  case 133:
/* Line 1792 of yacc.c  */
#line 1659 "parse.y"
    {
			(yyval.sigtype) = SIGNAL_LAST_METHOD;
					}
    break;

  case 134:
/* Line 1792 of yacc.c  */
#line 1665 "parse.y"
    {
			if(strcmp((yyvsp[(2) - (3)].id),"first")==0)
				(yyval.sigtype) = SIGNAL_FIRST_METHOD;
			else if(strcmp((yyvsp[(2) - (3)].id),"last")==0)
				(yyval.sigtype) = SIGNAL_LAST_METHOD;
			else {
				yyerror(_("signal must be 'first' or 'last'"));
				g_free((yyvsp[(2) - (3)].id));
				YYERROR;
			}
			g_free((yyvsp[(2) - (3)].id));
					}
    break;

  case 135:
/* Line 1792 of yacc.c  */
#line 1677 "parse.y"
    {
			if(strcmp((yyvsp[(1) - (3)].id),"first")==0)
				(yyval.sigtype) = SIGNAL_FIRST_METHOD;
			else if(strcmp((yyvsp[(1) - (3)].id),"last")==0)
				(yyval.sigtype) = SIGNAL_LAST_METHOD;
			else {
				yyerror(_("signal must be 'first' or 'last'"));
				g_free((yyvsp[(1) - (3)].id));
				YYERROR;
			}
			g_free((yyvsp[(1) - (3)].id));
					}
    break;

  case 136:
/* Line 1792 of yacc.c  */
#line 1689 "parse.y"
    {
			(yyval.sigtype) = SIGNAL_LAST_METHOD;
					}
    break;

  case 137:
/* Line 1792 of yacc.c  */
#line 1692 "parse.y"
    {
			/* the_scope was default thus public */
			the_scope = PUBLIC_SCOPE;
					}
    break;

  case 138:
/* Line 1792 of yacc.c  */
#line 1698 "parse.y"
    {
			gtktypes = g_list_prepend(gtktypes, debool ((yyvsp[(1) - (4)].id)));
						}
    break;

  case 139:
/* Line 1792 of yacc.c  */
#line 1701 "parse.y"
    {
			gtktypes = g_list_prepend(gtktypes, debool ((yyvsp[(1) - (5)].id)));
			signal_name=(yyvsp[(2) - (5)].id);
						}
    break;

  case 140:
/* Line 1792 of yacc.c  */
#line 1707 "parse.y"
    {
			gtktypes = g_list_append(gtktypes, debool ((yyvsp[(3) - (3)].id)));
						}
    break;

  case 141:
/* Line 1792 of yacc.c  */
#line 1710 "parse.y"
    { 
			gtktypes = g_list_append(gtktypes, debool ((yyvsp[(1) - (1)].id)));
       						}
    break;

  case 142:
/* Line 1792 of yacc.c  */
#line 1715 "parse.y"
    { (yyval.cbuf) = (yyvsp[(2) - (2)].cbuf); }
    break;

  case 143:
/* Line 1792 of yacc.c  */
#line 1716 "parse.y"
    { (yyval.cbuf) = NULL; }
    break;

  case 144:
/* Line 1792 of yacc.c  */
#line 1720 "parse.y"
    {
			if(!has_self) {
				yyerror(_("signal without 'self' as "
					  "first parameter"));
				free_all_global_state();
				YYERROR;
			}
			if(the_scope == CLASS_SCOPE) {
				yyerror(_("a method cannot be of class scope"));
				free_all_global_state();
				YYERROR;
			}
	                if (funcattrs != NULL) {
				char *error = g_strdup_printf
					(_("function attribute macros ('%s' in this case) may not be used with signal methods"),
					 funcattrs);
	                        yyerror (error);
		                YYERROR;
		        }
			push_function(the_scope, (yyvsp[(3) - (10)].sigtype),NULL,
				      (yyvsp[(5) - (10)].id), (yyvsp[(10) - (10)].cbuf),(yyvsp[(1) - (10)].line),
				      ccode_line, vararg, (yyvsp[(2) - (10)].list));
									}
    break;

  case 145:
/* Line 1792 of yacc.c  */
#line 1743 "parse.y"
    {
			if(!has_self) {
				yyerror(_("signal without 'self' as "
					  "first parameter"));
				free_all_global_state();
				YYERROR;
			}
			if(the_scope == CLASS_SCOPE) {
				yyerror(_("a method cannot be of class scope"));
				free_all_global_state();
				YYERROR;
			}
	                if (funcattrs != NULL) {
				char *error = g_strdup_printf
					(_("function attribute macros ('%s' in this case) may not be used with signal methods"),
					 funcattrs);
	                        yyerror (error);
		                YYERROR;
		        }
			push_function(the_scope, (yyvsp[(4) - (11)].sigtype), NULL,
				      (yyvsp[(6) - (11)].id), (yyvsp[(11) - (11)].cbuf), (yyvsp[(2) - (11)].line),
				      ccode_line, vararg, (yyvsp[(3) - (11)].list));
									}
    break;

  case 146:
/* Line 1792 of yacc.c  */
#line 1766 "parse.y"
    {
			if(!has_self) {
				yyerror(_("virtual method without 'self' as "
					  "first parameter"));
				free_all_global_state();
				YYERROR;
			}
			if(the_scope == CLASS_SCOPE) {
				yyerror(_("a method cannot be of class scope"));
				free_all_global_state();
				YYERROR;
			}
	                if (funcattrs != NULL) {
				char *error = g_strdup_printf
					(_("function attribute macros ('%s' in this case) may not be used with virtual methods"),
					 funcattrs);
	                        yyerror (error);
		                YYERROR;
		        }
			push_function(the_scope, VIRTUAL_METHOD, NULL, (yyvsp[(4) - (9)].id),
				      (yyvsp[(9) - (9)].cbuf), (yyvsp[(1) - (9)].line),
				      ccode_line, vararg, NULL);
									}
    break;

  case 147:
/* Line 1792 of yacc.c  */
#line 1789 "parse.y"
    {
			if(!has_self) {
				yyerror(_("virtual method without 'self' as "
					  "first parameter"));
				free_all_global_state();
				YYERROR;
			}
			if(the_scope == CLASS_SCOPE) {
				yyerror(_("a method cannot be of class scope"));
				free_all_global_state();
				YYERROR;
			}
	                if (funcattrs != NULL) {
				char *error = g_strdup_printf
					(_("function attribute macros ('%s' in this case) may not be used with virtual methods"),
					 funcattrs);
	                        yyerror (error);
		                YYERROR;
		        }
			push_function(the_scope, VIRTUAL_METHOD, NULL, (yyvsp[(4) - (9)].id),
				      (yyvsp[(9) - (9)].cbuf), (yyvsp[(2) - (9)].line),
				      ccode_line, vararg, NULL);
									}
    break;

  case 148:
/* Line 1792 of yacc.c  */
#line 1812 "parse.y"
    {
			if(!has_self) {
				yyerror(_("virtual method without 'szelf' as "
					  "first parameter"));
				free_all_global_state();
				YYERROR;
			}
	                if (funcattrs != NULL) {
				char *error = g_strdup_printf
					(_("function attribute macros ('%s' in this case) may not be used with virtual methods"),
					 funcattrs);
	                        yyerror (error);
		                YYERROR;
		        }
			push_function(PUBLIC_SCOPE, VIRTUAL_METHOD, NULL,
				      (yyvsp[(3) - (8)].id), (yyvsp[(8) - (8)].cbuf), (yyvsp[(1) - (8)].line),
				      ccode_line, vararg, NULL);
									}
    break;

  case 149:
/* Line 1792 of yacc.c  */
#line 1830 "parse.y"
    {
	                if (funcattrs != NULL) {
				char *error = g_strdup_printf
					(_("function attribute macros ('%s' in this case) may not be used with override methods"),
					 funcattrs);
	                        yyerror (error);
		                YYERROR;
		        }
			push_function(NO_SCOPE, OVERRIDE_METHOD, (yyvsp[(3) - (11)].id),
				      (yyvsp[(6) - (11)].id), (yyvsp[(11) - (11)].cbuf),
				      (yyvsp[(1) - (11)].line), ccode_line,
				      vararg, NULL);
									}
    break;

  case 150:
/* Line 1792 of yacc.c  */
#line 1843 "parse.y"
    {
			if(the_scope == CLASS_SCOPE) {
				yyerror(_("a method cannot be of class scope"));
				free_all_global_state();
				YYERROR;
			}
			push_function(the_scope, REGULAR_METHOD, NULL, (yyvsp[(3) - (8)].id),
				      (yyvsp[(8) - (8)].cbuf), (yyvsp[(1) - (8)].line), ccode_line,
				      vararg, NULL);
								}
    break;

  case 151:
/* Line 1792 of yacc.c  */
#line 1853 "parse.y"
    {
			if(strcmp((yyvsp[(1) - (5)].id), "init")==0) {
				push_init_arg((yyvsp[(3) - (5)].id),FALSE);
				push_function(NO_SCOPE, INIT_METHOD, NULL,
					      (yyvsp[(1) - (5)].id), (yyvsp[(5) - (5)].cbuf), (yyvsp[(2) - (5)].line),
					      ccode_line, FALSE, NULL);
			} else if(strcmp((yyvsp[(1) - (5)].id), "class_init")==0) {
				push_init_arg((yyvsp[(3) - (5)].id),TRUE);
				push_function(NO_SCOPE, CLASS_INIT_METHOD, NULL,
					      (yyvsp[(1) - (5)].id), (yyvsp[(5) - (5)].cbuf), (yyvsp[(2) - (5)].line),
					      ccode_line, FALSE, NULL);
			} else if(strcmp((yyvsp[(1) - (5)].id), "constructor")==0) {
				push_init_arg((yyvsp[(3) - (5)].id), FALSE);
				push_function(NO_SCOPE, CONSTRUCTOR_METHOD, NULL,
					      (yyvsp[(1) - (5)].id), (yyvsp[(5) - (5)].cbuf), (yyvsp[(2) - (5)].line),
					      ccode_line, FALSE, NULL);
			} else if(strcmp((yyvsp[(1) - (5)].id), "dispose")==0) {
				push_init_arg((yyvsp[(3) - (5)].id), FALSE);
				push_function(NO_SCOPE, DISPOSE_METHOD, NULL,
					      (yyvsp[(1) - (5)].id), (yyvsp[(5) - (5)].cbuf), (yyvsp[(2) - (5)].line),
					      ccode_line, FALSE, NULL);
			} else if(strcmp((yyvsp[(1) - (5)].id), "finalize")==0) {
				push_init_arg((yyvsp[(3) - (5)].id), FALSE);
				push_function(NO_SCOPE, FINALIZE_METHOD, NULL,
					      (yyvsp[(1) - (5)].id), (yyvsp[(5) - (5)].cbuf), (yyvsp[(2) - (5)].line),
					      ccode_line, FALSE, NULL);

			} else {
				g_free((yyvsp[(1) - (5)].id));
				g_free((yyvsp[(3) - (5)].id));
				g_string_free((yyvsp[(5) - (5)].cbuf),TRUE);
				yyerror(_("parse error "
					  "(untyped blocks must be init, "
					  "class_init, constructor, dispose "
					  "or finalize)"));
				YYERROR;
			}
						}
    break;

  case 152:
/* Line 1792 of yacc.c  */
#line 1893 "parse.y"
    {
			g_free(funcattrs); funcattrs = NULL;
			g_free(onerror); onerror = NULL;
			g_free(defreturn); defreturn = NULL;
			if(!set_attr_value((yyvsp[(1) - (2)].id), (yyvsp[(2) - (2)].id))) {
				g_free((yyvsp[(1) - (2)].id));
				g_free((yyvsp[(2) - (2)].id));
				yyerror(_("parse error"));
				YYERROR;
			}
			g_free((yyvsp[(1) - (2)].id));
					}
    break;

  case 153:
/* Line 1792 of yacc.c  */
#line 1905 "parse.y"
    {
			g_free(funcattrs); funcattrs = NULL;
			g_free(onerror); onerror = NULL;
			g_free(defreturn); defreturn = NULL;
			if(!set_attr_value((yyvsp[(1) - (4)].id), (yyvsp[(2) - (4)].id))) {
				g_free((yyvsp[(1) - (4)].id)); g_free((yyvsp[(2) - (4)].id));
				g_free((yyvsp[(3) - (4)].id)); g_free((yyvsp[(4) - (4)].id));
				yyerror(_("parse error"));
				YYERROR;
			}
			if(!set_attr_value((yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].id))) {
				funcattrs = onerror = defreturn = NULL;
				g_free((yyvsp[(1) - (4)].id)); g_free((yyvsp[(2) - (4)].id));
				g_free((yyvsp[(3) - (4)].id)); g_free((yyvsp[(4) - (4)].id));
				yyerror(_("parse error"));
				YYERROR;
			}
			g_free((yyvsp[(1) - (4)].id));
			g_free((yyvsp[(3) - (4)].id));
						}
    break;

  case 154:
/* Line 1792 of yacc.c  */
#line 1925 "parse.y"
    {
			g_free(funcattrs); funcattrs = NULL;
			g_free(onerror); onerror = NULL;
			g_free(defreturn); defreturn = NULL;
			if(!set_attr_value((yyvsp[(1) - (6)].id), (yyvsp[(2) - (6)].id))) {
				g_free((yyvsp[(1) - (6)].id)); g_free((yyvsp[(2) - (6)].id));
				g_free((yyvsp[(3) - (6)].id)); g_free((yyvsp[(4) - (6)].id));
				g_free((yyvsp[(5) - (6)].id)); g_free((yyvsp[(6) - (6)].id));
				yyerror(_("parse error"));
				YYERROR;
			}
			if(!set_attr_value((yyvsp[(3) - (6)].id), (yyvsp[(4) - (6)].id))) {
				funcattrs = onerror = defreturn = NULL;
				g_free((yyvsp[(1) - (6)].id)); g_free((yyvsp[(2) - (6)].id));
				g_free((yyvsp[(3) - (6)].id)); g_free((yyvsp[(4) - (6)].id));
				g_free((yyvsp[(5) - (6)].id)); g_free((yyvsp[(6) - (6)].id));
				yyerror(_("parse error"));
				YYERROR;
			}
			if(!set_attr_value((yyvsp[(5) - (6)].id), (yyvsp[(6) - (6)].id))) {
				funcattrs = onerror = defreturn = NULL;
				g_free((yyvsp[(1) - (6)].id)); g_free((yyvsp[(2) - (6)].id));
				g_free((yyvsp[(3) - (6)].id)); g_free((yyvsp[(4) - (6)].id));
				g_free((yyvsp[(5) - (6)].id)); g_free((yyvsp[(6) - (6)].id));
				yyerror(_("parse error"));
				YYERROR;
			}
			g_free((yyvsp[(1) - (6)].id));
			g_free((yyvsp[(3) - (6)].id));
			g_free((yyvsp[(5) - (6)].id));
						}
    break;

  case 155:
/* Line 1792 of yacc.c  */
#line 1956 "parse.y"
    {
			g_free(funcattrs); funcattrs = NULL;
			g_free(onerror); onerror = NULL;
			g_free(defreturn); defreturn = NULL;
					}
    break;

  case 156:
/* Line 1792 of yacc.c  */
#line 1963 "parse.y"
    { (yyval.id) = (yyvsp[(1) - (1)].id); }
    break;

  case 157:
/* Line 1792 of yacc.c  */
#line 1964 "parse.y"
    {
			(yyval.id) = ((yyvsp[(2) - (2)].cbuf))->str;
			g_string_free((yyvsp[(2) - (2)].cbuf), FALSE);
					}
    break;

  case 158:
/* Line 1792 of yacc.c  */
#line 1970 "parse.y"
    { vararg = FALSE; has_self = FALSE; }
    break;

  case 159:
/* Line 1792 of yacc.c  */
#line 1971 "parse.y"
    {
			vararg = FALSE;
			has_self = TRUE;
			if(strcmp((yyvsp[(1) - (1)].id),"self")==0)
				push_self((yyvsp[(1) - (1)].id), FALSE);
			else {
				g_free((yyvsp[(1) - (1)].id));
				yyerror(_("parse error"));
				YYERROR;
			}
       					}
    break;

  case 160:
/* Line 1792 of yacc.c  */
#line 1982 "parse.y"
    {
			vararg = FALSE;
			has_self = TRUE;
			if(strcmp((yyvsp[(1) - (2)].id),"self")==0)
				push_self((yyvsp[(1) - (2)].id), TRUE);
			else {
				g_free((yyvsp[(1) - (2)].id));
				yyerror(_("parse error"));
				YYERROR;
			}
       					}
    break;

  case 161:
/* Line 1792 of yacc.c  */
#line 1993 "parse.y"
    {
			vararg = FALSE;
			has_self = TRUE;
			if(strcmp((yyvsp[(2) - (2)].id),"self")==0)
				push_self((yyvsp[(2) - (2)].id), TRUE);
			else {
				g_free((yyvsp[(2) - (2)].id));
				yyerror(_("parse error"));
				YYERROR;
			}
       					}
    break;

  case 162:
/* Line 1792 of yacc.c  */
#line 2004 "parse.y"
    {
			has_self = TRUE;
			if(strcmp((yyvsp[(1) - (3)].id),"self")==0)
				push_self((yyvsp[(1) - (3)].id), FALSE);
			else {
				g_free((yyvsp[(1) - (3)].id));
				yyerror(_("parse error"));
				YYERROR;
			}
					}
    break;

  case 163:
/* Line 1792 of yacc.c  */
#line 2014 "parse.y"
    {
			has_self = TRUE;
			if(strcmp((yyvsp[(1) - (4)].id),"self")==0)
				push_self((yyvsp[(1) - (4)].id), TRUE);
			else {
				g_free((yyvsp[(1) - (4)].id));
				yyerror(_("parse error"));
				YYERROR;
			}
					}
    break;

  case 164:
/* Line 1792 of yacc.c  */
#line 2024 "parse.y"
    {
			has_self = TRUE;
			if(strcmp((yyvsp[(2) - (4)].id),"self")==0)
				push_self((yyvsp[(2) - (4)].id), TRUE);
			else {
				g_free((yyvsp[(2) - (4)].id));
				yyerror(_("parse error"));
				YYERROR;
			}
					}
    break;

  case 165:
/* Line 1792 of yacc.c  */
#line 2034 "parse.y"
    { has_self = FALSE; }
    break;

  case 166:
/* Line 1792 of yacc.c  */
#line 2037 "parse.y"
    { vararg = TRUE; }
    break;

  case 167:
/* Line 1792 of yacc.c  */
#line 2038 "parse.y"
    { vararg = FALSE; }
    break;

  case 168:
/* Line 1792 of yacc.c  */
#line 2041 "parse.y"
    { ; }
    break;

  case 169:
/* Line 1792 of yacc.c  */
#line 2042 "parse.y"
    { ; }
    break;

  case 170:
/* Line 1792 of yacc.c  */
#line 2045 "parse.y"
    {
			push_funcarg((yyvsp[(2) - (2)].id),NULL);
								}
    break;

  case 171:
/* Line 1792 of yacc.c  */
#line 2048 "parse.y"
    {
			push_funcarg((yyvsp[(2) - (3)].id),(yyvsp[(3) - (3)].id));
								}
    break;

  case 172:
/* Line 1792 of yacc.c  */
#line 2051 "parse.y"
    {
			if(strcmp((yyvsp[(4) - (6)].id),"check")!=0) {
				yyerror(_("parse error"));
				YYERROR;
			}
			g_free((yyvsp[(4) - (6)].id));
			push_funcarg((yyvsp[(2) - (6)].id),NULL);
								}
    break;

  case 173:
/* Line 1792 of yacc.c  */
#line 2059 "parse.y"
    {
			if(strcmp((yyvsp[(5) - (7)].id),"check")!=0) {
				yyerror(_("parse error"));
				YYERROR;
			}
			g_free((yyvsp[(5) - (7)].id));
			push_funcarg((yyvsp[(2) - (7)].id),(yyvsp[(3) - (7)].id));
								}
    break;

  case 174:
/* Line 1792 of yacc.c  */
#line 2069 "parse.y"
    { ; }
    break;

  case 175:
/* Line 1792 of yacc.c  */
#line 2070 "parse.y"
    { ; }
    break;

  case 176:
/* Line 1792 of yacc.c  */
#line 2073 "parse.y"
    {
			if(strcmp((yyvsp[(1) - (1)].id),"type")==0) {
				Node *node = node_new (CHECK_NODE,
						       "chtype", TYPE_CHECK,
						       NULL);
				checks = g_list_append(checks,node);
			} else if(strcmp((yyvsp[(1) - (1)].id),"null")==0) {
				Node *node = node_new (CHECK_NODE,
						       "chtype", NULL_CHECK,
						       NULL);
				checks = g_list_append(checks,node);
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
			g_free((yyvsp[(1) - (1)].id));
					}
    break;

  case 177:
/* Line 1792 of yacc.c  */
#line 2090 "parse.y"
    {
			Node *node = node_new (CHECK_NODE,
					       "chtype", GT_CHECK,
					       "number:steal", (yyvsp[(2) - (2)].id),
					       NULL);
			checks = g_list_append(checks,node);
					}
    break;

  case 178:
/* Line 1792 of yacc.c  */
#line 2097 "parse.y"
    {
			Node *node = node_new (CHECK_NODE,
					       "chtype", LT_CHECK,
					       "number:steal", (yyvsp[(2) - (2)].id),
					       NULL);
			checks = g_list_append(checks,node);
					}
    break;

  case 179:
/* Line 1792 of yacc.c  */
#line 2104 "parse.y"
    {
			Node *node = node_new (CHECK_NODE,
					       "chtype", GE_CHECK,
					       "number:steal", (yyvsp[(3) - (3)].id),
					       NULL);
			checks = g_list_append(checks,node);
					}
    break;

  case 180:
/* Line 1792 of yacc.c  */
#line 2111 "parse.y"
    {
			Node *node = node_new (CHECK_NODE,
					       "chtype", LE_CHECK,
					       "number:steal", (yyvsp[(3) - (3)].id),
					       NULL);
			checks = g_list_append(checks,node);
					}
    break;

  case 181:
/* Line 1792 of yacc.c  */
#line 2118 "parse.y"
    {
			Node *node = node_new (CHECK_NODE,
					       "chtype", EQ_CHECK,
					       "number:steal", (yyvsp[(3) - (3)].id),
					       NULL);
			checks = g_list_append(checks,node);
					}
    break;

  case 182:
/* Line 1792 of yacc.c  */
#line 2125 "parse.y"
    {
			Node *node = node_new (CHECK_NODE,
					       "chtype", NE_CHECK,
					       "number:steal", (yyvsp[(3) - (3)].id),
					       NULL);
			checks = g_list_append(checks,node);
					}
    break;

  case 183:
/* Line 1792 of yacc.c  */
#line 2134 "parse.y"
    {
			Node *node = node_new (ENUMDEF_NODE,
					       "etype:steal", (yyvsp[(6) - (7)].id),
					       "prefix:steal", (yyvsp[(2) - (7)].id),
					       "values:steal", enum_vals,
					       NULL);
			enum_vals = NULL;
			nodes = g_list_append (nodes, node);
			}
    break;

  case 184:
/* Line 1792 of yacc.c  */
#line 2143 "parse.y"
    {
			Node *node = node_new (ENUMDEF_NODE,
					       "etype:steal", (yyvsp[(7) - (8)].id),
					       "prefix:steal", (yyvsp[(2) - (8)].id),
					       "values:steal", enum_vals,
					       NULL);
			enum_vals = NULL;
			nodes = g_list_append (nodes, node);
			}
    break;

  case 185:
/* Line 1792 of yacc.c  */
#line 2154 "parse.y"
    {;}
    break;

  case 186:
/* Line 1792 of yacc.c  */
#line 2155 "parse.y"
    {;}
    break;

  case 187:
/* Line 1792 of yacc.c  */
#line 2158 "parse.y"
    {
			Node *node;
			char *num = (yyvsp[(3) - (3)].id);

			/* A float value, that's a bad enum */
			if (num[0] >= '0' &&
			    num[0] <= '9' &&
			    strchr (num, '.') != NULL) {
				g_free ((yyvsp[(1) - (3)].id));
				g_free (num);
				yyerror(_("parse error (enumerator value not integer constant)"));
				YYERROR;
			}
		       
			node = node_new (ENUMVALUE_NODE,
					 "name:steal", (yyvsp[(1) - (3)].id),
					 "value:steal", num,
					 NULL);
			enum_vals = g_list_append (enum_vals, node);
			}
    break;

  case 188:
/* Line 1792 of yacc.c  */
#line 2178 "parse.y"
    {
			Node *node;

			node = node_new (ENUMVALUE_NODE,
					 "name:steal", (yyvsp[(1) - (1)].id),
					 NULL);
			enum_vals = g_list_append (enum_vals, node);
	}
    break;

  case 189:
/* Line 1792 of yacc.c  */
#line 2188 "parse.y"
    {
			Node *node = node_new (FLAGS_NODE,
					       "ftype:steal", (yyvsp[(6) - (7)].id),
					       "prefix:steal", (yyvsp[(2) - (7)].id),
					       "values:steal", flag_vals,
					       NULL);
			flag_vals = NULL;
			nodes = g_list_append (nodes, node);
			}
    break;

  case 190:
/* Line 1792 of yacc.c  */
#line 2197 "parse.y"
    {
			Node *node = node_new (FLAGS_NODE,
					       "ftype:steal", (yyvsp[(7) - (8)].id),
					       "prefix:steal", (yyvsp[(2) - (8)].id),
					       "values:steal", flag_vals,
					       NULL);
			flag_vals = NULL;
			nodes = g_list_append (nodes, node);
			}
    break;

  case 191:
/* Line 1792 of yacc.c  */
#line 2208 "parse.y"
    {
			flag_vals = g_list_append (flag_vals, (yyvsp[(3) - (3)].id));
		}
    break;

  case 192:
/* Line 1792 of yacc.c  */
#line 2211 "parse.y"
    {
			flag_vals = g_list_append (flag_vals, (yyvsp[(1) - (1)].id));
		}
    break;

  case 193:
/* Line 1792 of yacc.c  */
#line 2216 "parse.y"
    {
			Node *node = node_new (ERROR_NODE,
					       "etype:steal", (yyvsp[(6) - (7)].id),
					       "prefix:steal", (yyvsp[(2) - (7)].id),
					       "values:steal", error_vals,
					       NULL);
			error_vals = NULL;
			nodes = g_list_append (nodes, node);
			}
    break;

  case 194:
/* Line 1792 of yacc.c  */
#line 2225 "parse.y"
    {
			Node *node = node_new (ERROR_NODE,
					       "etype:steal", (yyvsp[(7) - (8)].id),
					       "prefix:steal", (yyvsp[(2) - (8)].id),
					       "values:steal", error_vals,
					       NULL);
			error_vals = NULL;
			nodes = g_list_append (nodes, node);
			}
    break;

  case 195:
/* Line 1792 of yacc.c  */
#line 2236 "parse.y"
    {
			error_vals = g_list_append (error_vals, (yyvsp[(3) - (3)].id));
		}
    break;

  case 196:
/* Line 1792 of yacc.c  */
#line 2239 "parse.y"
    {
			error_vals = g_list_append (error_vals, (yyvsp[(1) - (1)].id));
		}
    break;

  case 197:
/* Line 1792 of yacc.c  */
#line 2245 "parse.y"
    { (yyval.id) = (yyvsp[(1) - (1)].id); }
    break;

  case 198:
/* Line 1792 of yacc.c  */
#line 2246 "parse.y"
    {
			(yyval.id) = g_strconcat("-",(yyvsp[(2) - (2)].id),NULL);
			g_free((yyvsp[(2) - (2)].id));
					}
    break;

  case 199:
/* Line 1792 of yacc.c  */
#line 2250 "parse.y"
    { (yyval.id) = (yyvsp[(1) - (1)].id); }
    break;

  case 200:
/* Line 1792 of yacc.c  */
#line 2251 "parse.y"
    { (yyval.id) = (yyvsp[(1) - (1)].id); }
    break;


/* Line 1792 of yacc.c  */
#line 4964 "parse.c"
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


/* Line 2055 of yacc.c  */
#line 2254 "parse.y"

