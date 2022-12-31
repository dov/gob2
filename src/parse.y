/* GOB C Preprocessor
 * Copyright (C) 1999-2000 the Free Software Foundation.
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2001-2009 George (Jiri) Lebl
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

%}

%union {
	char *id;
	GString *cbuf;
	GList *list;
	int line;
	int sigtype;
}

%token CLASS FROM
%token CONST VOID STRUCT UNION ENUM THREEDOTS
%token SIGNED UNSIGNED LONG SHORT INT FLOAT DOUBLE CHAR

%token <id> TOKEN NUMBER TYPETOKEN ARRAY_DIM SINGLE_CHAR
%token <cbuf> CCODE CTCODE ADCODE HTCODE PHCODE HCODE ACODE ATCODE STRING
%token <line> PUBLIC PRIVATE PROTECTED CLASSWIDE PROPERTY ARGUMENT
%token <line> VIRTUAL SIGNAL OVERRIDE
%token <line> NAME NICK BLURB MAXIMUM MINIMUM DEFAULT_VALUE ERROR FLAGS TYPE
%token <line> FLAGS_TYPE ENUM_TYPE PARAM_TYPE BOXED_TYPE OBJECT_TYPE

%%

prog:		ccodes class ccodes	{ ; }
	|	class ccodes		{ ; }
	|	ccodes class		{ ; }
	|	class			{ ; }
	;

ccode:		CCODE			{
			Node *node = node_new (CCODE_NODE,
					       "cctype", C_CCODE,
					       "cbuf:steal", ($<cbuf>1)->str,
					       "line_no", ccode_line,
					       NULL);
			nodes = g_list_append(nodes,node);
			g_string_free($<cbuf>1,FALSE);
					}
	|	ADCODE			{
			Node *node = node_new (CCODE_NODE,
					       "cctype", AD_CCODE,
					       "cbuf:steal", ($<cbuf>1)->str,
					       "line_no", ccode_line,
					       NULL);
			nodes = g_list_append(nodes,node);
			g_string_free($<cbuf>1,FALSE);
					}
	|	HCODE			{
			Node *node = node_new (CCODE_NODE,
					       "cctype", H_CCODE,
					       "cbuf:steal", ($<cbuf>1)->str,
					       "line_no", ccode_line,
					       NULL);
			nodes = g_list_append(nodes,node);
			g_string_free($<cbuf>1,FALSE);
					}
	|	HTCODE			{
			Node *node = node_new (CCODE_NODE,
					       "cctype", HT_CCODE,
					       "cbuf:steal", ($<cbuf>1)->str,
					       "line_no", ccode_line,
					       NULL);
			nodes = g_list_append(nodes,node);
			g_string_free($<cbuf>1,FALSE);
					}
	|	PHCODE			{
			Node *node = node_new (CCODE_NODE,
					       "cctype", PH_CCODE,
					       "cbuf:steal", ($<cbuf>1)->str,
					       "line_no", ccode_line,
					       NULL);
			nodes = g_list_append(nodes,node);
			g_string_free($<cbuf>1,FALSE);
					}
	|	ACODE			{
			Node *node = node_new (CCODE_NODE,
					       "cctype", A_CCODE,
					       "cbuf:steal", ($<cbuf>1)->str,
					       "line_no", ccode_line,
					       NULL);
			nodes = g_list_append(nodes,node);
			g_string_free($<cbuf>1,FALSE);
					}
	|	ATCODE			{
			Node *node = node_new (CCODE_NODE,
					       "cctype", AT_CCODE,
					       "cbuf:steal", ($<cbuf>1)->str,
					       "line_no", ccode_line,
					       NULL);
			nodes = g_list_append(nodes,node);
			g_string_free($<cbuf>1,FALSE);
					}
	|	CTCODE			{
			Node *node = node_new (CCODE_NODE,
					       "cctype", CT_CCODE,
					       "cbuf:steal", ($<cbuf>1)->str,
					       "line_no", ccode_line,
					       NULL);
			nodes = g_list_append(nodes,node);
			g_string_free($<cbuf>1,FALSE);
					}
	;

ccodes:		ccodes ccode		{ ; }
	|	ccodes enumcode		{ ; }
	|	ccodes flagcode		{ ; }
	|	ccodes errorcode	{ ; }
	|	ccode			{ ; }
	|	enumcode		{ ; }
	|	flagcode		{ ; }
	|	errorcode		{ ; }
	;

class:		classdec '{' classcode '}'	{
			((Class *)class)->nodes = class_nodes;
			class_nodes = NULL;
			nodes = g_list_append(nodes,class);
						}
	|	classdec '{' '}'		{
			((Class *)class)->nodes = NULL;
			class_nodes = NULL;
			nodes = g_list_append(nodes,class);
						}
	;

classdec:	CLASS TYPETOKEN FROM TYPETOKEN	classflags {
			class = node_new (CLASS_NODE,
					  "otype:steal", $<id>2,
					  "ptype:steal", $<id>4,
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
	;

classflags:
	| '(' TOKEN ')' classflags {
			if(strcmp($<id>2,"abstract") == 0) {
				abstract = TRUE;
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
		}
	| '(' TOKEN TOKEN ')' classflags {
			if(strcmp($<id>2,"chunks") == 0) {
				g_free (chunk_size);
				chunk_size = g_strdup($<id>3);
			} else if(strcmp($<id>2,"BonoboObject") == 0) {
				g_free (bonobo_object_class);
				bonobo_object_class = g_strdup($<id>3);
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
		}
	| '(' TOKEN TYPETOKEN ')' classflags {
			if (strcmp ($<id>2, "interface") == 0) {
				interfaces = g_list_append (interfaces,
							    g_strdup ($<id>3));
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
		}
	| '(' TOKEN NUMBER ')' classflags {
			if(strcmp($<id>2,"chunks") == 0) {
				g_free (chunk_size);
				if(atoi($<id>3) != 0)
					chunk_size = g_strdup($<id>3);
				else
					chunk_size = NULL;
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
		}
	| '(' TOKEN STRING STRING ')' classflags {
			if (strcmp ($<id>2, "GladeXML") == 0) {
				glade_xml = TRUE;
				add_construct_glade($<id>3, $<id>4, NULL);
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
		}
	| '(' TOKEN STRING STRING STRING ')' classflags {
			if (strcmp ($<id>2, "GladeXML") == 0) {
				glade_xml = TRUE;
				add_construct_glade($<id>3, $<id>4, $<id>5);
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
		}
	| '(' TOKEN TOKEN STRING ')' classflags {
			if (strcmp ($<id>2, "GladeXML") == 0) {
				glade_xml = TRUE;
				add_construct_glade($<id>3, $<id>4, NULL);
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
		}
	| '(' TOKEN TOKEN STRING STRING ')' classflags {
			if (strcmp ($<id>2, "GladeXML") == 0) {
				glade_xml = TRUE;
				add_construct_glade($<id>3, $<id>4, $<id>5);
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
		}
	;	

classcode:	classcode thing			{ ; }
	|	thing				{ ; }
	;

thing: 		method				{ ; }
	|	TOKEN method			{
			if (strcmp ($<id>1, "BonoboObject") != 0) {
				g_free ($<id>1);
				yyerror (_("parse error"));
				YYERROR;
			}
			g_free ($<id>1);
			last_added_method->bonobo_object_func = TRUE;
						}
	|	TOKEN TYPETOKEN method			{
			if (strcmp ($<id>1, "interface") != 0) {
				g_free ($<id>1);
				g_free ($<id>2);
				yyerror (_("parse error"));
				YYERROR;
			}
			g_free ($<id>1);
			node_set ((Node *)last_added_method,
				  "interface:steal", $<id>2,
				  NULL);
						}
	|	variable			{ ; }
	|	argument			{ ; }
	|	property			{ ; }
	|	';'				{ ; }
	;

scope:		PUBLIC			{ the_scope = PUBLIC_SCOPE; }
	|	PRIVATE			{ the_scope = PRIVATE_SCOPE; }
	|	PROTECTED		{ the_scope = PROTECTED_SCOPE; }
	|	CLASSWIDE		{ the_scope = CLASS_SCOPE; }
	;

destructor: 	TOKEN TOKEN 	{
			if (strcmp ($<id>1, "destroywith") == 0) {
				g_free ($<id>1);
				destructor_unref = FALSE;
				destructor = $<id>2;
				destructor_line = line_no;
				destructor_simple = TRUE;
			} else if (strcmp ($<id>1, "unrefwith") == 0) {
				g_free ($<id>1);
				destructor_unref = TRUE;
				destructor = $<id>2;
				destructor_line = line_no;
				destructor_simple = TRUE;
			} else {
				g_free ($<id>1);
				g_free ($<id>2);
				yyerror (_("parse error"));
				YYERROR;
			}
				}
	|	TOKEN '{' CCODE		{
			if (strcmp ($<id>1, "destroy") == 0) {
				g_free($<id>1);
				destructor_unref = FALSE;
				destructor = ($<cbuf>3)->str;
				g_string_free($<cbuf>3, FALSE);
				destructor_line = ccode_line;
				destructor_simple = FALSE;
			} else if (strcmp ($<id>1, "unref") == 0) {
				g_free ($<id>1);
				destructor_unref = TRUE;
				destructor = ($<cbuf>3)->str;
				g_string_free ($<cbuf>3, FALSE);
				destructor_line = ccode_line;
				destructor_simple = FALSE;
			} else {
				g_free ($<id>1);
				g_string_free ($<cbuf>3, TRUE);
				yyerror (_("parse error"));
				YYERROR;
			}
					}
	;

initializer:	'=' numtok	{
			initializer = $<id>2;
			initializer_line = ccode_line;
				}
	|	'=' '{' CCODE	{
			initializer = ($<cbuf>3)->str;
			initializer_line = ccode_line;
			g_string_free($<cbuf>3, FALSE);
				}
	;


varoptions:	destructor initializer	{ ; }
	|	initializer destructor	{ ; }
	|	initializer		{ destructor = NULL; }
	|	destructor		{ initializer = NULL; }
	|	TOKEN 			{
			if (strcmp ($<id>1, "GladeXML") == 0) {
				glade_widget = TRUE;
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
					}
	|				{
			destructor = NULL;
			initializer = NULL;
					}
	;

variable:	scope type TOKEN varoptions ';'		{
			push_variable($<id>3, the_scope,$<line>1, NULL);
						}
	|	scope type TOKEN ARRAY_DIM varoptions ';'	{
			push_variable($<id>3, the_scope, $<line>1, $<id>4);
						}
	;

argument:	ARGUMENT flags argtype TOKEN export TOKEN '{' CCODE TOKEN '{' CCODE ';' {
			Node *node = NULL;
			if(strcmp($<id>6,"get")==0 &&
			   strcmp($<id>9,"set")==0) {
				Type *type = pop_type();
				g_free ($<id>6); 
				g_free ($<id>9);
				node = node_new (ARGUMENT_NODE,
						 "gtktype:steal", $<id>3,
						 "atype:steal", type,
						 "flags:steal", $<list>2,
						 "name:steal", $<id>4,
						 "get:steal", ($<cbuf>8)->str,
						 "get_line", $<line>7,
						 "set:steal", ($<cbuf>11)->str,
						 "set_line", $<line>10,
						 "line_no", $<line>1,
						 NULL);

				class_nodes = g_list_append(class_nodes,node);

				g_string_free ($<cbuf>8, FALSE);
				g_string_free ($<cbuf>11, FALSE);

			} else if(strcmp($<id>6,"set")==0 &&
				strcmp($<id>9,"get")==0) {
				Type *type = pop_type();
				g_free ($<id>6); 
				g_free ($<id>9);
				node = node_new (ARGUMENT_NODE,
						 "gtktype:steal", $<id>3,
						 "atype:steal", type,
						 "flags:steal", $<list>2,
						 "name:steal", $<id>4,
						 "get:steal", ($<cbuf>11)->str,
						 "get_line", $<line>10,
						 "set:steal", ($<cbuf>8)->str,
						 "set_line", $<line>7,
						 "line_no", $<line>1,
						 NULL);
				g_string_free ($<cbuf>11, FALSE);
				g_string_free ($<cbuf>8, FALSE);
				class_nodes = g_list_append(class_nodes,node);
			} else {
				g_free ($<id>3); 
				g_free ($<id>4);
				g_free ($<id>6); 
				g_free ($<id>9);
				g_list_foreach ($<list>2, (GFunc)g_free, NULL);
				g_list_free ($<list>2);
				g_string_free ($<cbuf>11, TRUE);
				g_string_free ($<cbuf>8, TRUE);
				yyerror (_("parse error"));
				YYERROR;
			}

			if ($<id>5 != NULL) {
				Argument *arg = (Argument *)node;
				export_accessors (arg->name,
						  arg->get != NULL, arg->get_line,
						  arg->set != NULL, arg->set_line,
						  arg->atype,
						  arg->gtktype,
						  arg->line_no);
				g_free ($<id>5);
			} 

						}
	|	ARGUMENT flags argtype TOKEN export TOKEN '{' CCODE ';' {
			Node *node = NULL;
			if(strcmp($<id>6, "get") == 0) {
				Type *type = pop_type();
				g_free ($<id>6);
				node = node_new (ARGUMENT_NODE,
						 "gtktype:steal", $<id>3,
						 "atype:steal", type,
						 "flags:steal", $<list>2,
						 "name:steal", $<id>4,
						 "get:steal", ($<cbuf>8)->str,
						 "get_line", $<line>7,
						 "line_no", $<line>1,
						 NULL);

				g_string_free ($<cbuf>8, FALSE);
				class_nodes = g_list_append(class_nodes, node);
			} else if(strcmp($<id>6, "set") == 0) {
				Type *type = pop_type();
				g_free ($<id>6);
				node = node_new (ARGUMENT_NODE,
						 "gtktype:steal", $<id>3,
						 "atype:steal", type,
						 "flags:steal", $<list>2,
						 "name:steal", $<id>4,
						 "set:steal", ($<cbuf>8)->str,
						 "set_line", $<line>7,
						 "line_no", $<line>1,
						 NULL);

				g_string_free ($<cbuf>8, FALSE);
				class_nodes = g_list_append (class_nodes, node);
			} else {
				g_free ($<id>6); 
				g_free ($<id>3);
				g_free ($<id>4);
				g_list_foreach ($<list>2, (GFunc)g_free, NULL);
				g_list_free ($<list>2);
				g_string_free ($<cbuf>8, TRUE);
				yyerror(_("parse error"));
				YYERROR;
			}

			if ($<id>5 != NULL) {
				Argument *arg = (Argument *)node;
				export_accessors (arg->name,
						  arg->get != NULL, arg->get_line,
						  arg->set != NULL, arg->set_line,
						  arg->atype,
						  arg->gtktype,
						  arg->line_no);
				g_free ($<id>5);
			} 
						}
	|	ARGUMENT flags argtype TOKEN export TOKEN {
			Node *node;
			char *get, *set = NULL;
			Variable *var;
			Type *type;
			const char *root;
			
			if(strcmp($<id>6, "link")!=0 &&
			   strcmp($<id>6, "stringlink")!=0 && 
			   strcmp($<id>6, "objectlink")!=0) {
				g_free($<id>6); 
				g_free($<id>3);
				g_free($<id>4);
				g_list_foreach($<list>2,(GFunc)g_free,NULL);
				g_list_free($<list>2);
				yyerror(_("parse error"));
				YYERROR;
			}

			type = pop_type();

			var = find_var_or_die($<id>4, $<line>1);
			if(var->scope == PRIVATE_SCOPE) {
				root = "self->_priv";
			} else if(var->scope == CLASS_SCOPE) {
				root = "SELF_GET_CLASS(self)";
				if(no_self_alias)
					error_print(GOB_ERROR, $<line>1,
						    _("Self aliases needed when autolinking to a classwide member"));
			} else {
				root = "self";
			}

			if(strcmp($<id>6, "link")==0) {
				set = g_strdup_printf("%s->%s = ARG;",
						      root, $<id>4);
			} else if(strcmp($<id>6, "stringlink")==0) {
				set = g_strdup_printf("g_free (%s->%s); "
						      "%s->%s = g_strdup (ARG);",
						      root, $<id>4,
						      root, $<id>4);
			} else if(strcmp($<id>6, "objectlink")==0) {
				set = g_strdup_printf(
				  "if (ARG != NULL) "
				   "g_object_ref (G_OBJECT (ARG)); "
				  "if (%s->%s != NULL) "
				   "g_object_unref (G_OBJECT (%s->%s)); "
				  "%s->%s = ARG;",
				  root, $<id>4,
				  root, $<id>4,
				  root, $<id>4);
			} else {
				g_assert_not_reached();
			}

			get = g_strdup_printf("ARG = %s->%s;", root, $<id>4);
  
			g_free ($<id>6);

			if (type == NULL)
				type = (Type *)node_copy ((Node *)var->vtype);

			node = node_new (ARGUMENT_NODE,
					 "gtktype:steal", $<id>3,
					 "atype:steal", type,
					 "flags:steal", $<list>2,
					 "name:steal", $<id>4,
					 "get:steal", get,
					 "get_line", $<line>1,
					 "set:steal", set,
					 "set_line", $<line>1,
					 "line_no", $<line>1,
					 NULL);

			if ($<id>5 != NULL) {
				Argument *arg = (Argument *)node;
				export_accessors (arg->name,
						  arg->get != NULL, arg->get_line,
						  arg->set != NULL, arg->set_line,
						  arg->atype,
						  arg->gtktype,
						  arg->line_no);
				g_free ($<id>5);
			} 

			class_nodes = g_list_append (class_nodes, node);
						}
	;

export:         '(' TOKEN ')'                   {
			if (strcmp ($<id>2, "export")!=0) {
				g_free ($<id>2); 
				yyerror (_("parse error"));
				YYERROR;
			}
			$<id>$ = $<id>2;
						}
         |                                      {
			$<id>$ = NULL;
						}
	 ;

property:	PROPERTY TOKEN TOKEN param_spec TOKEN '{' CCODE TOKEN '{' CCODE ';' {
			ensure_property ();
			node_set ((Node *)property,
				  "line_no", $<line>1,
				  "gtktype:steal", debool ($<id>2),
				  "name:steal", $<id>3,
				  NULL);
			if (strcmp ($<id>5, "get") == 0 &&
			    strcmp ($<id>8, "set") == 0) {
				node_set ((Node *)property,
					  "get:steal", ($<cbuf>7)->str,
					  "get_line", $<line>6,
					  "set:steal", ($<cbuf>10)->str,
					  "set_line", $<line>9,
					  NULL);
				g_string_free ($<cbuf>7, FALSE);
				g_string_free ($<cbuf>10, FALSE);
				g_free ($<id>5); 
				g_free ($<id>8);
			} else if (strcmp ($<id>5, "set") == 0 &&
				   strcmp ($<id>8, "get") == 0) {
				node_set ((Node *)property,
					  "get:steal", ($<cbuf>10)->str,
					  "get_line", $<line>9,
					  "set:steal", ($<cbuf>7)->str,
					  "set_line", $<line>6,
					  NULL);
				g_string_free ($<cbuf>7, FALSE);
				g_string_free ($<cbuf>10, FALSE);
				g_free ($<id>5); 
				g_free ($<id>8);
			} else {
				g_string_free ($<cbuf>7, TRUE);
				g_string_free ($<cbuf>10, TRUE);
				g_free ($<id>5); 
				g_free ($<id>8);
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
	|	PROPERTY TOKEN TOKEN param_spec TOKEN '{' CCODE ';' {
			ensure_property ();
			node_set ((Node *)property,
				  "line_no", $<line>1,
				  "gtktype:steal", debool ($<id>2),
				  "name:steal", $<id>3,
				  NULL);
			if (strcmp ($<id>5, "get") == 0) {
				node_set ((Node *)property,
					  "get:steal", ($<cbuf>7)->str,
					  "get_line", $<line>6,
					  NULL);
				g_string_free ($<cbuf>7, FALSE);
				g_free ($<id>5); 
			} else if (strcmp ($<id>5, "set") == 0) {
				node_set ((Node *)property,
					  "set:steal", ($<cbuf>7)->str,
					  "set_line", $<line>6,
					  NULL);
				g_string_free ($<cbuf>7, FALSE);
				g_free ($<id>5); 
			} else {
				g_string_free ($<cbuf>7, TRUE);
				g_free ($<id>5); 
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
	|	PROPERTY TOKEN TOKEN param_spec ';' {
			ensure_property ();
			node_set ((Node *)property,
				  "line_no", $<line>1,
				  "gtktype:steal", debool ($<id>2),
				  "name:steal", $<id>3,
				  NULL);
			property_link_and_export ((Node *)property);
			if (property != NULL) {
				class_nodes = g_list_append (class_nodes,
							     property);
				property = NULL;
			}
		}
	;

param_spec:	'(' param_spec_list ')'	{ ; }
	|				{ ; }
	;

param_spec_list:	param_spec_list ',' param_spec_value	{ ; }
	|		param_spec_value			{ ; }
	;

string:		STRING				{ $<id>$ = $<id>1; }
	|	TOKEN '(' STRING ')'		{
			if (strcmp ($<id>1, "_") != 0) {
				g_free ($<id>1);
				yyerror(_("parse error"));
				YYERROR;
			}
			g_free ($<id>1);
			$<id>$ = g_strconcat ("_(", $<id>3, ")", NULL);
			g_free ($<id>3);
		}
	;

anyval:		numtok		{ $<id>$ = $<id>1; }
	|	string		{ $<id>$ = $<id>1; }
	;

param_spec_value: NAME '=' string		{
		ensure_property ();
		node_set ((Node *)property,
			  "canonical_name:steal", gob_str_delete_quotes($<id>3),
			  NULL);
		  }
	|	NICK '=' string		{
		ensure_property ();
		node_set ((Node *)property,
			  "nick:steal", $<id>3,
			  NULL);
		  }
	|	BLURB '=' string		{
		ensure_property ();
		node_set ((Node *)property,
			  "blurb:steal", $<id>3,
			  NULL);
		  }
	|	MAXIMUM '=' numtok		{
		ensure_property ();
		node_set ((Node *)property,
			  "maximum:steal", $<id>3,
			  NULL);
		  }
	|	MINIMUM '=' numtok		{
		ensure_property ();
		node_set ((Node *)property,
			  "minimum:steal", $<id>3,
			  NULL);
		  }
	|	DEFAULT_VALUE '=' anyval	{
		ensure_property ();
		node_set ((Node *)property,
			  "default_value:steal", $<id>3,
			  NULL);
		  }
	|	FLAGS '=' flaglist		{
		ensure_property ();
		node_set ((Node *)property,
			  "flags:steal", $<list>3,
			  NULL);
		  }
	|	TYPE '=' type			{
		Type *type = pop_type ();
		ensure_property ();
		node_set ((Node *)property,
			  "ptype:steal", type,
			  NULL);
		  }
	|	FLAGS_TYPE '=' TYPETOKEN	{
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", $<id>3,
			  NULL);
		  }
	|	FLAGS_TYPE '=' TOKEN		{
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", $<id>3,
			  NULL);
		  }
	|	ENUM_TYPE '=' TYPETOKEN		{
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", $<id>3,
			  NULL);
		  }
	|	ENUM_TYPE '=' TOKEN		{
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", $<id>3,
			  NULL);
		  }
	|	PARAM_TYPE '=' TYPETOKEN	{
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", $<id>3,
			  NULL);
		  }
	|	PARAM_TYPE '=' TOKEN		{
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", $<id>3,
			  NULL);
		  }
	|	BOXED_TYPE '=' TYPETOKEN	{
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", $<id>3,
			  NULL);
		  }
	|	BOXED_TYPE '=' TOKEN		{
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", $<id>3,
			  NULL);
		  }
	|	OBJECT_TYPE '=' TYPETOKEN	{
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", $<id>3,
			  NULL);
		  }
	|	OBJECT_TYPE '=' TOKEN		{
		ensure_property ();
		node_set ((Node *)property,
			  "extra_gtktype:steal", $<id>3,
			  NULL);
		  }
	|	TOKEN		{
		ensure_property ();
		if (strcmp ($<id>1, "override") == 0) {
			g_free($<id>1);
			node_set ((Node *)property,
				  "override", TRUE,
				  NULL);
		} else if (strcmp ($<id>1, "link") == 0) {
			g_free($<id>1);
			node_set ((Node *)property,
				  "link", TRUE,
				  NULL);
		} else if (strcmp ($<id>1, "export") == 0) {
			g_free($<id>1);
			node_set ((Node *)property,
				  "export", TRUE,
				  NULL);
		} else {
			g_free($<id>1);
			yyerror(_("parse error"));
			YYERROR;
		}
		  }
	;

argtype:	TOKEN '(' TOKEN type ')' 	{
			if(strcmp($<id>3,"type")!=0) {
				g_free($<id>1);
				g_free($<id>3);
				yyerror(_("parse error"));
				YYERROR;
			}
			$<id>$ = debool ($<id>1);
						}
	|	TOKEN				{
			$<id>$ = debool ($<id>1);
			typestack = g_list_prepend(typestack,NULL);
						}
	;
	
flags:		'(' flaglist ')'		{ $<list>$ = $<list>2; }
	|					{ $<list>$ = NULL; }
	;

flaglist:	TOKEN '|' flaglist		{
			$<list>$ = g_list_append($<list>3,$<id>1);
						}
	|	TOKEN 				{
			$<list>$ = g_list_append(NULL,$<id>1);
						}
	;


type:		specifier_list pointer		 		{
			Node *node = node_new (TYPE_NODE, 
					       "name:steal", $<id>1,
					       "pointer:steal", $<id>2,
					       NULL);
			typestack = g_list_prepend(typestack,node);
							}
	|	specifier_list				{
			Node *node = node_new (TYPE_NODE, 
					       "name:steal", $<id>1,
					       NULL);
			typestack = g_list_prepend(typestack,node);
							}
	;

/* The special cases are neccessary to avoid conflicts */
specifier_list:	spec_list				{
			$<id>$ = $<id>1;
							}
	|	TOKEN					{
			$<id>$ = $<id>1;
							}
	|	CONST TOKEN				{
			$<id>$ = g_strconcat("const ", $<id>2, NULL);
			g_free($<id>2);
							}
	|	TOKEN CONST				{
			$<id>$ = g_strconcat($<id>1, " const", NULL);
			g_free($<id>1);
							}
	|	strunionenum TOKEN			{
			$<id>$ = g_strconcat($<id>1, " ", $<id>2, NULL);
			g_free($<id>2);
							}
	|	CONST strunionenum TOKEN		{
			$<id>$ = g_strconcat("const ", $<id>2, " ",
					     $<id>3, NULL);
			g_free($<id>3);
							}
	|	strunionenum TOKEN CONST		{
			$<id>$ = g_strconcat($<id>1, " ",
					     $<id>2, " const", NULL);
			g_free($<id>2);
							}
	;

/* The special const cases take care of conflicts ! */
spec_list:	specifier spec_list	 		{
			$<id>$ = g_strconcat($<id>1, " ", $<id>2, NULL);
			g_free($<id>2);
							}
	|	TYPETOKEN spec_list	 		{
			$<id>$ = g_strconcat($<id>1, " ", $<id>2, NULL);
			g_free($<id>1);
			g_free($<id>2);
							}
	|	CONST spec_list		 		{
			$<id>$ = g_strconcat("const ", $<id>2, NULL);
			g_free($<id>2);
							}
	|	TYPETOKEN				{
			$<id>$ = $<id>1;
							}
	|	TYPETOKEN CONST				{
			$<id>$ = g_strconcat($<id>1, " const", NULL);
			g_free($<id>1);
							}
	|	specifier				{
			$<id>$ = g_strdup($<id>1);
							}
	|	specifier CONST				{
			$<id>$ = g_strconcat($<id>1, " const", NULL);
							}
	;

specifier:	VOID			{ $<id>$ = "void"; }
        |	CHAR			{ $<id>$ = "char"; }
        |	SHORT			{ $<id>$ = "short"; }
        |	INT			{ $<id>$ = "int"; }
        |	LONG			{ $<id>$ = "long"; }
        |	FLOAT			{ $<id>$ = "float"; }
        |	DOUBLE			{ $<id>$ = "double"; }
        |	SIGNED			{ $<id>$ = "signed"; }
        |	UNSIGNED		{ $<id>$ = "unsigned"; }
        ;

strunionenum:	STRUCT			{ $<id>$ = "struct"; }
	|	UNION			{ $<id>$ = "union"; }
	|	ENUM			{ $<id>$ = "enum"; }
	;

pointer:	'*'			{ $<id>$ = g_strdup("*"); }
	|	'*' CONST		{ $<id>$ = g_strdup("* const"); }
	|	'*' pointer		{
				$<id>$ = g_strconcat("*", $<id>2, NULL);
				g_free($<id>2);
					}
	|	'*' CONST pointer	{
				$<id>$ = g_strconcat("* const", $<id>3, NULL);
				g_free($<id>3);
					}
	;

/* this never sets the_scope */
simplesigtype:	TOKEN sigtype	{
			if(strcmp($<id>1, "first")==0)
				$<sigtype>$ = SIGNAL_FIRST_METHOD;
			else if(strcmp($<id>1, "last")==0)
				$<sigtype>$ = SIGNAL_LAST_METHOD;
			else {
				yyerror(_("signal must be 'first' or 'last'"));
				g_free($<id>1);
				YYERROR;
			}
			g_free($<id>1);
					}
	|	sigtype			{
			$<sigtype>$ = SIGNAL_LAST_METHOD;
					}
	;

/* this always sets the_scope */
fullsigtype:	scope TOKEN sigtype	{
			if(strcmp($<id>2,"first")==0)
				$<sigtype>$ = SIGNAL_FIRST_METHOD;
			else if(strcmp($<id>2,"last")==0)
				$<sigtype>$ = SIGNAL_LAST_METHOD;
			else {
				yyerror(_("signal must be 'first' or 'last'"));
				g_free($<id>2);
				YYERROR;
			}
			g_free($<id>2);
					}
	|	TOKEN scope sigtype	{
			if(strcmp($<id>1,"first")==0)
				$<sigtype>$ = SIGNAL_FIRST_METHOD;
			else if(strcmp($<id>1,"last")==0)
				$<sigtype>$ = SIGNAL_LAST_METHOD;
			else {
				yyerror(_("signal must be 'first' or 'last'"));
				g_free($<id>1);
				YYERROR;
			}
			g_free($<id>1);
					}
	|	scope sigtype		{
			$<sigtype>$ = SIGNAL_LAST_METHOD;
					}
	|	simplesigtype		{
			/* the_scope was default thus public */
			the_scope = PUBLIC_SCOPE;
					}
	;
	
sigtype:	TOKEN '(' tokenlist ')'		{
			gtktypes = g_list_prepend(gtktypes, debool ($<id>1));
						}
	|	TOKEN STRING '(' tokenlist ')'		{
			gtktypes = g_list_prepend(gtktypes, debool ($<id>1));
			signal_name=$<id>2;
						}
	;

tokenlist:	tokenlist ',' TOKEN 		{
			gtktypes = g_list_append(gtktypes, debool ($<id>3));
						}
	|	TOKEN				{ 
			gtktypes = g_list_append(gtktypes, debool ($<id>1));
       						}
	;

codenocode:	'{' CCODE			{ $<cbuf>$ = $<cbuf>2; }
	|	';'				{ $<cbuf>$ = NULL; }
	;

/*here CCODE will include the ending '}' */
method:		SIGNAL flags fullsigtype type TOKEN '(' funcargs ')' methodmods codenocode {
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
			push_function(the_scope, $<sigtype>3,NULL,
				      $<id>5, $<cbuf>10,$<line>1,
				      ccode_line, vararg, $<list>2);
									}
	|	scope SIGNAL flags simplesigtype type TOKEN '(' funcargs ')' methodmods codenocode {
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
			push_function(the_scope, $<sigtype>4, NULL,
				      $<id>6, $<cbuf>11, $<line>2,
				      ccode_line, vararg, $<list>3);
									}
	|	VIRTUAL scope type TOKEN '(' funcargs ')' methodmods codenocode	{
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
			push_function(the_scope, VIRTUAL_METHOD, NULL, $<id>4,
				      $<cbuf>9, $<line>1,
				      ccode_line, vararg, NULL);
									}
	|	scope VIRTUAL type TOKEN '(' funcargs ')' methodmods codenocode	{
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
			push_function(the_scope, VIRTUAL_METHOD, NULL, $<id>4,
				      $<cbuf>9, $<line>2,
				      ccode_line, vararg, NULL);
									}
	|	VIRTUAL type TOKEN '(' funcargs ')' methodmods codenocode	{
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
				      $<id>3, $<cbuf>8, $<line>1,
				      ccode_line, vararg, NULL);
									}
	|	OVERRIDE '(' TYPETOKEN ')' type TOKEN '(' funcargs ')' methodmods codenocode	{
	                if (funcattrs != NULL) {
				char *error = g_strdup_printf
					(_("function attribute macros ('%s' in this case) may not be used with override methods"),
					 funcattrs);
	                        yyerror (error);
		                YYERROR;
		        }
			push_function(NO_SCOPE, OVERRIDE_METHOD, $<id>3,
				      $<id>6, $<cbuf>11,
				      $<line>1, ccode_line,
				      vararg, NULL);
									}
	|	scope type TOKEN '(' funcargs ')' methodmods codenocode	{
			if(the_scope == CLASS_SCOPE) {
				yyerror(_("a method cannot be of class scope"));
				free_all_global_state();
				YYERROR;
			}
			push_function(the_scope, REGULAR_METHOD, NULL, $<id>3,
				      $<cbuf>8, $<line>1, ccode_line,
				      vararg, NULL);
								}
	|	TOKEN '(' TOKEN ')' codenocode	{
			if(strcmp($<id>1, "init")==0) {
				push_init_arg($<id>3,FALSE);
				push_function(NO_SCOPE, INIT_METHOD, NULL,
					      $<id>1, $<cbuf>5, $<line>2,
					      ccode_line, FALSE, NULL);
			} else if(strcmp($<id>1, "class_init")==0) {
				push_init_arg($<id>3,TRUE);
				push_function(NO_SCOPE, CLASS_INIT_METHOD, NULL,
					      $<id>1, $<cbuf>5, $<line>2,
					      ccode_line, FALSE, NULL);
			} else if(strcmp($<id>1, "constructor")==0) {
				push_init_arg($<id>3, FALSE);
				push_function(NO_SCOPE, CONSTRUCTOR_METHOD, NULL,
					      $<id>1, $<cbuf>5, $<line>2,
					      ccode_line, FALSE, NULL);
			} else if(strcmp($<id>1, "dispose")==0) {
				push_init_arg($<id>3, FALSE);
				push_function(NO_SCOPE, DISPOSE_METHOD, NULL,
					      $<id>1, $<cbuf>5, $<line>2,
					      ccode_line, FALSE, NULL);
			} else if(strcmp($<id>1, "finalize")==0) {
				push_init_arg($<id>3, FALSE);
				push_function(NO_SCOPE, FINALIZE_METHOD, NULL,
					      $<id>1, $<cbuf>5, $<line>2,
					      ccode_line, FALSE, NULL);

			} else {
				g_free($<id>1);
				g_free($<id>3);
				g_string_free($<cbuf>5,TRUE);
				yyerror(_("parse error "
					  "(untyped blocks must be init, "
					  "class_init, constructor, dispose "
					  "or finalize)"));
				YYERROR;
			}
						}
	;

methodmods:	TOKEN retcode		{
			g_free(funcattrs); funcattrs = NULL;
			g_free(onerror); onerror = NULL;
			g_free(defreturn); defreturn = NULL;
			if(!set_attr_value($<id>1, $<id>2)) {
				g_free($<id>1);
				g_free($<id>2);
				yyerror(_("parse error"));
				YYERROR;
			}
			g_free($<id>1);
					}
	|	TOKEN retcode TOKEN retcode	{
			g_free(funcattrs); funcattrs = NULL;
			g_free(onerror); onerror = NULL;
			g_free(defreturn); defreturn = NULL;
			if(!set_attr_value($<id>1, $<id>2)) {
				g_free($<id>1); g_free($<id>2);
				g_free($<id>3); g_free($<id>4);
				yyerror(_("parse error"));
				YYERROR;
			}
			if(!set_attr_value($<id>3, $<id>4)) {
				funcattrs = onerror = defreturn = NULL;
				g_free($<id>1); g_free($<id>2);
				g_free($<id>3); g_free($<id>4);
				yyerror(_("parse error"));
				YYERROR;
			}
			g_free($<id>1);
			g_free($<id>3);
						}
	|	TOKEN retcode TOKEN retcode TOKEN retcode	{
			g_free(funcattrs); funcattrs = NULL;
			g_free(onerror); onerror = NULL;
			g_free(defreturn); defreturn = NULL;
			if(!set_attr_value($<id>1, $<id>2)) {
				g_free($<id>1); g_free($<id>2);
				g_free($<id>3); g_free($<id>4);
				g_free($<id>5); g_free($<id>6);
				yyerror(_("parse error"));
				YYERROR;
			}
			if(!set_attr_value($<id>3, $<id>4)) {
				funcattrs = onerror = defreturn = NULL;
				g_free($<id>1); g_free($<id>2);
				g_free($<id>3); g_free($<id>4);
				g_free($<id>5); g_free($<id>6);
				yyerror(_("parse error"));
				YYERROR;
			}
			if(!set_attr_value($<id>5, $<id>6)) {
				funcattrs = onerror = defreturn = NULL;
				g_free($<id>1); g_free($<id>2);
				g_free($<id>3); g_free($<id>4);
				g_free($<id>5); g_free($<id>6);
				yyerror(_("parse error"));
				YYERROR;
			}
			g_free($<id>1);
			g_free($<id>3);
			g_free($<id>5);
						}
	|				{
			g_free(funcattrs); funcattrs = NULL;
			g_free(onerror); onerror = NULL;
			g_free(defreturn); defreturn = NULL;
					}
	;

retcode:	numtok			{ $<id>$ = $<id>1; }
	|	'{' CCODE		{
			$<id>$ = ($<cbuf>2)->str;
			g_string_free($<cbuf>2, FALSE);
					}
	;
	
funcargs:	VOID			{ vararg = FALSE; has_self = FALSE; }
	|	TOKEN			{
			vararg = FALSE;
			has_self = TRUE;
			if(strcmp($<id>1,"self")==0)
				push_self($<id>1, FALSE);
			else {
				g_free($<id>1);
				yyerror(_("parse error"));
				YYERROR;
			}
       					}
	|	TOKEN CONST {
			vararg = FALSE;
			has_self = TRUE;
			if(strcmp($<id>1,"self")==0)
				push_self($<id>1, TRUE);
			else {
				g_free($<id>1);
				yyerror(_("parse error"));
				YYERROR;
			}
       					}
	|	CONST TOKEN {
			vararg = FALSE;
			has_self = TRUE;
			if(strcmp($<id>2,"self")==0)
				push_self($<id>2, TRUE);
			else {
				g_free($<id>2);
				yyerror(_("parse error"));
				YYERROR;
			}
       					}
	|	TOKEN ',' arglist	{
			has_self = TRUE;
			if(strcmp($<id>1,"self")==0)
				push_self($<id>1, FALSE);
			else {
				g_free($<id>1);
				yyerror(_("parse error"));
				YYERROR;
			}
					}
	|	TOKEN CONST ',' arglist	{
			has_self = TRUE;
			if(strcmp($<id>1,"self")==0)
				push_self($<id>1, TRUE);
			else {
				g_free($<id>1);
				yyerror(_("parse error"));
				YYERROR;
			}
					}
	|	CONST TOKEN ',' arglist	{
			has_self = TRUE;
			if(strcmp($<id>2,"self")==0)
				push_self($<id>2, TRUE);
			else {
				g_free($<id>2);
				yyerror(_("parse error"));
				YYERROR;
			}
					}
	|	arglist			{ has_self = FALSE; }
	;

arglist:	arglist1 ',' THREEDOTS	{ vararg = TRUE; }
	|	arglist1		{ vararg = FALSE; }
	;
	
arglist1:	arglist1 ',' arg	{ ; }
	|	arg			{ ; }
	;

arg:		type TOKEN					{
			push_funcarg($<id>2,NULL);
								}
	|	type TOKEN ARRAY_DIM				{
			push_funcarg($<id>2,$<id>3);
								}
	|	type TOKEN '(' TOKEN checklist ')'		{
			if(strcmp($<id>4,"check")!=0) {
				yyerror(_("parse error"));
				YYERROR;
			}
			g_free($<id>4);
			push_funcarg($<id>2,NULL);
								}
	|	type TOKEN ARRAY_DIM '(' TOKEN checklist ')'	{
			if(strcmp($<id>5,"check")!=0) {
				yyerror(_("parse error"));
				YYERROR;
			}
			g_free($<id>5);
			push_funcarg($<id>2,$<id>3);
								}
	;
	
checklist:	checklist check 	{ ; }
	|	check			{ ; }
	;

check:		TOKEN			{
			if(strcmp($<id>1,"type")==0) {
				Node *node = node_new (CHECK_NODE,
						       "chtype", TYPE_CHECK,
						       NULL);
				checks = g_list_append(checks,node);
			} else if(strcmp($<id>1,"null")==0) {
				Node *node = node_new (CHECK_NODE,
						       "chtype", NULL_CHECK,
						       NULL);
				checks = g_list_append(checks,node);
			} else {
				yyerror(_("parse error"));
				YYERROR;
			}
			g_free($<id>1);
					}
	|	'>' numtok		{
			Node *node = node_new (CHECK_NODE,
					       "chtype", GT_CHECK,
					       "number:steal", $<id>2,
					       NULL);
			checks = g_list_append(checks,node);
					}
	|	'<' numtok		{
			Node *node = node_new (CHECK_NODE,
					       "chtype", LT_CHECK,
					       "number:steal", $<id>2,
					       NULL);
			checks = g_list_append(checks,node);
					}
	|	'>' '=' numtok		{
			Node *node = node_new (CHECK_NODE,
					       "chtype", GE_CHECK,
					       "number:steal", $<id>3,
					       NULL);
			checks = g_list_append(checks,node);
					}
	|	'<' '=' numtok		{
			Node *node = node_new (CHECK_NODE,
					       "chtype", LE_CHECK,
					       "number:steal", $<id>3,
					       NULL);
			checks = g_list_append(checks,node);
					}
	|	'=' '=' numtok		{
			Node *node = node_new (CHECK_NODE,
					       "chtype", EQ_CHECK,
					       "number:steal", $<id>3,
					       NULL);
			checks = g_list_append(checks,node);
					}
	|	'!' '=' numtok		{
			Node *node = node_new (CHECK_NODE,
					       "chtype", NE_CHECK,
					       "number:steal", $<id>3,
					       NULL);
			checks = g_list_append(checks,node);
					}
	;

enumcode:	ENUM TOKEN '{' enumvals '}' TYPETOKEN ';' {
			Node *node = node_new (ENUMDEF_NODE,
					       "etype:steal", $<id>6,
					       "prefix:steal", $<id>2,
					       "values:steal", enum_vals,
					       NULL);
			enum_vals = NULL;
			nodes = g_list_append (nodes, node);
			}
	|	ENUM TOKEN '{' enumvals ',' '}' TYPETOKEN ';' {
			Node *node = node_new (ENUMDEF_NODE,
					       "etype:steal", $<id>7,
					       "prefix:steal", $<id>2,
					       "values:steal", enum_vals,
					       NULL);
			enum_vals = NULL;
			nodes = g_list_append (nodes, node);
			}
	;

enumvals:	enumvals ',' enumval	{;}
	|	enumval			{;}
	;

enumval:	TOKEN '=' numtok	{
			Node *node;
			char *num = $<id>3;

			/* A float value, that's a bad enum */
			if (num[0] >= '0' &&
			    num[0] <= '9' &&
			    strchr (num, '.') != NULL) {
				g_free ($<id>1);
				g_free (num);
				yyerror(_("parse error (enumerator value not integer constant)"));
				YYERROR;
			}
		       
			node = node_new (ENUMVALUE_NODE,
					 "name:steal", $<id>1,
					 "value:steal", num,
					 NULL);
			enum_vals = g_list_append (enum_vals, node);
			}
	|	TOKEN			{
			Node *node;

			node = node_new (ENUMVALUE_NODE,
					 "name:steal", $<id>1,
					 NULL);
			enum_vals = g_list_append (enum_vals, node);
	}
	;

flagcode:	FLAGS TOKEN '{' flagvals '}' TYPETOKEN ';' {
			Node *node = node_new (FLAGS_NODE,
					       "ftype:steal", $<id>6,
					       "prefix:steal", $<id>2,
					       "values:steal", flag_vals,
					       NULL);
			flag_vals = NULL;
			nodes = g_list_append (nodes, node);
			}
	|	FLAGS TOKEN '{' flagvals ',' '}' TYPETOKEN ';' {
			Node *node = node_new (FLAGS_NODE,
					       "ftype:steal", $<id>7,
					       "prefix:steal", $<id>2,
					       "values:steal", flag_vals,
					       NULL);
			flag_vals = NULL;
			nodes = g_list_append (nodes, node);
			}
	;

flagvals:	flagvals ',' TOKEN	{
			flag_vals = g_list_append (flag_vals, $<id>3);
		}
	|	TOKEN			{
			flag_vals = g_list_append (flag_vals, $<id>1);
		}
	;

errorcode:	ERROR TOKEN '{' errorvals '}' TYPETOKEN ';' {
			Node *node = node_new (ERROR_NODE,
					       "etype:steal", $<id>6,
					       "prefix:steal", $<id>2,
					       "values:steal", error_vals,
					       NULL);
			error_vals = NULL;
			nodes = g_list_append (nodes, node);
			}
	|	ERROR TOKEN '{' errorvals ',' '}' TYPETOKEN ';' {
			Node *node = node_new (ERROR_NODE,
					       "etype:steal", $<id>7,
					       "prefix:steal", $<id>2,
					       "values:steal", error_vals,
					       NULL);
			error_vals = NULL;
			nodes = g_list_append (nodes, node);
			}
	;

errorvals:	errorvals ',' TOKEN	{
			error_vals = g_list_append (error_vals, $<id>3);
		}
	|	TOKEN			{
			error_vals = g_list_append (error_vals, $<id>1);
		}
	;


numtok:		NUMBER			{ $<id>$ = $<id>1; }
	|	'-' NUMBER		{
			$<id>$ = g_strconcat("-",$<id>2,NULL);
			g_free($<id>2);
					}
	|	SINGLE_CHAR		{ $<id>$ = $<id>1; }
	|	TOKEN			{ $<id>$ = $<id>1; }
	;
	
%%
