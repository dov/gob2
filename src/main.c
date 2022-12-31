/* GOB C Preprocessor
 * Copyright (C) 1999,2000 the Free Software Foundation.
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2001-2011 George (Jiri) Lebl
 *
 * Author: George (Jiri) Lebl
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

#include "config.h"
#include <glib.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "treefuncs.h"
#include "parse.h"
#include "out.h"
#include "util.h"
#include "checks.h"

#include "main.h"

char *filename = NULL;

int yyparse(void);

extern int yydebug;
extern FILE * yyin;
extern Node *class;
extern GList *nodes;
extern GList *enums;

extern GList *include_files;

extern GHashTable *gtk_doc_hash;

char *filebase;
char *fullfilebase;
static char *outfilebase;
static char *outfilehbase;
static char *outfilephbase;
static char *funcbase;
static char *pfuncbase;
static char *macrobase;
static char *macrois;
static char *pmacrois;
static char *macrotype;
static char *pmacrotype;
static char *typebase;
static char *ptypebase;

char *output_dir = NULL;

char file_sep = '-';

static int signals = 0; /* number of signals */
static int set_properties = 0; /* number of named (set) properties */
static int get_properties = 0; /* number of named (get) properties */
static int overrides = 0; /* number of override methods */
static int privates = 0; /* number of private data members */
static int protecteds = 0; /* number of protected methods */
static int unreftors = 0; /* number of variable unreffing destructors */
static int destructors = 0; /* number of variable non-unreffing destructors */
static int initializers = 0; /* number of variable initializers */
static int glade_widgets = 0; /* number of glade widgets */
static gboolean overrode_get_type = FALSE; /* provided your won _get_type */

static gboolean made_aliases = FALSE;  /* if we made any shorthand aliases
					  and need the REALLY UGLY HACK to
					  avoid warnings */

/* the special variable types we need to define */
static gboolean special_array[SPECIAL_LAST] = {0};
static gboolean any_special = FALSE;

static gboolean need_constructor = FALSE;
static Method * user_constructor = NULL;

static gboolean need_dispose = FALSE;
static Method * dispose_handler = NULL;
static Method * user_dispose_method = NULL;
 
static gboolean need_finalize = FALSE;
static Method * finalize_handler = NULL;
static Method * user_finalize_method = NULL;

FILE *out = NULL;
FILE *outh = NULL;
FILE *outph = NULL;

gboolean no_touch = FALSE;
gboolean no_touch_headers = FALSE;
gboolean for_cpp = FALSE;
gboolean no_gnu = FALSE;
gboolean exit_on_warn = FALSE;
gboolean exit_on_error = TRUE;
gboolean got_error = FALSE;
gint private_header = PRIVATE_HEADER_ONDEMAND;
gboolean no_extern_c = FALSE;
gboolean no_write = FALSE;
gboolean no_lines = FALSE;
gboolean no_self_alias = FALSE;
gboolean always_private_struct = FALSE;
gboolean gtk3_ok = FALSE;

gint prealloc = 0;



gboolean use_m4 = FALSE; /* preprocess sources with m4 */
gboolean use_m4_clean = FALSE; /* preprocess sources with m4, no m4 flags */
char *m4_commandline = NULL;
#define M4_INCLUDE_DIR  PKGDATADIR "/m4"
#define M4_BASE_FILENAME "gobm4.m4"
#define M4_FLAGS "-P -s -I" M4_INCLUDE_DIR  " -DGOBM4_GOB_VERSION=" VERSION " " M4_BASE_FILENAME
#define M4_COMMANDLINE "m4"

int method_unique_id = 1;

static void
make_bases (void)
{
	filebase = replace_sep (((Class *)class)->otype, file_sep);
	gob_strdown (filebase);

	if (output_dir != NULL &&
	    output_dir[0] != '\0') {
		fullfilebase = g_build_filename (output_dir, filebase, NULL);
	} else {
		fullfilebase = g_strdup (filebase);
	}

	funcbase = replace_sep (((Class *)class)->otype, '_');
	gob_strdown (funcbase);

	pfuncbase = replace_sep (((Class *)class)->ptype, '_');
	gob_strdown (pfuncbase);

	macrobase = replace_sep (((Class *)class)->otype, '_');
	gob_strup (macrobase);
	
	macrois = make_pre_macro (((Class *)class)->otype, "IS");
	pmacrois = make_pre_macro (((Class *)class)->ptype, "IS");

	macrotype = make_pre_macro (((Class *)class)->otype, "TYPE");
	pmacrotype = make_pre_macro (((Class *)class)->ptype, "TYPE");

	typebase = remove_sep (((Class *)class)->otype);

	ptypebase = remove_sep (((Class *)class)->ptype);
}

static char *
get_gtk_doc (const char *id)
{
	char *val;

	if(!gtk_doc_hash)
		return NULL;

	val = g_hash_table_lookup(gtk_doc_hash, id);
	if(val)
		return g_strdup_printf("/**\n * %s_%s:\n%s **/\n",
				       funcbase, id, val);
	val = g_hash_table_lookup(gtk_doc_hash, id);
	if(val)
		return g_strdup_printf("/**\n * %s_%s:\n%s **/\n",
				       funcbase, id, val);
	return NULL;
}

static void
print_type(FILE *fp, const Type *t, gboolean postfix_to_stars)
{
	char *s;

	s = get_type(t, postfix_to_stars);
	out_printf(fp, "%s", s); 
	g_free(s);
}


static void
print_method (FILE *fp,
	      const char *typeprefix,
	      const char *nameprefix,
	      const char *subnameprefix,
	      const char *namepostfix,
	      const char *afterargs,
	      const char *postfix,
	      const Method *m,
	      gboolean print_funcattrs,
	      gboolean one_arg_per_line,
	      gboolean no_funcbase,
	      gboolean kill_underscore,
	      gboolean first_unused,
	      gboolean fake_names)
{
	GList *li;
	const char *id;

	out_printf(fp, "%s", typeprefix); 
	print_type(fp, m->mtype, TRUE);

	id = m->id;

	if(no_funcbase)
		out_printf(fp, "%s%s%s%s(",
			   nameprefix, subnameprefix, id, namepostfix); 
	else
		out_printf(fp, "%s%s_%s%s%s(",
			   nameprefix, funcbase, subnameprefix, id,
			   namepostfix); 
	
	if(m->args) {
		for(li=m->args; li; li=g_list_next(li)) {
			FuncArg *arg = li->data;
			const char *unused = "";

			if ( ! no_gnu &&
			     ! for_cpp && /* g++ has a cow with this */
			    li == m->args &&
			    first_unused) {
				unused = " G_GNUC_UNUSED";
			}

			print_type(fp, arg->atype, FALSE);
			if (fake_names)
				out_printf (fp, "___fake___");
			if(li->next)
				out_printf(fp, "%s%s%s,%s", arg->name,
					   arg->atype->postfix ?
					   arg->atype->postfix : "",
					   unused,
					   one_arg_per_line ? "\n\t\t\t\t\t" : " ");
			else
				out_printf(fp, "%s%s%s", arg->name,
					   arg->atype->postfix ?
					   arg->atype->postfix : "",
					   unused); 
		}
		if(m->vararg)
			out_printf(fp, ",%s...",
				   one_arg_per_line ? "\n\t\t\t\t\t" : " "); 
	} else {
		out_printf(fp, "void"); 
	}
	/* Slightly icky: sometimes we are called st m->funcattrs
	   hasn't been set, but if so it should be NULL since its been
	   zero-initialized.  */
	if(print_funcattrs && m->funcattrs != NULL
	   && strlen(m->funcattrs) > 0) {
	        /* To keep the output neat, we trim off the trailing '\n'
	           from the end of funcattrs for a moment.  */
	        size_t funcattrs_len = strlen(m->funcattrs);
                gboolean funcattrs_chomped = FALSE;
	        if((m->funcattrs)[funcattrs_len - 1] == '\n') {
	                m->funcattrs[funcattrs_len - 1] = '\0';
                        funcattrs_chomped = TRUE;
		} 
	        out_printf(fp, "%s)\n%s%s", afterargs, m->funcattrs, postfix);
                /* Put it back like it was (though it shouldn't matter).  */
		if (funcattrs_chomped) {
		        (m->funcattrs)[funcattrs_len - 1] = '\n';
		}
	}
	else {
	        out_printf(fp, "%s)%s", afterargs, postfix); 
	}
}

static gboolean
any_method_to_alias(Class *c)
{
	GList *li;
	
	for(li=c->nodes;li;li=g_list_next(li)) {
		Node *node = li->data;
		if(node->type == METHOD_NODE) {
			Method *m = (Method *)node;
			
			if(m->method == INIT_METHOD ||
			   m->method == CLASS_INIT_METHOD ||
			   m->method == CONSTRUCTOR_METHOD ||
			   m->method == DISPOSE_METHOD ||
			   m->method == FINALIZE_METHOD ||
			   m->method == OVERRIDE_METHOD)
				continue;

			return TRUE;
		}
	}
	return FALSE;
}


static void
make_method_aliases (Class *c)
{
	GList *li;
	
	for(li = c->nodes; li != NULL; li = li->next) {
		Node *node = li->data;
		if(node->type == METHOD_NODE) {
			Method *m = (Method *)node;
			
			if(m->method == INIT_METHOD ||
			   m->method == CLASS_INIT_METHOD ||
			   m->method == CONSTRUCTOR_METHOD ||
			   m->method == DISPOSE_METHOD ||
			   m->method == FINALIZE_METHOD ||
			   m->method == OVERRIDE_METHOD)
				continue;

			out_printf (out, "#define self_%s %s_%s\n",
				    m->id,
				    funcbase,
				    m->id);
		}
	}
}

static void
add_bad_hack_to_avoid_unused_warnings(const Class *c)
{
	GList *li;

	/* if we haven't had any methods, just return */
	if( ! made_aliases)
		return;
	
	if( ! no_gnu)
		out_printf(out, "\n\n#if (!defined __GNUC__) || (defined __GNUC__ && defined __STRICT_ANSI__)\n");
	out_printf(out,
		   "/*REALLY BAD HACK\n"
		   "  This is to avoid unused warnings if you don't call\n"
		   "  some method.  I need to find a better way to do\n"
		   "  this, not needed in GCC since we use some gcc\n"
		   "  extentions to make saner, faster code */\n"
		   "static void\n"
		   "___%s_really_bad_hack_to_avoid_warnings(void)\n"
		   "{\n", funcbase);
	out_printf(out, "\t((void (*)(void))GET_NEW_VARG)();\n");
	for(li=c->nodes;li;li=g_list_next(li)) {
		Node *node = li->data;
		if(node->type == METHOD_NODE) {
			Method *m = (Method *)node;
			
			if(m->method == INIT_METHOD ||
			   m->method == CLASS_INIT_METHOD ||
			   m->method == CONSTRUCTOR_METHOD ||
			   m->method == DISPOSE_METHOD ||
			   m->method == FINALIZE_METHOD ||
			   m->method == OVERRIDE_METHOD)
				continue;

			/* in C++ mode we don't alias new */
			if(for_cpp && strcmp(m->id, "new")==0)
				continue;

			out_printf(out, "\t((void (*)(void))self_%s)();\n", m->id);
		}
	}
	out_printf(out, "\t___%s_really_bad_hack_to_avoid_warnings();\n",
		   funcbase);
	if(!no_gnu)
		out_printf(out, "}\n#endif /* !__GNUC__ || (__GNUC__ && __STRICT_ANSI__) */\n\n");
	else
		out_printf(out, "}\n\n");
}

static void
put_variable(const Variable *v, FILE *fp)
{
	out_printf(fp, "\t");
	print_type(fp, v->vtype, FALSE);
	out_printf(fp, "%s%s;", v->id,
		   v->vtype->postfix?
		   v->vtype->postfix:""); 
	if(v->scope == PROTECTED_SCOPE)
		out_printf(fp, " /* protected */");
	out_printf(fp, "\n");
}

static void
put_vs_method(const Method *m)
{
	if(m->method != SIGNAL_LAST_METHOD &&
	   m->method != SIGNAL_FIRST_METHOD &&
	   m->method != VIRTUAL_METHOD)
		return;

	/* if a signal mark it as such */
	if(m->method != VIRTUAL_METHOD)
	        print_method(outh, "\t/*signal*/", "(* ", "", ") ", "", ";\n",
			     m, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE);
	else
	        print_method(outh, "\t", "(* ", "", ") ", "", ";\n",
			     m, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE);

}

static void
put_pub_method(const Method *m)
{
	if(m->scope != PUBLIC_SCOPE)
		return;

	out_addline_infile(outh, m->line_no);
	print_method(outh, "", "\t", "", "\t", "", ";\n", m,
		     TRUE, TRUE, FALSE, TRUE, FALSE, FALSE);
	out_addline_outfile(outh);
}

static void
put_signal_macro (const Method *m, gboolean gnu)
{
	if(m->method != SIGNAL_LAST_METHOD &&
	   m->method != SIGNAL_FIRST_METHOD)
		return;

	if ( ! gnu) {
		/* connect */
		out_printf (outh, "#define %s_connect__%s(object,func,data)\t"
			    "g_signal_connect(%s(object),\"%s\","
			    "(GCallback)(func),(data))\n",
			    funcbase, m->id, macrobase, m->id);

		/* connect_after */
		out_printf (outh, "#define %s_connect_after__%s(object,func,data)\t"
			    "g_signal_connect_after(%s(object),\"%s\","
			    "(GCallback)(func),(data))\n",
			    funcbase, m->id, macrobase, m->id);

		/* connect_data */
		out_printf (outh, "#define %s_connect_data__%s"
			    "(object,func,data,destroy_data,flags)\t"
			    "g_signal_connect_data(%s(object),\"%s\","
			    "(GCallback)(func),(data),(destroy_data),(GConnectFlags)(flags))\n",
			    funcbase, m->id, macrobase, m->id);
	} else {
		/* connect */
		out_printf (outh, "#define %s_connect__%s(object,func,data)\t"
			    "g_signal_connect("
			    "%s(__extension__ ({%s *___object = (object); ___object; })),"
			    "\"%s\","
			    "(GCallback) __extension__ ({",
			    funcbase, m->id, macrobase, typebase, m->id);
		print_method (outh, "", "(* ___", "", ") ", ", gpointer ___data ",
			      " = (func); ", m, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE);
		out_printf (outh, "___%s; }), (data))\n", m->id);

		/* connect_after */
		out_printf (outh, "#define %s_connect_after__%s(object,func,data)\t"
			    "g_signal_connect_after("
			    "%s(__extension__ ({%s *___object = (object); ___object; })),"
			    "\"%s\","
			    "(GCallback) __extension__ ({",
			    funcbase, m->id, macrobase, typebase, m->id);
		print_method (outh, "", "(* ___", "", ") ", ", gpointer ___data ",
			      " = (func); ", m, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE);
		out_printf (outh, "___%s; }), (data))\n", m->id);

		/* connect_data */
		out_printf (outh, "#define %s_connect_data__%s"
			    "(object,func,data,destroy_data,flags)\t"
			    "g_signal_connect_data("
			    "%s(__extension__ ({%s *___object = (object); ___object; })),"
			    "\"%s\","
			    "(GCallback) __extension__ ({",
			    funcbase, m->id, macrobase, typebase, m->id);
		print_method (outh, "", "(* ___", "", ") ", ", gpointer ___data ",
			      " = (func); ", m, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE);
		out_printf (outh, "___%s; }), (data), (destroy_data), (GConnectFlags)(flags))\n", m->id);
	}
}

static void
put_signal_macros (const Class *c, gboolean gnu)
{
	const GList *li;

	if (signals < 0)
		return;

	for (li = c->nodes; li != NULL; li = li->next) {
		const Node *n = li->data;
		if (n->type == METHOD_NODE)
			put_signal_macro ((Method *)n, gnu);
	}
}

static void
put_local_signal_macro (const Method *m)
{
	if(m->method != SIGNAL_LAST_METHOD &&
	   m->method != SIGNAL_FIRST_METHOD)
		return;

	/* connect */
	out_printf (out, "#define self_connect__%s(object,func,data)\t"
		    "%s_connect__%s((object),(func),(data))\n",
		    m->id, funcbase, m->id);

	/* connect_after */
	out_printf (out, "#define self_connect_after__%s(object,func,data)\t"
		    "%s_connect_after__%s((object),(func),(data))\n",
		    m->id, funcbase, m->id);

	/* connect_data */
	out_printf (out, "#define self_connect_data__%s(object,func,data,destroy_data,flags)\t"
		    "%s_connect_data__%s((object),(func),(data),(destroy_data),(flags))\n",
		    m->id, funcbase, m->id);
}

static void
put_local_signal_macros (const Class *c)
{
	const GList *li;

	if (signals < 0)
		return;

	for (li = c->nodes; li != NULL; li = li->next) {
		const Node *n = li->data;
		if (n->type == METHOD_NODE)
			put_local_signal_macro ((Method *)n);
	}
}


static void
put_prot_method(const Method *m)
{
	FILE *f;

 	if(m->scope != PROTECTED_SCOPE)
 		return;
 
	f = outph ? outph : out;

	out_addline_infile(f, m->line_no);
	print_method(f, "", "\t", "", "\t", "", ";\n",
		     m, TRUE, FALSE, FALSE, TRUE, FALSE, FALSE);
	out_addline_outfile(f);
}

static void
put_priv_method_prot(const Method *m)
{
	if(m->method == SIGNAL_LAST_METHOD ||
	   m->method == SIGNAL_FIRST_METHOD ||
	   m->method == VIRTUAL_METHOD) {
		if(m->cbuf)
			print_method(out,
				     "static ", "___real_", "", " ", "", ";\n",
				     m, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE);
	}
	/* no else, here, it might still have a private prototype, it's not
	 * exclusive */

	if((m->method == OVERRIDE_METHOD &&
	    m->cbuf)) {
		/* add unique ID */
		char *s = g_strdup_printf("___%x_", (guint)m->unique_id);
		if (m->line_no > 0)
			out_addline_infile(out, m->line_no);
		print_method(out, "static ", s, "", " ", "",
			     no_gnu?";\n":" G_GNUC_UNUSED;\n",
			     m, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
		if (m->line_no > 0)
			out_addline_outfile(out);
		g_free(s);
	} else if(m->scope == PRIVATE_SCOPE ||
		  m->method == INIT_METHOD ||
		  m->method == CLASS_INIT_METHOD ||
		  m->method == CONSTRUCTOR_METHOD ||
		  m->method == DISPOSE_METHOD ||
		  m->method == FINALIZE_METHOD) {
		if (m->line_no > 0)
			out_addline_infile(out, m->line_no);
		print_method(out, "static ", "", "", " ", "",
			     no_gnu?";\n":" G_GNUC_UNUSED;\n",
			     m, TRUE, FALSE, FALSE, TRUE, FALSE, FALSE);
		if (m->line_no > 0)
			out_addline_outfile(out);
	}
}

static GList *
make_func_arg (const char *typename, gboolean is_class, const char *name)
{
	Node *node;
	Node *type;
	char *tn;
	
	if (is_class)
		tn = g_strconcat (typename, ":Class", NULL);
	else
		tn = g_strdup (typename);

	type = node_new (TYPE_NODE,
			 "name:steal", tn,
			 "pointer", "*",
			 NULL);
	node = node_new (FUNCARG_NODE,
			 "atype:steal", (Type *)type,
			 "name", name,
			 NULL);
	return g_list_prepend (NULL, node);
}

static void
make_inits(Class *cl)
{
	int got_class_init = FALSE;
	int got_init = FALSE;
	GList *li;
	Node *node;
	for(li=cl->nodes;li;li=g_list_next(li)) {
		Node *n = li->data;
		if(n->type == METHOD_NODE) {
			Method *m = (Method *)n;
			if(m->method == INIT_METHOD) {
				if(got_init)
					error_print(GOB_ERROR, m->line_no, "init defined more then once");
				got_init = TRUE;
			} else if(m->method == CLASS_INIT_METHOD) {
				if(got_class_init)
					error_print(GOB_ERROR, m->line_no, "class_init defined more then once");
				got_class_init = TRUE;
			}
		}
	}
	if(!got_class_init) {
		Type *type = (Type *)node_new (TYPE_NODE,
					       "name", "void",
					       NULL);
		node = node_new (METHOD_NODE,
				 "scope", NO_SCOPE,
				 "method", CLASS_INIT_METHOD,
				 "mtype:steal", type,
				 "id", "class_init",
				 "args:steal", make_func_arg (cl->otype,
							      TRUE /* is_class */,
							      "c" /* name */),
				 "unique_id", method_unique_id++,
				 NULL);
		cl->nodes = g_list_prepend(cl->nodes, node);
	}
	if(!got_init) {
		Type *type = (Type *)node_new (TYPE_NODE,
					       "name", "void",
					       NULL);
		node = node_new (METHOD_NODE,
				 "scope", NO_SCOPE,
				 "method", INIT_METHOD,
				 "mtype:steal", type,
				 "id", "init",
				 "args:steal", make_func_arg (cl->otype,
							      FALSE /* is_class */,
							      "o" /* name */),
				 "unique_id", method_unique_id++,
				 NULL);
		cl->nodes = g_list_prepend(cl->nodes, node);
	}
}

static Method *
find_method(const Class *cl, int method, const char *id)
{
	GList *li;

	for(li=cl->nodes;li;li=g_list_next(li)) {
		Node *n = li->data;
		if(n->type == METHOD_NODE) {
			Method *m = (Method *)n;
			if (m->method == method
			    && (id == NULL || strcmp(m->id, id)==0))
				return m;
		}
	}

	return NULL;
}

static void
find_constructor(const Class *cl)
{
	user_constructor = find_method(cl, CONSTRUCTOR_METHOD, NULL);
}

static void
find_dispose(const Class *cl)
{
	dispose_handler = find_method(cl, OVERRIDE_METHOD, "dispose");
	if (dispose_handler != NULL) {
		if(strcmp(dispose_handler->otype, "G:Object") != 0)
			error_print(GOB_ERROR, dispose_handler->line_no,
				    "dispose method override "
				    "of class other then "
				    "G:Object");
		if(g_list_length(dispose_handler->args) != 1)
			error_print(GOB_ERROR, dispose_handler->line_no,
				    "dispose method override "
				    "with more then one "
				    "parameter");
	}

	user_dispose_method = find_method(cl, DISPOSE_METHOD, NULL);
}

static void
find_finalize(const Class *cl)
{
	finalize_handler = find_method(cl, OVERRIDE_METHOD, "finalize");
	if (finalize_handler != NULL) {
		if(strcmp(finalize_handler->otype, "G:Object") != 0)
			error_print(GOB_ERROR, finalize_handler->line_no,
				    "finalize method override "
				    "of class other then "
				    "G:Object");
		if(g_list_length(finalize_handler->args) != 1)
			error_print(GOB_ERROR, finalize_handler->line_no,
				    "finalize method override "
				    "with more then one "
				    "parameter");
	}

	user_finalize_method = find_method(cl, FINALIZE_METHOD, NULL);
}


/* hash of method -> name of signal prototype */
static GHashTable *marsh = NULL;

/* list of methods with different signal prototypes,
   we check this list if we can use a signal prototype of a
   previous signal method, there are only uniques here */
static GList *eq_signal_methods = NULL;

/* compare a list of strings */
static gboolean
is_list_equal(const GList *a, const GList *b)
{
	for(;a && b; a=a->next, b=b->next) {
		if(strcmp(a->data, b->data)!=0) {
			return FALSE;
		}
	}
	/* the the lists were different length */
	if(a || b)
		return FALSE;
	return TRUE;
}

static Method *
find_same_type_signal(const Method *m)
{
	GList *li;
	for(li=eq_signal_methods;li;li=li->next) {
		Method *mm = li->data;
		if(is_list_equal(mm->gtktypes, m->gtktypes))
			return mm;
	}
	return NULL;
}

static void
print_signal_marsal_args (const Method *m)
{
	if (strcmp (m->gtktypes->next->data, "NONE") != 0) {
		GList *li;
		int i;
		for (i = 0, li = m->gtktypes->next;
		     li != NULL;
		     i++, li = li->next) {
			char *get_func;

			if (strcmp (li->data, "UNICHAR") == 0)
				/* hack because glib is braindamaged */
				get_func = g_strdup ("g_value_get_uint");
			else if (strncmp(li->data, "BOXED_", 6) == 0)
			        get_func = g_strdup ("g_value_get_boxed");
			else
				get_func = g_strdup_printf
					("g_value_get_%s", (char *)li->data);

			gob_strdown (get_func);
			out_printf (out, ",\n\t\t(%s) "
				    "%s (param_values + %d)",
				    get_cast (li->data, FALSE),
				    get_func, i + 1);
			g_free (get_func);
		}
	}
	out_printf (out, ",\n\t\tdata2);\n");
}


static void
add_signal_prots(Method *m)
{
	GList *li;
	static int sig = 1;
	char *s;
	Method *mm;
	gboolean ret_none = FALSE;
	gboolean arglist_none = FALSE;
	const char *retcast;
	const char *unused = "";

	if ( ! no_gnu && ! for_cpp /* g++ has a cow with this */) {
		unused = " G_GNUC_UNUSED";
	}
	
	if (m->method != SIGNAL_LAST_METHOD &&
	    m->method != SIGNAL_FIRST_METHOD)
		return;

	if (marsh == NULL)
		marsh = g_hash_table_new(NULL, NULL);

	g_assert (m->gtktypes->next != NULL);

	ret_none = strcmp(m->gtktypes->data, "NONE") == 0;
	arglist_none = strcmp(m->gtktypes->next->data, "NONE") == 0;
	
	if (ret_none && arglist_none)
		return;

	/* if we already did a signal prototype just use that */
	mm = find_same_type_signal (m);
	if (mm != NULL) {
		s = g_hash_table_lookup (marsh, mm);
		g_hash_table_insert (marsh, m, s);
		return;
	}

	if (ret_none)
		retcast = NULL;
	else
		retcast = get_cast (m->gtktypes->data, FALSE);
	
	s = g_strdup_printf("Sig%d", sig++);
	
	g_hash_table_insert(marsh, m, s);
	eq_signal_methods = g_list_prepend(eq_signal_methods, m);
	
	/* we know that we'll know all the gtktypes (so get_cast can't fail) */
	out_printf(out, "\ntypedef %s (*___%s) (%s *, ",
		   get_cast(m->gtktypes->data, FALSE), s, typebase);
	
	if ( ! arglist_none) {
		for (li = m->gtktypes->next; li != NULL; li = li->next)
			out_printf (out, "%s, ", get_cast (li->data, FALSE));
	}
	out_printf (out, "gpointer);\n"); 
	
	out_printf (out, "\nstatic void\n"
		    "___marshal_%s (GClosure *closure,\n"
		    "\tGValue *return_value%s,\n"
		    "\tguint n_param_values,\n"
		    "\tconst GValue *param_values,\n"
		    "\tgpointer invocation_hint%s,\n"
		    "\tgpointer marshal_data)\n"
		    "{\n",
		    s,
		    unused,
		    unused);

	if ( ! ret_none)
		out_printf (out, "\t%s v_return;\n", retcast);

	out_printf (out, "\tregister ___%s callback;\n"
		    "\tregister GCClosure *cc = (GCClosure*) closure;\n"
		    "\tregister gpointer data1, data2;\n\n",
		    s);

	out_printf (out, "\tg_return_if_fail (n_param_values == %d);\n\n",
		    arglist_none ? 1 : g_list_length (m->gtktypes));

	out_printf (out,
		    "\tif (G_CCLOSURE_SWAP_DATA (closure)) {\n"
		    "\t\tdata1 = closure->data;\n"
		    "\t\tdata2 = g_value_peek_pointer (param_values + 0);\n"
		    "\t} else {\n"
		    "\t\tdata1 = g_value_peek_pointer (param_values + 0);\n"
		    "\t\tdata2 = closure->data;\n"
		    "\t}\n\n");

	out_printf (out, "\tcallback = (___%s) "
		    "(marshal_data != NULL ? marshal_data : cc->callback);"
		    "\n\n", s);
	
	if (ret_none) {
		out_printf (out, "\tcallback ((%s *)data1", typebase);
	} else {
		out_printf (out, "\tv_return = callback ((%s *)data1",
			    typebase);
	}

	print_signal_marsal_args (m);

	if ( ! ret_none) {
		/* FIXME: This code is so fucking ugly it hurts */
		gboolean take_ownership = 
			(strcmp ((char *)m->gtktypes->data, "STRING") == 0 ||
			 strcmp ((char *)m->gtktypes->data, "BOXED") == 0);
		char *set_func;


		if (strcmp (m->gtktypes->data, "UNICHAR") == 0)
			/* hack because glib is braindamaged */
			set_func = g_strdup ("g_value_set_uint");
		else
			set_func = g_strdup_printf ("g_value_%s_%s",
						    take_ownership ?
						    "take" : "set",
						    (char *)m->gtktypes->data);
		gob_strdown (set_func);

		out_printf (out, "\n\t%s (return_value, v_return);\n",
			    set_func);

		g_free (set_func);
	}
	if (no_gnu || for_cpp /* g++ has a cow with G_GNUC_UNUSED */) {
		if (ret_none)
			out_printf (out, "\n\treturn_value = NULL;\n");
		out_printf (out, "\tinvocation_hint = NULL;\n");

	}
	out_printf (out, "}\n\n");
}

static void
add_enums(Class *c)
{
	GList *li;
	out_printf(out, "\n");
	if(signals>0) {
		out_printf(out, "enum {\n");
		for(li=c->nodes;li;li=g_list_next(li)) {
			Node *n = li->data;
			if(n->type == METHOD_NODE) {
				Method *m = (Method *)n;
				if(m->method == SIGNAL_LAST_METHOD ||
				   m->method == SIGNAL_FIRST_METHOD) {
					char *s = g_strdup(m->id);
					gob_strup(s);
					out_printf(out, "\t%s_SIGNAL,\n", s);
					g_free(s);
				}
			}
		}
		out_printf(out, "\tLAST_SIGNAL\n};\n\n");
	}
	if (set_properties > 0 ||
	    get_properties > 0) {
		out_printf(out, "enum {\n\tPROP_0");
		for(li=c->nodes;li;li=g_list_next(li)) {
			Node *n = li->data;
			if (n->type == PROPERTY_NODE) {
				Property *p = (Property *)n;
				char *s = g_strdup (p->name);
				gob_strup (s);
				out_printf (out, ",\n\tPROP_%s", s);
				g_free(s);
			} else if (n->type == ARGUMENT_NODE) {
				Argument *a = (Argument *)n;
				char *s = g_strdup(a->name);
				gob_strup (s);
				out_printf(out, ",\n\tPROP_%s", s);
				g_free(s);
			}
		}
		out_printf(out, "\n};\n\n");
	}

	if (signals > 0)
		out_printf(out,
			   "static guint object_signals[LAST_SIGNAL] = {0};\n\n");

	out_printf(out, "/* pointer to the class of our parent */\n");
	out_printf(out, "static %sClass *parent_class = NULL;\n\n", ptypebase);
}

static void
add_interface_methods (Class *c, const char *interface)
{
	GList *li;
	gboolean added_line = FALSE;

	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		Method *m = (Method *)n;
		if (n->type != METHOD_NODE ||
		    m->method == OVERRIDE_METHOD ||
		    m->interface == NULL ||
		    strcmp (m->interface, interface) != 0)
			continue;

		if (m->line_no > 0) {
			out_addline_infile (out, m->line_no);
			added_line = TRUE;
		} else if (m->line_no == 0 &&
			   added_line) {
			out_addline_outfile (out);
			added_line = FALSE;
		}
		out_printf (out, "\tiface->%s = self_%s;\n",
			    m->id, m->id);
	}
	if (added_line)
		out_addline_outfile (out);
}

static void
add_interface_inits (Class *c)
{
	GList *li;

	if (c->interfaces == NULL)
		return;

	out_printf(out, "\n");

	for (li = c->interfaces; li != NULL; li = li->next) {
		const char *interface = li->data;
		const char *end;
		char *name = replace_sep (interface, '_');
		char *type = remove_sep (interface);

		if(!gtk3_ok)
		{
			/* EEEK! evil, we should have some sort of option
			 * to force this for arbitrary interfaces, since
			 * some are Class and some are Iface.  Glib is shite
			 * in consistency. */
		
			if (strcmp (type, "GtkEditable") == 0 ||
			    strcmp (type, "GTypePlugin") == 0)
				end = "Class";
			else
				// We'll assume Iface is the standard ending 
				end = "Iface";
			}
		else
		{
			/*GTK3 doesn't need Iface end*/
			end="Interface";
		}
		
		out_printf (out, "\nstatic void\n"
			    "___%s_init (%s%s *iface)\n"
			    "{\n",
			    name, type, end);

		add_interface_methods (c, interface);

		out_printf (out, "}\n\n");

		g_free (name);
		g_free (type);
	}
}

static void
add_interface_infos (void)
{
	GList *li;
	for (li = ((Class *)class)->interfaces;
	     li != NULL;
	     li = li->next) {
		char *name = replace_sep (li->data, '_');
		out_printf (out,
			    "\t\tstatic const GInterfaceInfo %s_info = {\n"
			    "\t\t\t(GInterfaceInitFunc) ___%s_init,\n"
			    "\t\t\tNULL,\n"
			    "\t\t\tNULL\n"
			    "\t\t};\n\n",
			    name, name);
		g_free (name);
	}
}

static void
add_interfaces (void)
{
	GList *li;
	for (li = ((Class *)class)->interfaces;
	     li != NULL;
	     li = li->next) {
		char *name = replace_sep (li->data, '_');
		char *type = make_pre_macro (li->data, "TYPE");

		out_printf (out,
			    "\t\tg_type_add_interface_static (type,\n"
			    "\t\t\t%s,\n"
			    "\t\t\t&%s_info);\n",
			    type,
			    name);

		g_free (type);
		g_free (name);
	}
}

static void
add_get_type(void)
{
	/*char *chunk_size = ((Class*)class)->chunk_size;*/

	out_printf(out,
		   "GType\n"
		   "%s_get_type (void)\n"
		   "{\n"
		   "\tstatic GType type = 0;\n\n"
		   "\tif ___GOB_UNLIKELY(type == 0) {\n"
		   "\t\tstatic const GTypeInfo info = {\n"
		   "\t\t\tsizeof (%sClass),\n"
		   "\t\t\t(GBaseInitFunc) NULL,\n"
		   "\t\t\t(GBaseFinalizeFunc) NULL,\n"
		   "\t\t\t(GClassInitFunc) %s_class_init,\n"
		   "\t\t\t(GClassFinalizeFunc) NULL,\n"
		   "\t\t\tNULL /* class_data */,\n"
		   "\t\t\tsizeof (%s),\n"
		   "\t\t\t%d /* n_preallocs */,\n"
		   "\t\t\t(GInstanceInitFunc) %s_init,\n"
		   "\t\t\tNULL\n"
		   "\t\t};\n\n",
		   funcbase, typebase, funcbase, typebase, prealloc, funcbase);

	add_interface_infos ();

	out_printf (out,
		    "\t\ttype = g_type_register_static (%s, \"%s\", &info, (GTypeFlags)%s);\n",
		    pmacrotype, typebase, ((Class *)class)->abstract ? "G_TYPE_FLAG_ABSTRACT" : "0");

	add_interfaces ();

	/*
	if(chunk_size)  {
		out_printf(out,
			   "#if %s > 0\n"
			   "\t\tgtk_type_set_chunk_alloc(type, %s);\n"
			   "#endif\n", 
			   chunk_size, chunk_size);
	}
	*/
	out_printf(out,
		   "\t}\n\n"
		   "\treturn type;\n"
		   "}\n\n");
}

static void
add_bonobo_object_get_type (void)
{
	/* char *chunk_size = ((Class*)class)->chunk_size; */
	/* _vicious_ spanks seth with a rusty nail
        out_printf(out,
		   "\n#warning \"Bonobo isn't fully ported to glib 2.0 and "
		   "gob2 doesn't officially support it yet. It'd be safer "
                   "and a lot more fun to blow goats.\"\n");
		   */

	out_printf (out,
		    "GType\n"
		    "%s_get_type (void)\n" /* 1 */
		    "{\n"
		    "\tstatic GType type = 0;\n\n"
		    "\tif ___GOB_UNLIKELY(type == 0) {\n"
		    "\t\tstatic const GTypeInfo info = {\n"
		    "\t\t\tsizeof (%sClass),\n" /* 2 */
		    "\t\t\t(GBaseInitFunc) NULL,\n"
		    "\t\t\t(GBaseFinalizeFunc) NULL,\n"
		    "\t\t\t(GClassInitFunc) %s_class_init,\n" /* 3 */
		    "\t\t\tNULL, /* class_finalize */\n"
		    "\t\t\tNULL, /* class_data */\n"
		    "\t\t\tsizeof (%s),\n" /* 4 */
		    "\t\t\t0, /* n_preallocs */\n"
		    "\t\t\t(GInstanceInitFunc)  %s_init,\n" /* 5 */
		    "\t\t\tNULL\n"
		    "\t\t};\n\n",
		    funcbase /* 1 */,
		    typebase /* 2 */,
		    funcbase /* 3 */,
		    typebase /* 4 */,
		    funcbase /* 5 */);

	add_interface_infos ();

	out_printf (out,
		   "\t\ttype = bonobo_type_unique (\n"
		   "\t\t\tBONOBO_OBJECT_TYPE,\n"
		   "\t\t\tPOA_%s__init, NULL,\n" /* 1 */
		   "\t\t\tG_STRUCT_OFFSET (%sClass, _epv),\n" /* 2 */
		   "\t\t\t&info, \"%s\");\n", /* 3 */
		   ((Class*)class)->bonobo_object_class /* 1 */,
		   typebase /* 2 */,
		   typebase /* 3 */);

	add_interfaces ();

	/*if(chunk_size)  {
		out_printf(out,
			   "#if %s > 0\n"
			   "\t\tgtk_type_set_chunk_alloc(type, %s);\n"
			   "#endif\n", 
			   chunk_size, chunk_size);
	}*/
	out_printf(out,
		   "\t}\n\n"
		   "\treturn type;\n"
		   "}\n\n");
}

static void
add_overrides(Class *c, const char *oname,
	      gboolean did_base_obj)
{
	GList *li;
	GHashTable *done;
	char *s;
	
	done = g_hash_table_new (g_str_hash, g_str_equal);
	if (did_base_obj) {
		s = g_strdup ("GObject");
		g_hash_table_insert (done, s, s);
	}
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		char *f;
		Method *m = (Method *)n;
		if(n->type != METHOD_NODE ||
		   m->method != OVERRIDE_METHOD)
			continue;

		s = remove_sep(m->otype);
		
		if(g_hash_table_lookup(done, s)) {
			g_free(s);
			continue;
		}
		g_hash_table_insert(done, s, s);

		f = replace_sep(m->otype, '_');
		gob_strdown(f);

		out_printf(out, "\t%sClass *%s_class = (%sClass *)%s;\n",
			   s, f, s, oname);
		
		g_free(f);
	}
	g_hash_table_foreach (done, (GHFunc)g_free, NULL);
	g_hash_table_destroy (done);
}

static char *
make_run_signal_flags(Method *m, gboolean last)
{
	GList *li;
	GString *gs;
	char *flags[] = {
		"RUN_FIRST",
		"RUN_LAST",
		"RUN_CLEANUP",
		"NO_RECURSE",
		"DETAILED",
		"ACTION",
		"NO_HOOKS",
		NULL
	};

	gs = g_string_new(NULL);

	if(last)
		g_string_assign(gs, "G_SIGNAL_RUN_LAST");
	else
		g_string_assign(gs, "G_SIGNAL_RUN_FIRST");

	if(m->scope == PUBLIC_SCOPE)
		g_string_append(gs, " | G_SIGNAL_ACTION");

	for(li = m->flags; li; li = li->next) {
		char *flag = li->data;
		int i;
		for(i=0;flags[i];i++) {
			if(strcmp(flags[i], flag)==0)
				break;
		}
		/* if we haven't found it in our list */
		if( ! flags[i]) {
			error_printf(GOB_WARN, m->line_no,
				     "Unknown flag '%s' used, "
				     "perhaps it was misspelled",
				     flag);
		}
		g_string_sprintfa(gs, " | G_SIGNAL_%s", flag);
	}

	{
		char *ret = gs->str;
		g_string_free(gs, FALSE);
		return ret;
	}
}
		

static void
add_signals(Class *c)
{
	GList *li;

	out_printf(out, "\n");
	for(li=c->nodes;li;li=g_list_next(li)) {
		Node *n = li->data;
		char *mar, *sig, *flags;
		gboolean is_none, last = FALSE;
		Method *m = (Method *)n;

		if(n->type != METHOD_NODE ||
		   (m->method != SIGNAL_FIRST_METHOD &&
		    m->method != SIGNAL_LAST_METHOD))
			continue;

		if(m->method == SIGNAL_FIRST_METHOD)
			last = FALSE;
		else
			last = TRUE;

		if(g_hash_table_lookup(marsh, m))
			mar = g_strconcat("___marshal_",
					  (char *)g_hash_table_lookup(marsh, m),
					  NULL);
		else
			mar = g_strdup("g_cclosure_marshal_VOID__VOID");
		
		is_none = (strcmp(m->gtktypes->next->data, "NONE")==0);

		sig = g_strdup (m->id);
		gob_strup (sig);
		flags = make_run_signal_flags (m, last);
		out_printf (out, "\tobject_signals[%s_SIGNAL] =\n"
			    "\t\tg_signal_new (%s,\n"
			    "\t\t\tG_TYPE_FROM_CLASS (g_object_class),\n"
			    "\t\t\t(GSignalFlags)(%s),\n"
			    "\t\t\tG_STRUCT_OFFSET (%sClass, %s),\n"
			    "\t\t\tNULL, NULL,\n"
			    "\t\t\t%s,\n"
			    "\t\t\tG_TYPE_%s, %d",
			    sig, m->signal_name /*m->id* if not given signal_name*/,
			    flags,
			    typebase, m->id, mar,
			    (char *)m->gtktypes->data,
			    is_none ? 0 : g_list_length(m->gtktypes->next));
		g_free(mar);
		g_free(sig);
		g_free(flags);
		
		if( ! is_none) {
			GList *l;
			char  *t;
			for(l = m->gtktypes->next; l != NULL; l = l->next) {
				char *str = l->data;
				if (strncmp (str, "BOXED_", 6) == 0)
					t = g_strdup (&(str[6]));
				else
					t = g_strconcat ("G_TYPE_", str, NULL);
				out_printf (out, ",\n\t\t\t%s", t);
				g_free (t);
			}
		}

		out_printf(out, ");\n");

		if(strcmp(m->gtktypes->data, "NONE") != 0 ||
		   ! is_none) {
			GList *gl, *al;
			out_printf(out, "\tif ___GOB_UNLIKELY(");
			if(strcmp(m->gtktypes->data, "NONE") != 0) {
				out_printf(out, "sizeof(");
				print_type(out, m->mtype, FALSE);
				out_printf(out, "%s",
					   m->mtype->postfix ?
					   m->mtype->postfix : ""); 
				out_printf(out, ") != sizeof(%s) || ",
					   get_cast(m->gtktypes->data, FALSE));
			}

			for(al = m->args->next, gl = m->gtktypes->next;
			    al != NULL && gl != NULL;
			    al = al->next, gl = gl->next) {
				FuncArg *arg = al->data;
				char *gtkarg = gl->data;

				out_printf(out, "sizeof(");
				print_type(out, arg->atype, FALSE);
				out_printf(out, "%s",
					   arg->atype->postfix ?
					   arg->atype->postfix : ""); 
				out_printf(out, ") != sizeof(%s) || ",
					   get_cast(gtkarg, FALSE));
			}

			out_printf (out,
				    "parent_class == NULL /* avoid warning */");

			out_printf(out, ") {\n"
				   "\t\tg_error(\"%s line %d: Type mismatch "
				   "of \\\"%s\\\" signal signature\");\n"
				   "\t}\n",
				   filename, m->line_no, m->id);

		}
	}
}

static void
set_def_handlers(Class *c, const char *oname)
{
	GList *li;
	gboolean set_line = FALSE;

	out_printf(out, "\n");
	for(li = c->nodes; li; li = g_list_next(li)) {
		Node *n = li->data;
		Method *m = (Method *)n;

		if(n->type != METHOD_NODE ||
		   (m->method != SIGNAL_FIRST_METHOD &&
		    m->method != SIGNAL_LAST_METHOD &&
		    m->method != VIRTUAL_METHOD &&
		    m->method != OVERRIDE_METHOD))
			continue;

		if(m->line_no > 0 && m->cbuf) {
			out_addline_infile(out, m->line_no);
			set_line = TRUE;
		} else if(set_line) {
			out_addline_outfile(out);
			set_line = FALSE;
		}


		if (m->method == OVERRIDE_METHOD) {
			char *s;
			s = replace_sep (m->otype, '_');
			gob_strdown (s);

			if (need_dispose &&
			    dispose_handler != NULL &&
			    strcmp (m->id, "dispose") == 0)
				out_printf (out, "\tg_object_class->dispose "
					    "= ___dispose;\n");
			else if (need_finalize &&
				finalize_handler &&
				strcmp(m->id, "finalize") == 0)
				out_printf(out,
					   "\tg_object_class->finalize = ___finalize;\n");
			else if (m->cbuf != NULL)
				out_printf(out,
					   "\t%s_class->%s = ___%x_%s_%s;\n",
					   s, m->id, (guint)m->unique_id,
					   funcbase, m->id);
			else
				out_printf(out, "\t%s_class->%s = NULL;\n",
					   s, m->id);
		} else {
			if(m->cbuf)
				out_printf(out, "\t%s->%s = ___real_%s_%s;\n",
					   oname, m->id,
					   funcbase, m->id);
			else
				out_printf(out, "\t%s->%s = NULL;\n",
					   oname, m->id);
		}
	}
	if(set_line)
		out_addline_outfile(out);
}

static void
make_argument (Argument *a)
{
	GString *flags;
	GList *l;
	char *s;
	char *argflags[] = {
		"CONSTRUCT",
		"CONSTRUCT_ONLY",
		"LAX_VALIDATION",
		"PRIVATE",
		NULL
	};

	flags = g_string_new ("(GParamFlags)(");

	if(a->get && a->set)
		g_string_append (flags, "G_PARAM_READABLE | G_PARAM_WRITABLE");
	else if(a->get)
		g_string_append (flags, "G_PARAM_READABLE");
	else
		g_string_append (flags, "G_PARAM_WRITABLE");

	g_assert(a->get || a->set);

	for (l = a->flags; l != NULL; l = l->next) {
		char *flag = l->data;
		int i;
		if(strcmp (flag, "READABLE") == 0 ||
		   strcmp (flag, "WRITABLE") == 0) {
			error_print(GOB_WARN, a->line_no,
				    "READABLE and "
				    "WRITABLE argument flags are "
				    "set automatically");
			continue;
		}
		for(i = 0; argflags[i]; i++) {
			if(strcmp(argflags[i], flag)==0)
				break;
		}
		g_string_sprintfa(flags, " | %s%s", argflags[i] ? "G_PARAM_" : "", flag);
	}

	g_string_append (flags, ")");

	s = g_strdup(a->name);
	gob_strup (s);
	if (!strcmp (a->gtktype, "ENUM"))
		out_printf(out, "\tparam_spec = g_param_spec_enum (\"%s\", NULL, NULL,\n"
			   "\t\tG_TYPE_ENUM, 0,\n"
			   "\t\t%s);\n",
			   a->name, flags->str);
	if (!strcmp (a->gtktype, "FLAGS"))
		out_printf(out, "\tparam_spec = g_param_spec_flags (\"%s\", NULL, NULL,\n"
			   "\t\tG_TYPE_FLAGS, 0,\n"
			   "\t\t%s);\n",
			   a->name, flags->str);
	else if (!strcmp (a->gtktype, "OBJECT"))
		out_printf(out, "\tparam_spec = g_param_spec_object (\"%s\", NULL, NULL,\n"
			   "\t\tG_TYPE_OBJECT,\n"
			   "\t\t%s);\n",
			   a->name, flags->str);
	else if (!strcmp (a->gtktype, "STRING"))
		out_printf(out, "\tparam_spec = g_param_spec_string (\"%s\", NULL, NULL,\n"
			   "\t\tNULL,\n"
			   "\t\t%s);\n",
			   a->name, flags->str);
	else if (!strcmp (a->gtktype, "INT"))
		out_printf(out, "\tparam_spec = g_param_spec_int (\"%s\", NULL, NULL,\n"
			   "\t\tG_MININT, G_MAXINT,\n"
			   "\t\t0,\n"
			   "\t\t%s);\n",
			   a->name, flags->str);
	else if (!strcmp (a->gtktype, "UINT"))
		out_printf(out, "\tparam_spec = g_param_spec_uint (\"%s\", NULL, NULL,\n"
			   "\t\t0, G_MAXUINT,\n"
			   "\t\t0,\n"
			   "\t\t%s);\n",
			   a->name, flags->str);
	else if (!strcmp (a->gtktype, "INT"))
		out_printf(out, "\tparam_spec = g_param_spec_int (\"%s\", NULL, NULL,\n"
			   "\t\tG_MININT, G_MAXINT,\n"
			   "\t\t0,\n"
			   "\t\t%s);\n",
			   a->name, flags->str);
	else if (!strcmp (a->gtktype, "CHAR"))
		out_printf(out, "\tparam_spec = g_param_spec_char (\"%s\", NULL, NULL,\n"
			   "\t\t-128, 127,\n"
			   "\t\t0,\n"
			   "\t\t%s);\n",
			   a->name, flags->str);
	else if (!strcmp (a->gtktype, "UCHAR"))
		out_printf(out, "\tparam_spec = g_param_spec_uchar (\"%s\", NULL, NULL,\n"
			   "\t\t0, 0xFF,\n"
			   "\t\t0,\n"
			   "\t\t%s);\n",
			   a->name, flags->str);
	else if (!strcmp (a->gtktype, "BOOL") ||
		 !strcmp (a->gtktype, "BOOLEAN"))
		out_printf(out, "\tparam_spec = g_param_spec_boolean (\"%s\", NULL, NULL,\n"
			   "\t\tFALSE,\n"
			   "\t\t%s);\n",
			   a->name, flags->str);
	else if (!strcmp (a->gtktype, "LONG"))
		out_printf(out, "\tparam_spec = g_param_spec_long (\"%s\", NULL, NULL,\n"
			   "\t\tG_MINLONG, G_MAXLONG,\n"
			   "\t\t0,\n"
			   "\t\t%s);\n",
			   a->name, flags->str);
	else if (!strcmp (a->gtktype, "ULONG"))
		out_printf(out, "\tparam_spec = g_param_spec_ulong (\"%s\", NULL, NULL,\n"
			   "\t\t0, G_MAXULONG,\n"
			   "\t\t0,\n"
			   "\t\t%s);\n",
			   a->name, flags->str);
	else if (!strcmp (a->gtktype, "INT64"))
		out_printf(out, "\tparam_spec = g_param_spec_int64 (\"%s\", NULL, NULL,\n"
			   "\t\tG_MININT64, G_MAXINT64,\n"
			   "\t\t0,\n"
			   "\t\t%s);\n",
			   a->name, flags->str);
	else if (!strcmp (a->gtktype, "UINT64"))
		out_printf(out, "\tparam_spec = g_param_spec_uint64 (\"%s\", NULL, NULL,\n"
			   "\t\t0, G_MAXUINT64,\n"
			   "\t\t0,\n"
			   "\t\t%s);\n",
			   a->name, flags->str);
	else if (!strcmp (a->gtktype, "FLOAT"))
		out_printf(out, "\tparam_spec = g_param_spec_float (\"%s\", NULL, NULL,\n"
			   "\t\t-G_MAXFLOAT, G_MAXFLOAT,\n"
			   "\t\t0,\n"
			   "\t\t%s);\n",
			   a->name, flags->str);
	else if (!strcmp (a->gtktype, "DOUBLE"))
		out_printf(out, "\tparam_spec = g_param_spec_double (\"%s\", NULL, NULL,\n"
			   "\t\t-G_MAXDOUBLE, G_MAXDOUBLE,\n"
			   "\t\t0,\n"
			   "\t\t%s);\n",
			   a->name, flags->str);
	else if (!strcmp (a->gtktype, "POINTER"))
		out_printf(out, "\tparam_spec = g_param_spec_pointer (\"%s\", NULL, NULL,\n"
			   "\t\t%s);\n",
			   a->name, flags->str);
	else
		error_printf (GOB_ERROR, a->line_no,
			      "%s type is not supported for arguments, try using properties",
			      a->gtktype);

	out_printf(out, "\tg_object_class_install_property (g_object_class,\n"
		   "\t\tPROP_%s, param_spec);\n", s);


	g_free(s);
	g_string_free(flags, TRUE);
}

#define value_for_print(str, alt) (str != NULL ? str : alt)

static void
make_property (Property *p)
{
	char *s;

	if (p->get == NULL && p->set == NULL) {
		error_print (GOB_ERROR, p->line_no,
			     "Property has no getter nor setter");
	}

	if (p->override) {
		if (p->flags != NULL)
			error_print (GOB_WARN, p->line_no,
				     "Overridden property, flags ignored");
		if (p->nick != NULL)
			error_print (GOB_WARN, p->line_no,
				     "Overridden property, nick ignored");
		if (p->blurb != NULL)
			error_print (GOB_WARN, p->line_no,
				     "Overridden property, blurb ignored");
		if (p->minimum != NULL)
			error_print (GOB_WARN, p->line_no,
				     "Overridden property, minimum ignored");
		if (p->maximum != NULL)
			error_print (GOB_WARN, p->line_no,
				     "Overridden property, maximum ignored");
		if (p->default_value != NULL)
			error_print (GOB_WARN, p->line_no,
				     "Overridden property, default_value ignored");

		s = g_strdup (p->name);
		gob_strup (s);
		out_printf (out, "\tg_object_class_override_property (g_object_class,\n"
			    "\t\tPROP_%s,\n"
			    "\t\t\"%s\");\n", s, value_for_print (p->canonical_name, p->name) );
		g_free (s);
	} else {
		GString *flags;
		GList *l;
		char *argflags[] = {
			"CONSTRUCT",
			"CONSTRUCT_ONLY",
			"LAX_VALIDATION",
			"PRIVATE",
			NULL
		};

		flags = g_string_new ("(GParamFlags)(");

		if (p->get != NULL && p->set != NULL)
			g_string_append (flags, "G_PARAM_READABLE | G_PARAM_WRITABLE");
		else if (p->get != NULL)
			g_string_append (flags, "G_PARAM_READABLE");
		else
			g_string_append (flags, "G_PARAM_WRITABLE");


		for (l = p->flags; l != NULL; l = l->next) {
			char *flag = l->data;
			int i;
			if(strcmp (flag, "READABLE") == 0 ||
			   strcmp (flag, "WRITABLE") == 0) {
				error_print(GOB_WARN, p->line_no,
					    "READABLE and "
					    "WRITABLE argument flags are "
					    "set automatically");
				continue;
			}
			for(i = 0; argflags[i]; i++) {
				if(strcmp(argflags[i], flag)==0)
					break;
			}
			g_string_sprintfa(flags, " | %s%s", argflags[i] ? "G_PARAM_" : "", flag);
		}

		g_string_append (flags, ")");

		if (strcmp (p->gtktype, "CHAR") == 0) {
			out_printf (out, "\tparam_spec = g_param_spec_char\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* minimum */,\n"
				    "\t\t %s /* maximum */,\n"
				    "\t\t %s /* default_value */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    value_for_print (p->minimum, "-128"),
				    value_for_print (p->maximum, "127"),
				    value_for_print (p->default_value, "0"),
				    flags->str);
		} else if (strcmp (p->gtktype, "UCHAR") == 0) {
			out_printf (out, "\tparam_spec = g_param_spec_uchar\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* minimum */,\n"
				    "\t\t %s /* maximum */,\n"
				    "\t\t %s /* default_value */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    value_for_print (p->minimum, "0"),
				    value_for_print (p->maximum, "0xFF"),
				    value_for_print (p->default_value, "0"),
				    flags->str);
		} else if (strcmp (p->gtktype, "BOOLEAN") == 0) {
			out_printf (out, "\tparam_spec = g_param_spec_boolean\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* default_value */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    value_for_print (p->default_value, "FALSE"),
				    flags->str);
		} else if (strcmp (p->gtktype, "INT") == 0) {
			out_printf (out, "\tparam_spec = g_param_spec_int\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* minimum */,\n"
				    "\t\t %s /* maximum */,\n"
				    "\t\t %s /* default_value */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    value_for_print (p->minimum, "G_MININT"),
				    value_for_print (p->maximum, "G_MAXINT"),
				    value_for_print (p->default_value, "0"),
				    flags->str);
		} else if (strcmp (p->gtktype, "UINT") == 0) {
			out_printf (out, "\tparam_spec = g_param_spec_uint\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* minimum */,\n"
				    "\t\t %s /* maximum */,\n"
				    "\t\t %s /* default_value */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    value_for_print (p->minimum, "0"),
				    value_for_print (p->maximum, "G_MAXUINT"),
				    value_for_print (p->default_value, "0"),
				    flags->str);
		} else if (strcmp (p->gtktype, "LONG") == 0) {
			out_printf (out, "\tparam_spec = g_param_spec_long\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* minimum */,\n"
				    "\t\t %s /* maximum */,\n"
				    "\t\t %s /* default_value */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    value_for_print (p->minimum, "G_MINLONG"),
				    value_for_print (p->maximum, "G_MAXLONG"),
				    value_for_print (p->default_value, "0"),
				    flags->str);
		} else if (strcmp (p->gtktype, "ULONG") == 0) {
			out_printf (out, "\tparam_spec = g_param_spec_ulong\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* minimum */,\n"
				    "\t\t %s /* maximum */,\n"
				    "\t\t %s /* default_value */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    value_for_print (p->minimum, "0"),
				    value_for_print (p->maximum, "G_MAXULONG"),
				    value_for_print (p->default_value, "0"),
				    flags->str);
		} else if (strcmp (p->gtktype, "INT64") == 0) {
			out_printf (out, "\tparam_spec = g_param_spec_int64\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* minimum */,\n"
				    "\t\t %s /* maximum */,\n"
				    "\t\t %s /* default_value */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    value_for_print (p->minimum, "G_MININT64"),
				    value_for_print (p->maximum, "G_MAXINT64"),
				    value_for_print (p->default_value, "0"),
				    flags->str);
		} else if (strcmp (p->gtktype, "UINT64") == 0) {
			out_printf (out, "\tparam_spec = g_param_spec_uint64\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* minimum */,\n"
				    "\t\t %s /* maximum */,\n"
				    "\t\t %s /* default_value */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    value_for_print (p->minimum, "0"),
				    value_for_print (p->maximum, "G_MAXUINT64"),
				    value_for_print (p->default_value, "0"),
				    flags->str);
		} else if (strcmp (p->gtktype, "UNICHAR") == 0) {
			out_printf (out, "\tparam_spec = g_param_spec_unichar\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* default_value */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    value_for_print (p->default_value, "0"),
				    flags->str);
		} else if (strcmp (p->gtktype, "ENUM") == 0) {
			char *type = make_me_type (p->extra_gtktype,
						   "G_TYPE_ENUM");
			out_printf (out, "\tparam_spec = g_param_spec_enum\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* enum_type */,\n"
				    "\t\t %s /* default_value */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    type,
				    value_for_print (p->default_value, "0"),
				    flags->str);
			g_free (type);
		} else if (strcmp (p->gtktype, "FLAGS") == 0) {
			char *type = make_me_type (p->extra_gtktype,
						   "G_TYPE_FLAGS");
			out_printf (out, "\tparam_spec = g_param_spec_flags\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* flags_type */,\n"
				    "\t\t %s /* default_value */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    type,
				    value_for_print (p->default_value, "0"),
				    flags->str);
			g_free (type);
		} else if (strcmp (p->gtktype, "FLOAT") == 0) {
			out_printf (out, "\tparam_spec = g_param_spec_float\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* minimum */,\n"
				    "\t\t %s /* maximum */,\n"
				    "\t\t %s /* default_value */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    value_for_print (p->minimum, "-G_MAXFLOAT"),
				    value_for_print (p->maximum, "G_MAXFLOAT"),
				    value_for_print (p->default_value, "0.0"),
				    flags->str);
		} else if (strcmp (p->gtktype, "DOUBLE") == 0) {
			out_printf (out, "\tparam_spec = g_param_spec_double\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* minimum */,\n"
				    "\t\t %s /* maximum */,\n"
				    "\t\t %s /* default_value */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    value_for_print (p->minimum, "-G_MAXDOUBLE"),
				    value_for_print (p->maximum, "G_MAXDOUBLE"),
				    value_for_print (p->default_value, "0.0"),
				    flags->str);
		} else if (strcmp (p->gtktype, "STRING") == 0) {
			out_printf (out, "\tparam_spec = g_param_spec_string\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* default_value */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    value_for_print (p->default_value, "NULL"),
				    flags->str);
		} else if (strcmp (p->gtktype, "PARAM") == 0) {
			char *type = make_me_type (p->extra_gtktype,
						   "G_TYPE_PARAM");
			out_printf (out, "\tparam_spec = g_param_spec_param\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* param_type */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    type,
				    flags->str);
			g_free (type);
		} else if (strcmp (p->gtktype, "BOXED") == 0) {
			char *type = make_me_type (p->extra_gtktype,
						   "G_TYPE_BOXED");
			out_printf (out, "\tparam_spec = g_param_spec_boxed\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* boxed_type */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    type,
				    flags->str);
			g_free (type);
		} else if (strcmp (p->gtktype, "POINTER") == 0) {
			out_printf (out, "\tparam_spec = g_param_spec_pointer\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    flags->str);
		/* FIXME: VALUE_ARRAY */
		} else if (strcmp (p->gtktype, "CLOSURE") == 0) {
			out_printf (out, "\tparam_spec = g_param_spec_pointer\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    flags->str);
		} else if (strcmp (p->gtktype, "OBJECT") == 0) {
			char *type = make_me_type (p->extra_gtktype,
						   "G_TYPE_BOXED");
			out_printf (out, "\tparam_spec = g_param_spec_object\n"
				    "\t\t(\"%s\" /* name */,\n"
				    "\t\t %s /* nick */,\n"
				    "\t\t %s /* blurb */,\n"
				    "\t\t %s /* object_type */,\n"
				    "\t\t %s);\n",
				    value_for_print (p->canonical_name, p->name),
				    value_for_print (p->nick, "NULL"),
				    value_for_print (p->blurb, "NULL"),
				    type,
				    flags->str);
			g_free (type);
		} else {
			error_printf (GOB_ERROR, p->line_no,
				      "%s type is not supported by properties",
				      p->gtktype);
		}

		s = g_strdup (p->name);
		gob_strup (s);
		out_printf (out, "\tg_object_class_install_property (g_object_class,\n"
			    "\t\tPROP_%s,\n"
			    "\t\tparam_spec);\n", s);
		g_free (s);

		g_string_free (flags, TRUE);
	}
}

static void
make_arguments(Class *c)
{
	GList *li;
	if (get_properties > 0)
		out_printf(out, "\tg_object_class->get_property = ___object_get_property;\n");
	if (set_properties > 0)
		out_printf(out, "\tg_object_class->set_property = ___object_set_property;\n");
	out_printf (out, "    {\n");
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if ((n->type == PROPERTY_NODE && ! ((Property *) n)->override)
		    || n->type == ARGUMENT_NODE) {
			out_printf(out, "\tGParamSpec   *param_spec;\n\n");
			break;
		}
	}

	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if (n->type == PROPERTY_NODE)
			make_property ((Property *)n);
		else if (n->type == ARGUMENT_NODE)
			make_argument ((Argument *)n);
	}
	out_printf(out, "    }\n");
}

static void
print_initializer(Method *m, Variable *v)
{
	char *root;
	
	if(v->glade_widget)
		return;

	if(v->initializer == NULL)
		return;

	if(v->scope == PRIVATE_SCOPE)
		root = g_strconcat(((FuncArg *)m->args->data)->name,
				   "->_priv", NULL);
	else
		root = g_strdup(((FuncArg *)m->args->data)->name);

	if(v->initializer_line > 0)
		out_addline_infile(out, v->initializer_line);

	if (v->initializer_simple)
		out_printf(out, "\t%s->%s = %s;\n",
		   root, v->id, v->initializer);
	else if (strcmp(v->id, "_glade_xml") == 0)
		/* This is OK, this v->initializer string is set internally
		   and it will eat exactly one string! */
		out_printf(out,v->initializer, ((FuncArg *)m->args->data)->name);
	else
		out_printf(out, "%s", v->initializer);

	if(v->initializer_line > 0)
		out_addline_outfile(out);

	g_free(root);
}

static void
print_glade_widget(Method *m, Variable *v)
{
	char *root;
  char *cast;
	
	if(!v->glade_widget)
		return;

	if(v->scope == PRIVATE_SCOPE)
		root = g_strconcat(((FuncArg *)m->args->data)->name,
				   "->_priv", NULL);
	else
		root = g_strdup(((FuncArg *)m->args->data)->name);

	cast = get_type(v->vtype, FALSE);
	out_printf(out, "\t%s->%s = (%s)glade_xml_get_widget(%s->_glade_xml, \"%s\");\n",
		   root, v->id, cast, root, v->id);

	g_free(root);
}
	
static void
print_destructor (Variable *v)
{
	const char *root;

	if(v->destructor == NULL)
		return;

	if(v->scope == PRIVATE_SCOPE)
		root = "self->_priv";
	else
		root = "self";

	if(v->destructor_simple) {
		if(v->destructor_line > 0)
			out_addline_infile(out, v->destructor_line);

		if (for_cpp) {
			out_printf(out, "\tif(%s->%s) { "
				   "(reinterpret_cast<void (*)(void *)>(%s)) ((gpointer)%s->%s); "
				   "%s->%s = NULL; }\n",
				   root, v->id, v->destructor, root, v->id,
				   root, v->id);
		} else {
			out_printf(out, "\tif(%s->%s) { "
				   "%s ((gpointer) %s->%s); "
				   "%s->%s = NULL; }\n",
				   root, v->id, v->destructor, root, v->id,
				   root, v->id);
		}

		if(v->destructor_line > 0)
			out_addline_outfile(out);
	} else {
		out_printf(out, "#define %s (%s->%s)\n", v->id, root, v->id);
		out_printf(out, "#define VAR %s\n", v->id);
		out_printf(out, "\t{\n");
		if(v->destructor_line > 0)
			out_addline_infile(out, v->destructor_line);

		out_printf(out, "\t%s}\n", v->destructor);

		if(v->destructor_line > 0)
			out_addline_outfile(out);
		out_printf(out, "\tmemset(&(%s), 0, sizeof(%s));\n",
			   v->id, v->id);
		out_printf(out, "#undef VAR\n");
		out_printf(out, "#undef %s\n", v->id);
	}
}

static void
add_constructor (Class *c)
{
	out_printf(out, "\nstatic GObject *\n"
		   "___constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties)\n"
		   "{\n");
	out_printf(out,
		   "#define __GOB_FUNCTION__ \"%s::constructor\"\n",
		   c->otype);

	out_printf(out, "\tGObject *obj_self;\n");
	out_printf(out, "\t%s *self;\n", typebase);

	out_printf(out, "\tobj_self = G_OBJECT_CLASS (parent_class)->constructor (type, n_construct_properties, construct_properties);\n");
	out_printf(out, "\tself = %s (obj_self);\n", macrobase);

	if (user_constructor->line_no > 0)
		out_addline_infile (out, user_constructor->line_no);
	out_printf (out, "\t%s_constructor (self);\n", funcbase);
	if (user_constructor->line_no > 0)
		out_addline_outfile (out);

	out_printf(out, "\treturn obj_self;\n");
	out_printf(out, "}\n"
		   "#undef __GOB_FUNCTION__\n\n");
}

static void
print_unreftors (Class *c)
{
	GList *li;
	for(li = ((Class *)class)->nodes;
	    li != NULL;
	    li = li->next) {
		Node *n = li->data;
		Variable *v = (Variable *)n;
		if (n->type == VARIABLE_NODE &&
		    v->scope != CLASS_SCOPE &&
		    v->destructor_unref)
			print_destructor (v);
	}
}

static void
add_dispose (Class *c)
{
	out_printf(out, "\nstatic void\n"
		   "___dispose (GObject *obj_self)\n"
		   "{\n");
	out_printf(out,
		   "#define __GOB_FUNCTION__ \"%s::dispose\"\n",
		   c->otype);

	if (unreftors > 0 || user_dispose_method != NULL) {
		out_printf (out, "\t%s *self%s = %s (obj_self);\n",
			    typebase,
			    ! no_gnu ? " G_GNUC_UNUSED" : "",
			    macrobase);
	}

	if (dispose_handler != NULL) {
		if (unreftors > 0) {
			print_unreftors (c);
		}

		/* so we get possible bad argument warning */
		if (dispose_handler->line_no > 0)
			out_addline_infile (out, dispose_handler->line_no);
		out_printf (out, "\t___%x_%s_dispose(obj_self);\n",
			    (guint)dispose_handler->unique_id, funcbase);
		if (dispose_handler->line_no > 0)
			out_addline_outfile (out);
	} else {
		if (user_dispose_method != NULL) {
			if (user_dispose_method->line_no > 0)
				out_addline_infile (out, user_dispose_method->line_no);
			out_printf (out, "\t%s_dispose (self);\n", funcbase);
			if (user_dispose_method->line_no > 0)
				out_addline_outfile (out);
		}

		if (unreftors > 0) {
			print_unreftors (c);
		}

		out_printf (out,
			    "\tif (G_OBJECT_CLASS (parent_class)->dispose) \\\n"
			    "\t\t(* G_OBJECT_CLASS (parent_class)->dispose) (obj_self);\n");
	}

	out_printf(out, "}\n"
		   "#undef __GOB_FUNCTION__\n\n");
}

static void
print_destructors (Class *c)
{
	GList *li;
	for (li = ((Class *)class)->nodes;
	     li != NULL;
	     li = li->next) {
		Node *n = li->data;
		Variable *v = (Variable *)n;
		if (n->type == VARIABLE_NODE &&
		    v->scope != CLASS_SCOPE &&
		    ! v->destructor_unref)
			print_destructor (v);
	}
}

static void
add_finalize (Class *c)
{
	out_printf(out,
		   "\nstatic void\n"
		   "___finalize(GObject *obj_self)\n"
		   "{\n");
	out_printf(out,
		   "#define __GOB_FUNCTION__ \"%s::finalize\"\n",
		   c->otype);

	if (privates > 0 ||
	    destructors > 0 ||
	    user_finalize_method != NULL) {
		const char *unused = "";
		if ( ! no_gnu)
			unused = " G_GNUC_UNUSED";
		out_printf(out, "\t%s *self%s = %s (obj_self);\n",
			   typebase, unused, macrobase);
	}
	if (privates > 0) {
		const char *unused = "";
		if ( ! no_gnu)
			unused = " G_GNUC_UNUSED";
		out_printf(out, "\tgpointer priv%s = self->_priv;\n",
			   unused);
	}

	if(finalize_handler) {
		if (destructors > 0) {
			print_destructors (c);
		}

		/* so we get possible bad argument warning */
		if(finalize_handler->line_no > 0)
			out_addline_infile(out, finalize_handler->line_no);
		out_printf(out, "\t___%x_%s_finalize(obj_self);\n",
			   (guint)finalize_handler->unique_id, funcbase);
		if(finalize_handler->line_no > 0)
			out_addline_outfile(out);
	} else {
		if (user_finalize_method != NULL) {
			if (user_finalize_method->line_no > 0)
				out_addline_infile (out, user_finalize_method->line_no);
			out_printf (out, "\t%s_finalize (self);\n", funcbase);
			if (user_finalize_method->line_no > 0)
				out_addline_outfile (out);
		}

		if (destructors > 0) {
			print_destructors (c);
		}

		out_printf(out,
			   "\tif(G_OBJECT_CLASS(parent_class)->finalize) \\\n"
			   "\t\t(* G_OBJECT_CLASS(parent_class)->finalize)(obj_self);\n");
	}

	out_printf(out, "}\n"
		   "#undef __GOB_FUNCTION__\n\n");
}

static void
make_bonobo_object_epv (Class *c, const char *classname)
{
	GList *li;
	gboolean added_line = FALSE;

	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		Method *m = (Method *)n;
		if(n->type != METHOD_NODE ||
		   m->method == OVERRIDE_METHOD)
			continue;

		if (m->bonobo_object_func) {
			if(m->line_no > 0) {
				out_addline_infile(out, m->line_no);
				added_line = TRUE;
			} else if (m->line_no == 0 &&
				   added_line) {
				out_addline_outfile(out);
				added_line = FALSE;
			}
			out_printf (out, "\t%s->_epv.%s = self_%s;\n",
				    classname, m->id, m->id);
		}
	}
	if (added_line)
		out_addline_outfile(out);
}

static void
add_inits(Class *c)
{
	const char *unused = "";
	GList *li;

	if ( ! no_gnu)
		unused = " G_GNUC_UNUSED";

	for(li=c->nodes;li;li=g_list_next(li)) {
		Node *n = li->data;
		Method *m;

		if(n->type != METHOD_NODE)
			continue;
		m = (Method *)n;
		if(m->method == INIT_METHOD) {
			if(m->line_no > 0)
				out_addline_infile(out, m->line_no);
			print_method(out, "static ", "\n", "", " ", "", "\n",
				     m, FALSE, FALSE, FALSE, TRUE, TRUE,
				     FALSE);
			out_printf(out, "{\n");
			if(m->line_no > 0)
				out_addline_outfile(out);
			out_printf(out,
				   "#define __GOB_FUNCTION__ \"%s::init\"\n",
				   c->otype);
			if (privates > 0) {
				out_printf(out, "\t%s->_priv = "
						"G_TYPE_INSTANCE_GET_PRIVATE(%s,%s,%sPrivate);\n",
						((FuncArg *)m->args->data)->name,
						((FuncArg *)m->args->data)->name,
						macrotype,
						typebase);
			} else if(always_private_struct) {
				out_printf(out, "\t%s->_priv = NULL;\n",
					   ((FuncArg *)m->args->data)->name);
			}
			if(initializers > 0) {
				GList *li;
				for(li = ((Class *)class)->nodes;
				    li != NULL;
				    li = li->next) {
					Node *n = li->data;
					Variable *v = (Variable *)n;
					if(n->type != VARIABLE_NODE ||
					   v->scope == CLASS_SCOPE)
						continue;
					print_initializer(m, v);
				}
			}
			if(glade_widgets > 0) {
				GList *li;
				for(li = ((Class *)class)->nodes;
				    li != NULL;
				    li = li->next) {
					Node *n = li->data;
					Variable *v = (Variable *)n;
					if(n->type != VARIABLE_NODE ||
					   v->scope == CLASS_SCOPE)
						continue;
					print_glade_widget(m, v);
				}
			}
		} else if(m->method == CLASS_INIT_METHOD) {
			gboolean did_base_obj = FALSE;

			if(m->line_no > 0)
				out_addline_infile(out, m->line_no);
			print_method(out, "static ", "\n", "", " ", "", "\n",
				     m, FALSE, FALSE, FALSE, TRUE, TRUE,
				     FALSE);
			out_printf(out, "{\n");
			if(m->line_no > 0)
				out_addline_outfile(out);
			out_printf(out,
				   "#define __GOB_FUNCTION__ \"%s::class_init\"\n",
				   c->otype);
			if (set_properties > 0 ||
			    get_properties > 0 ||
			    signals > 0 ||
			    need_constructor ||
			    need_dispose ||
			    need_finalize) {
				out_printf(out,
					   "\tGObjectClass *"
					   "g_object_class%s = "
					   "(GObjectClass*) %s;\n",
					   unused,
					   ((FuncArg *)m->args->data)->name);
				did_base_obj = TRUE;
			}

			if (overrides > 0)
				add_overrides (c,
					       ((FuncArg *)m->args->data)->name,
					       did_base_obj);

			if (privates > 0)
				out_printf (out,
					    "\n\tg_type_class_add_private(%s,sizeof(%sPrivate));\n",
					    ((FuncArg *)m->args->data)->name,
					    typebase);

			if (initializers > 0) {
				GList *li;
				for(li = ((Class *)class)->nodes;
				    li != NULL;
				    li = li->next) {
					Node *n = li->data;
					Variable *v = (Variable *)n;
					if(n->type == VARIABLE_NODE &&
					   v->scope == CLASS_SCOPE)
						print_initializer(m, v);
				}
			}
			
			out_printf(out, "\n\tparent_class = ");
			if(for_cpp)
				out_printf(out, "(%sClass *)", ptypebase);
			out_printf(out, "g_type_class_ref (%s);\n",
				   pmacrotype);

			if(signals > 0)
				add_signals(c);

			set_def_handlers(c, ((FuncArg *)m->args->data)->name);

			/* if there are no handlers for these things, we
			 * need to set them up here */
			if(need_constructor)
				out_printf(out, "\tg_object_class->constructor "
					   "= ___constructor;\n");
			if(need_dispose && !dispose_handler)
				out_printf(out, "\tg_object_class->dispose "
					   "= ___dispose;\n");
			if(need_finalize && !finalize_handler)
				out_printf(out, "\tg_object_class->finalize = "
					   "___finalize;\n");
			
			if(get_properties > 0 || set_properties > 0)
				make_arguments(c);

			if (c->bonobo_object_class != NULL) {
				make_bonobo_object_epv (c, ((FuncArg *)m->args->data)->name);
			}
		} else
			continue;

		if(m->cbuf) {
			out_printf(out, " {\n");
			out_addline_infile(out, m->ccode_line);
			out_printf(out, "%s\n", m->cbuf);
			out_addline_outfile(out);
			out_printf(out, " }\n");
		}
		out_printf(out, "}\n"
			   "#undef __GOB_FUNCTION__\n");
	}
}

static void
add_argument (Argument *a, gboolean is_set)
{
	char *s;
	char *cbuf;
	char *the_type_lower;
	int line_no;

	if(is_set) {
		cbuf = a->set;
		line_no = a->set_line;
	} else {
		cbuf = a->get;
		line_no = a->get_line;
	}
	if (cbuf == NULL)
		return;
	s = g_strdup(a->name);
	gob_strup (s);
	out_printf(out, "\tcase PROP_%s:\n\t{", s);

	the_type_lower = g_strdup (a->gtktype);
	gob_strdown (the_type_lower);

	/* HACK because there is no g_value_set/get for unichar */
	if (strcmp (the_type_lower, "unichar") == 0) {
		g_free (the_type_lower);
		the_type_lower = g_strdup ("uint");
	}

	if (is_set) {
		char *cast;
		const char *unused = "";

		if ( ! no_gnu && ! for_cpp /* g++ has a cow with this */) {
			unused = " G_GNUC_UNUSED";
		}

		if (a->atype != NULL &&
		    /* gcc -Wbad-function-cast is wanking stupid, moronic
		       and otherwise evil so we should just use a (gint)
		       or (guint) cast, not the specific type cast */
		    (for_cpp ||
		     (strcmp (a->gtktype, "ENUM") != 0 &&
		     strcmp (a->gtktype, "FLAGS") != 0)))
			cast = get_type (a->atype, TRUE);
		else
			cast = g_strdup (get_cast (a->gtktype, FALSE));

		out_printf (out, "\t%s ARG%s = (%s) g_value_get_%s (VAL);\n",
			    cast, unused, cast, the_type_lower);

		g_free (cast);
	} else if ( ! is_set) {
		char *cast;

		if (a->atype != NULL)
			cast = get_type (a->atype, TRUE);
		else
			cast = g_strdup (get_cast (a->gtktype, FALSE));
		out_printf (out, "\t%s ARG;\n"
			    "\tmemset (&ARG, 0, sizeof (%s));\n",
			    cast, cast);

		g_free(cast);
	}
	g_free (s);
	out_printf(out, "\t\t{\n");
	if (line_no > 0)
		out_addline_infile (out, line_no);
	out_printf (out, "%s\n", cbuf);
	if (line_no > 0)
		out_addline_outfile (out);
	out_printf (out, "\t\t}\n");
	if ( ! is_set) {
		if (strcmp (a->gtktype, "OBJECT") == 0)
			out_printf (out, "\t\tg_value_set_%s (VAL, G_OBJECT (ARG));\n",
				    the_type_lower);
		else
			out_printf (out, "\t\t"
				    "g_value_set_%s (VAL, ARG);\n",
				    the_type_lower);
	}
	g_free (the_type_lower);

	if (is_set &&
	    (no_gnu || for_cpp /* g++ has a cow with G_GNUC_UNUSED */)) {
		out_printf (out, "\t\tif (&ARG) break;\n");
	}

	out_printf (out, "\t\tbreak;\n");

	out_printf (out, "\t}\n");
}

static void
add_property (Property *p, gboolean is_set)
{
	const char *cbuf;
	char *the_type_lower;
	char *name_upper;
	int line_no;

	if (is_set) {
		cbuf = p->set;
		line_no = p->set_line;
	} else {
		cbuf = p->get;
		line_no = p->get_line;
	}
	if (cbuf == NULL)
		return;

	name_upper = g_strdup (p->name);
	gob_strup (name_upper);
	the_type_lower = g_strdup (p->gtktype);
	gob_strdown (the_type_lower);

	out_printf (out, "\tcase PROP_%s:\n", name_upper);

	out_printf(out, "\t\t{\n");
	if (line_no > 0)
		out_addline_infile (out, line_no);
	out_printf (out, "%s\n", cbuf);
	if (line_no > 0)
		out_addline_outfile (out);
	out_printf (out, "\t\t}\n");

	g_free (name_upper);
	g_free (the_type_lower);

	out_printf (out, "\t\tbreak;\n");
}

static void
add_getset_arg(Class *c, gboolean is_set)
{
	GList *li;
	const char *unused = "";
	const char *hack_unused = "";

	if ( ! no_gnu && ! for_cpp /* g++ has a cow with this */) {
		unused = " G_GNUC_UNUSED";
	} else {
		hack_unused = "if (&VAL || &pspec) break;\n\t\t";
	}

	out_printf(out, "\nstatic void\n"
		   "___object_%s_property (GObject *object,\n"
		   "\tguint property_id,\n"
		   "\t%sGValue *VAL%s,\n"
		   "\tGParamSpec *pspec%s)\n"
		   "#define __GOB_FUNCTION__ \"%s::%s_property\"\n"
		   "{\n"
		   "\t%s *self%s;\n\n"
		   "\tself = %s (object);\n\n"
		   "\tswitch (property_id) {\n",
		   is_set ? "set" : "get",
		   is_set ? "const " : "",
		   unused,
		   unused,
		   c->otype,
		   is_set ? "set" : "get",
		   typebase,
		   unused,
		   macrobase);

	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if (n->type == PROPERTY_NODE)
			add_property ((Property *)n, is_set);
		else if (n->type == ARGUMENT_NODE)
			add_argument ((Argument *)n, is_set);
	}
	out_printf (out, "\tdefault:\n"
		    "/* Apparently in g++ this is needed, glib is b0rk */\n"
		    "#ifndef __PRETTY_FUNCTION__\n"
		    "#  undef G_STRLOC\n"
		    "#  define G_STRLOC	__FILE__ \":\" G_STRINGIFY (__LINE__)\n"
		    "#endif\n"
		    "\t\tG_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);\n"
		    "\t\t%sbreak;\n\t}\n"
		    "}\n"
		    "#undef __GOB_FUNCTION__\n", hack_unused);
}

static void
print_checks (Method *m, FuncArg *fa)
{
	GList *li;
	gboolean is_void;
	gboolean checked_null = FALSE;
	is_void = (strcmp(m->mtype->name, "void")==0 &&
		   m->mtype->pointer == NULL);
	
	for(li = fa->checks; li != NULL; li = li->next) {
		Check *ch = li->data;
		char *s;
		/* point to the method prot in .gob for failed checks */
		if(m->line_no > 0)
			out_addline_infile(out, m->line_no);
		if(is_void)
			out_printf(out, "\tg_return_if_fail (");
		else
			out_printf(out, "\tg_return_val_if_fail (");
		switch(ch->chtype) {
		case NULL_CHECK:
			out_printf(out, "%s != NULL", fa->name);
			checked_null = TRUE;
			break;
		case TYPE_CHECK:
			s = make_pre_macro(fa->atype->name, "IS");
			if(checked_null)
				out_printf(out, "%s (%s)", s, fa->name);
			else
				/* if not check null, null may be valid */
				out_printf(out, "!(%s) || %s (%s)", fa->name,
					   s, fa->name);
			g_free(s);
			break;
		case LT_CHECK:
			out_printf(out, "%s < %s", fa->name, ch->number);
			break;
		case GT_CHECK:
			out_printf(out, "%s > %s", fa->name, ch->number);
			break;
		case LE_CHECK:
			out_printf(out, "%s <= %s", fa->name, ch->number);
			break;
		case GE_CHECK:
			out_printf(out, "%s >= %s", fa->name, ch->number);
			break;
		case EQ_CHECK:
			out_printf(out, "%s == %s", fa->name, ch->number);
			break;
		case NE_CHECK:
			out_printf(out, "%s != %s", fa->name, ch->number);
			break;
		}
		if(is_void)
			out_printf(out, ");\n");
		else {
			out_printf(out, ", (");
			print_type(out, m->mtype, TRUE);
			out_printf(out, ")%s);\n",
				m->onerror?m->onerror:"0");
		}
	}
}

static void
print_preconditions(Method *m)
{
	GList *li;
	
	for(li=m->args;li;li=g_list_next(li)) {
		FuncArg *fa = li->data;
		if(fa->checks)
			print_checks(m, fa);
	}
	if(m->line_no>0)
		out_addline_outfile(out);
}

static void
print_method_body (Method *m, gboolean pre, gboolean unused_self)
{
	out_printf(out, "{\n");
	if (m->line_no > 0)
		out_addline_outfile(out);
	out_printf(out, "#define __GOB_FUNCTION__ \"%s::%s\"\n",
		   ((Class *)class)->otype,
		   m->id);
	if (pre)
		print_preconditions(m);

	if ( ! pre &&
	     unused_self &&
	    (no_gnu || for_cpp) &&
	    m->args != NULL &&
	    ((FuncArg *)(m->args->data))->name != NULL &&
	    strcmp (((FuncArg *)(m->args->data))->name, "self") == 0) {
		out_printf (out, "\tif (&self) { ; }\n");
	}

	/* Note: the trailing }'s are on one line, this is so
	   that we get the no return warning correctly and point to
	   the correct line in the .gob file, yes this is slightly
	   ugly in the .c file, but that is not supposed to be
	   human readable anyway. */
	if(m->cbuf) {
		out_printf(out, "{\n");
		if(m->ccode_line>0)
			out_addline_infile(out, m->ccode_line);
		out_printf(out, "\t%s}", m->cbuf);
	}

	/* Note, there is no \n between the last } and this } so that
	 * errors/warnings reported on the end of the body get pointed to the
	 * right line in the .gob source */
	out_printf(out, "}\n");

	if(m->cbuf)
		out_addline_outfile(out);
	out_printf(out, "#undef __GOB_FUNCTION__\n");
}

static void
put_signal_args (Method *m)
{
	GList *li;
	GList *ali;
	int i;

	if (m->args->next == NULL)
		return;

	for (ali = m->gtktypes->next, li = m->args->next, i = 1;
	     li != NULL && ali != NULL;
	     li = li->next, ali = ali->next, i++) {
		FuncArg *fa = li->data;
		char *str = ali->data;
		char *cast = g_strdup (get_cast (str, FALSE));
		/* FIXME: This code is so fucking ugly it hurts */
		gboolean do_static = 
			(strcmp  (str, "STRING") == 0 ||
			 strcmp  (str, "BOXED") == 0 ||
			 strncmp (str, "BOXED_", 6) == 0);
		char *set_func;
		char *t;

		if (cast == NULL) {
			cast = get_type (fa->atype, TRUE);
		}
		/* we should have already proved before that
		   the we know all the types */
		g_assert (cast != NULL);

		if (strncmp (str, "BOXED_", 6) == 0)
			t = g_strdup (&(str[6]));
		else
			t = g_strconcat ("G_TYPE_", str, NULL);

		out_printf (out,
			    "\t___param_values[%d].g_type = 0;\n"
			    "\tg_value_init (&___param_values[%d], %s);\n",
			    i, i, t);
		g_free (t);

		if (strcmp (str, "UNICHAR") == 0)
			/* hack because glib is braindamaged */
			set_func = g_strdup ("g_value_set_uint");
		else if (strncmp (str, "BOXED_", 6) == 0)
			set_func = g_strdup ("g_value_set_static_boxed");
		else
			set_func = g_strdup_printf ("g_value_set%s_%s",
						    do_static ? "_static" : "",
						    str);
		gob_strdown (set_func);

		out_printf (out, "\t%s (&___param_values[%d], (%s) %s);\n\n",
			    set_func, i, cast, fa->name);

		g_free (set_func);
		g_free (cast);
	}
}

static void
clear_signal_args (Method *m)
{
	GList *li;
	int i;

	out_printf (out, "\n\tg_value_unset (&___param_values[0]);\n");

	if (m->args->next == NULL)
		return;

	for (li = m->args->next, i = 1;
	     li != NULL;
	     li = li->next, i++) {
		out_printf (out,
			    "\tg_value_unset (&___param_values[%d]);\n", i);
	}
}

static char *
get_arg_names_for_macro (Method *m)
{
	const char *sep;
	GList *li;
	GString *gs = g_string_new(NULL);
	sep = "";
	for(li=m->args;li;li=g_list_next(li)) {
		FuncArg *arg = li->data;
		g_string_sprintfa (gs, "%s___%s", sep, arg->name);
		sep = ",";
	}
	return g_string_free (gs, FALSE);
}

static void
put_method(Method *m)
{
	char *s, *args, *doc;
	gboolean is_void;
	is_void = (strcmp(m->mtype->name, "void")==0 &&
		   m->mtype->pointer == NULL);
	out_printf(out, "\n");
	if(m->method != OVERRIDE_METHOD) {
		doc = get_gtk_doc(m->id);
		if(doc) {
			out_printf(out, "%s", doc);
			g_free(doc);
		}
	}
	switch(m->method) {
	case REGULAR_METHOD:
		if(m->line_no > 0)
			out_addline_infile(out, m->line_no);
		if(m->scope == PRIVATE_SCOPE)
			print_method(out, "static ", "\n", "", " ", "", "\n",
				     m, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE);
		else /* PUBLIC, PROTECTED */
			print_method(out, "", "\n", "", " ", "", "\n",
				     m, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE);
		print_method_body(m, TRUE, TRUE);
		/* the outfile line was added above */
		break;
	case SIGNAL_FIRST_METHOD:
	case SIGNAL_LAST_METHOD:
		if(m->line_no > 0)
			out_addline_infile(out, m->line_no);
		if(m->scope == PRIVATE_SCOPE)
			print_method(out, "static ", "\n", "", " ", "", "\n",
				     m, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE);
		else /* PUBLIC, PROTECTED */
			print_method(out, "", "\n", "", " ", "", "\n",
				     m, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE);
		out_printf (out, "{\n");

		out_addline_outfile (out);

		out_printf (out,
			    "\tGValue ___param_values[%d];\n"
			    "\tGValue ___return_val;\n\n"
			    "memset (&___return_val, 0, "
			      "sizeof (___return_val));\n"
			    "memset (&___param_values, 0, "
			      "sizeof (___param_values));\n\n",
			    g_list_length (m->args));

		print_preconditions (m);

		out_printf (out,
			    "\n\t___param_values[0].g_type = 0;\n"
			    "\tg_value_init (&___param_values[0], G_TYPE_FROM_INSTANCE (%s));\n"
			    "\tg_value_set_instance (&___param_values[0], (gpointer) %s);\n\n",
			    ((FuncArg *)m->args->data)->name,
			    ((FuncArg *)m->args->data)->name);

		put_signal_args (m);

		if (strcmp (m->gtktypes->data, "NONE") != 0) {
			const char *defret = NULL;

			out_printf (out, "\tg_value_init (&___return_val, G_TYPE_%s);\n",
				    (char *)m->gtktypes->data);

			if (m->defreturn != NULL)
				defret = m->defreturn;
			else if (m->onerror != NULL)
				defret = m->onerror;

			if (defret != NULL) {
				char *set_func;
				/* FIXME: This code is so fucking ugly it hurts */
				gboolean do_static = 
					(strcmp ((char *)m->gtktypes->data, "STRING") == 0 ||
					 strcmp ((char *)m->gtktypes->data, "BOXED") == 0);
				char *cast = g_strdup (get_cast (m->gtktypes->data, FALSE));
				if (cast == NULL)
					cast = get_type (m->mtype, TRUE);

				if (strcmp (m->gtktypes->data, "UNICHAR") == 0)
					/* hack because glib is braindamaged */
					set_func = g_strdup ("g_value_set_uint");
				else
					set_func = g_strdup_printf ("g_value_set%s_%s",
								    do_static ? "_static" : "",
								    (char *)m->gtktypes->data);
				gob_strdown (set_func);

				out_printf (out, "\t%s (&___return_val, (%s) (%s));\n",
					    set_func, cast, defret);

				g_free (set_func);
				g_free (cast);
			}
			out_printf (out, "\n");
		}

		s = g_strdup (m->id);
		gob_strup (s);

		out_printf(out, "\tg_signal_emitv (___param_values,\n"
			   "\t\tobject_signals[%s_SIGNAL],\n"
			   "\t\t0 /* detail */,\n"
			   "\t\t&___return_val);\n", s);

		g_free (s);

		clear_signal_args (m);

		if (strcmp (m->gtktypes->data, "NONE") != 0) {
			char *cast = g_strdup (get_cast (m->gtktypes->data, FALSE));
			char *getfunc;
			/* Hack because glib is very very braindead */
			gboolean do_dup = 
				(strcmp ((char *)m->gtktypes->data, "STRING") == 0 ||
				 strcmp ((char *)m->gtktypes->data, "BOXED") == 0 ||
				 strcmp ((char *)m->gtktypes->data, "OBJECT") == 0 ||
				 strcmp ((char *)m->gtktypes->data, "PARAM") == 0);

			if (strcmp (m->gtktypes->data, "UNICHAR") == 0)
				/* hack because glib is braindamaged */
				getfunc = g_strdup ("g_value_get_uint");
			else
				getfunc = g_strdup_printf ("g_value_%s_%s",
							   do_dup ? "dup" : "get",
							   (char *)m->gtktypes->data);
			gob_strdown (getfunc);

			if (cast == NULL)
				cast = get_type (m->mtype, TRUE);

			out_printf (out,
				    "\n\t{\n"
				    "\t\t");
			print_type (out, m->mtype, TRUE);
			out_printf (out,
				    " ___ret = (%s) %s (&___return_val);\n"
				    "\t\tg_value_unset (&___return_val);\n"
				    "\t\treturn ___ret;\n"
				    "\t}\n",
				    cast, getfunc);

			g_free (cast);
			g_free (getfunc);
		}
		out_printf(out, "}\n");

		if(!m->cbuf)
			break;
		if(m->line_no > 0)
			out_addline_infile(out, m->line_no);
		print_method(out, "static ", "\n___real_", "", " ", "", "\n",
			     m, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE);
		print_method_body(m, FALSE, TRUE);
		/* the outfile line was added above */
		break;
	case VIRTUAL_METHOD:
		if(m->line_no > 0)
			out_addline_infile(out, m->line_no);
		if(m->scope==PRIVATE_SCOPE)
			print_method(out, "static ", "\n", "", " ", "", "\n",
				     m, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE);
		else /* PUBLIC, PROTECTED */
			print_method(out, "", "\n", "", " ", "", "\n",
				     m, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE);
		out_printf(out, "{\n");
		out_addline_outfile(out);
		out_printf(out, "\t%sClass *klass;\n", typebase);
		print_preconditions(m);
		out_printf(out, "\tklass = %s_GET_CLASS(%s);\n\n"
			"\tif(klass->%s)\n",
			macrobase, ((FuncArg *)m->args->data)->name,
			m->id);
		if(strcmp(m->mtype->name, "void") == 0 &&
		   m->mtype->pointer == NULL) {
			GList *li;
			out_printf(out, "\t\t(*klass->%s)(%s",
				   m->id,
				   ((FuncArg *)m->args->data)->name);
			for(li=m->args->next;li;li=g_list_next(li)) {
				FuncArg *fa = li->data;
				out_printf(out, ",%s", fa->name);
			}
			out_printf(out, ");\n}\n");
		} else {
			GList *li;
			out_printf(out, "\t\treturn (*klass->%s)(%s",
				   m->id,
				   ((FuncArg *)m->args->data)->name);
			for(li=m->args->next;li;li=g_list_next(li)) {
				FuncArg *fa = li->data;
				out_printf(out, ",%s", fa->name);
			}
			out_printf(out, ");\n"
				"\telse\n"
				"\t\treturn (");
			print_type(out, m->mtype, TRUE);
			if(m->defreturn)
				out_printf(out, ")(%s);\n}\n", m->defreturn);
			else if(m->onerror)
				out_printf(out, ")(%s);\n}\n", m->onerror);
			else
				out_printf(out, ")(0);\n}\n");
		}

		if(!m->cbuf)
			break;
		if(m->line_no > 0)
			out_addline_infile(out, m->line_no);
		print_method(out, "static ", "\n___real_", "", " ", "", "\n",
			     m, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE);
		print_method_body(m, FALSE, TRUE);
		/* the outfile line was added above */
		break;
	case OVERRIDE_METHOD:
		if(!m->cbuf)
			break;
		if(m->line_no > 0)
			out_addline_infile(out, m->line_no);
		s = g_strdup_printf("\n___%x_", (guint)m->unique_id);
		print_method(out, "static ", s, "", " ", "", "\n",
			     m, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE);
		g_free(s);
		out_addline_outfile(out);
		s = replace_sep(m->otype, '_');
		gob_strup (s);
		args = get_arg_names_for_macro(m);
		if(is_void) {
			out_printf(out, "#define PARENT_HANDLER(%s) \\\n"
				   "\t{ if(%s_CLASS(parent_class)->%s) \\\n"
				   "\t\t(* %s_CLASS(parent_class)->%s)(%s); }\n",
				   args, s, m->id, s, m->id, args);
		} else {
			out_printf(out, "#define PARENT_HANDLER(%s) \\\n"
				   "\t((%s_CLASS(parent_class)->%s)? \\\n"
				   "\t\t(* %s_CLASS(parent_class)->%s)(%s): \\\n"
				   "\t\t(",
				   args, s, m->id, s, m->id, args);
			out_printf(out, "(");
			print_type(out, m->mtype, TRUE);
			out_printf(out, ")%s))\n",
				   m->onerror?m->onerror:"0");
		}
		g_free(args);
		g_free(s);
		print_method_body(m, TRUE, TRUE);
		/* the outfile line was added above */
		out_printf(out, "#undef PARENT_HANDLER\n");
		break;
	case CONSTRUCTOR_METHOD:
	case DISPOSE_METHOD:
	case FINALIZE_METHOD:
		if(m->line_no > 0)
			out_addline_infile(out, m->line_no);
		print_method(out, "static ", "\n", "", " ", "", "\n",
			     m, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE);
		print_method_body(m, TRUE, TRUE);
		/* the outfile line was added above */
	default:
		break;
	}
}

static void
open_files(void)
{
	char *outfile, *outfileh, *outfileph;

	outfilebase = g_strconcat (fullfilebase, for_cpp ? ".cc" : ".c", NULL);
	outfile = g_strconcat(outfilebase, no_touch ? "#gob#" : "", NULL);

	outfilehbase = g_strconcat (fullfilebase, ".h", NULL);
	outfileh = g_strconcat(outfilehbase, no_touch_headers ? "#gob#" : "", NULL);

	if ((privates > 0 || protecteds > 0 ||
	     private_header == PRIVATE_HEADER_ALWAYS) &&
	    private_header != PRIVATE_HEADER_NEVER) {
		char sep[2] = {0,0};
		if (file_sep != 0)
			sep[0] = file_sep;
		outfilephbase = g_strconcat (fullfilebase, sep, "private.h", NULL);
		outfileph = g_strconcat (outfilephbase, no_touch ? "#gob#" : "", NULL);
	} else {
		outfilephbase = NULL;
		outfileph = NULL;
	}

	
	if ( ! no_write) {
		out = fopen (outfile, "w");
		if (out == NULL) {
			error_printf (GOB_ERROR, 0,
				      "Cannot open outfile: %s", outfile);
		}
		outh = fopen (outfileh, "w");
		if (outh == NULL) {
			error_printf (GOB_ERROR, 0,
				      "Cannot open outfile: %s", outfileh);
		}
		if (outfileph != NULL) {
			outph = fopen (outfileph, "w");
			if (outph == NULL) {
				error_printf (GOB_ERROR, 0,
					      "Cannot open outfile: %s",
					      outfileph);
			}
		}
	}
}

static void
put_argument_nongnu_wrappers (Class *c)
{
	GList *li;

	if (get_properties < 0 && set_properties < 0)
		return;

	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		const char *name, *gtktype;
		gboolean get, set;
		Type *atype;
		char *aname;
		char *cast;

		if (n->type == ARGUMENT_NODE) {
			Argument *a = (Argument *)n;
			name = a->name;
			gtktype = a->gtktype;
			atype = a->atype;
			get = a->get != NULL;
			set = a->set != NULL;
		} else if (n->type == PROPERTY_NODE) {
			Property *p = (Property *)n;
			name = p->name;
			gtktype = p->gtktype;
			atype = p->ptype;
			get = p->get != NULL;
			set = p->set != NULL;
		} else {
			continue;
		}

		aname = g_strdup (name);
		gob_strup (aname);

		if (atype != NULL)
			cast = get_type (atype, TRUE);
		else
			cast = g_strdup (get_cast (gtktype, TRUE));

		if (cast != NULL) {
			if (set)
				out_printf (outh, "#define %s_PROP_%s(arg)    \t"
					    "\"%s\",(%s)(arg)\n",
					    macrobase, aname, name, cast);
			if (get)
				out_printf (outh, "#define %s_GET_PROP_%s(arg)\t"
					    "\"%s\",(%s*)(arg)\n",
					    macrobase, aname, name, cast);
		} else {
			if(set)
				out_printf (outh, "#define %s_PROP_%s(arg)    \t"
					    "\"%s\",(arg)\n",
					    macrobase, aname, name);
			if(get)
				out_printf (outh, "#define %s_GET_PROP_%s(arg)\t"
					    "\"%s\",(arg)\n",
					    macrobase, aname, name);
		}
		g_free (cast);
		g_free (aname);
	}
}

static void
put_argument_gnu_wrappers(Class *c)
{
	GList *li;

	if(get_properties < 0 && set_properties < 0)
		return;

	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		const char *name, *gtktype;
		gboolean get, set;
		Type *atype;
		char *aname;
		char *cast;

		if (n->type == ARGUMENT_NODE) {
			Argument *a = (Argument *)n;
			name = a->name;
			gtktype = a->gtktype;
			atype = a->atype;
			get = a->get != NULL;
			set = a->set != NULL;
		} else if (n->type == PROPERTY_NODE) {
			Property *p = (Property *)n;
			name = p->name;
			gtktype = p->gtktype;
			atype = p->ptype;
			get = p->get != NULL;
			set = p->set != NULL;
		} else {
			continue;
		}

		aname = g_strdup (name);
		gob_strup (aname);

		if (atype != NULL)
			cast = get_type (atype, TRUE);
		else
			cast = g_strdup (get_cast (gtktype, TRUE));

		if (cast != NULL) {
			if (set)
				out_printf (outh, "#define %s_PROP_%s(arg)    \t"
					   "\"%s\", __extension__ ({%sz = (arg); z;})\n",
					   macrobase, aname, name, cast);
			if (get)
				out_printf (outh, "#define %s_GET_PROP_%s(arg)\t"
					   "\"%s\", __extension__ ({%s*z = (arg); z;})\n",
					   macrobase, aname, name, cast);
		} else {
			if (set)
				out_printf (outh, "#define %s_PROP_%s(arg)    \t"
					   "\"%s\",(arg)\n",
					   macrobase, aname, name);
			if (get)
				out_printf (outh, "#define %s_GET_PROP_%s(arg)\t"
					   "\"%s\",(arg)\n",
					   macrobase, aname, name);
		}
		g_free (cast);
		g_free (aname);
	}
}

static void
print_ccode_block(CCode *cc)
{
	FILE *fp;
	switch(cc->cctype) {
	case HT_CCODE:
		/* HT code is printed exactly like normal header
		   code but is printed before */
	case H_CCODE:
		fp = outh;
		out_printf(fp, "\n");
		break;
	case AT_CCODE:
		/* AT code is printed exactly like normal 'all'
		   code but is printed before */
	case A_CCODE:
		if(outph) {
			out_printf(outph, "\n");
			out_printf(outph, "%s\n", cc->cbuf);
			out_addline_infile(outph, cc->line_no);
			out_addline_outfile(outph);
		}
		out_printf(outh, "\n");
		out_printf(outh, "%s\n", cc->cbuf);
		fp = out;
		out_printf(fp, "\n");
		out_addline_infile(fp, cc->line_no);
		break;
	default:
	case CT_CCODE:
	case C_CCODE:
		fp = out;
		out_printf(fp, "\n");
		out_addline_infile(fp, cc->line_no);
		break;
	case PH_CCODE:
		if(outph)
			fp = outph;
		else
			fp = out;
		out_printf(fp, "\n");
		out_addline_infile(fp, cc->line_no);
		break;
	}
	out_printf(fp, "%s\n", cc->cbuf);
	if(cc->cctype == C_CCODE ||
	   cc->cctype == AD_CCODE ||
	   cc->cctype == A_CCODE ||
	   cc->cctype == AT_CCODE ||
	   cc->cctype == PH_CCODE)
		out_addline_outfile(fp);
}

static void
print_class_block(Class *c)
{
	GList *li;
	char *s;
	gboolean printed_private = FALSE;

	if (c->glade_xml)
	{
		out_printf(outph ? outph : outh, "#include <gtk/gtk.h>\n");
		out_printf(outph ? outph : outh, "#include <glade/glade-xml.h>\n\n");
	}
	
	if(any_special) {
		out_printf(out, "/* utility types we may need */\n");
		if(special_array[SPECIAL_2POINTER])
			out_printf(out, "typedef struct { "
				   "gpointer a; gpointer b; "
				   "} ___twopointertype;\n");
		if(special_array[SPECIAL_3POINTER])
			out_printf(out, "typedef struct { "
				   "gpointer a; gpointer b; "
				   "gpointer c; "
				   "} ___threepointertype;\n");
		if(special_array[SPECIAL_INT_POINTER])
			out_printf(out, "typedef struct { "
				   "gint a; gpointer b; "
				   "} ___intpointertype;\n");
		out_printf(out, "\n");
	}

	out_printf(outh, "\n/*\n"
		   " * Type checking and casting macros\n"
		   " */\n");
	out_printf(outh, "#define %s\t"
		   "(%s_get_type())\n",
		   macrotype, funcbase);
	out_printf(outh, "#define %s(obj)\t"
		   "G_TYPE_CHECK_INSTANCE_CAST((obj), %s_get_type(), %s)\n",
		   macrobase, funcbase, typebase);
	out_printf(outh, "#define %s_CONST(obj)\t"
		   "G_TYPE_CHECK_INSTANCE_CAST((obj), %s_get_type(), %s const)\n",
		   macrobase, funcbase, typebase); 
	out_printf(outh, "#define %s_CLASS(klass)\t"
		   "G_TYPE_CHECK_CLASS_CAST((klass), %s_get_type(), %sClass)\n",
		   macrobase, funcbase, typebase);
	out_printf(outh, "#define %s(obj)\t"
		   "G_TYPE_CHECK_INSTANCE_TYPE((obj), %s_get_type ())\n\n",
		   macrois, funcbase);
	out_printf(outh,
		   "#define %s_GET_CLASS(obj)\t"
		   "G_TYPE_INSTANCE_GET_CLASS((obj), %s_get_type(), %sClass)\n",
		   macrobase, funcbase, typebase);

	if ( ! no_self_alias) {
		out_printf(out, "/* self casting macros */\n");
		out_printf(out, "#define SELF(x) %s(x)\n", macrobase);
		out_printf(out, "#define SELF_CONST(x) %s_CONST(x)\n", macrobase);
		out_printf(out, "#define IS_SELF(x) %s(x)\n", macrois);
		out_printf(out, "#define TYPE_SELF %s\n", macrotype);
		out_printf(out, "#define SELF_CLASS(x) %s_CLASS(x)\n\n",
			   macrobase);
		out_printf(out, "#define SELF_GET_CLASS(x) %s_GET_CLASS(x)\n\n",
			   macrobase);

		out_printf(out, "/* self typedefs */\n");
		out_printf(out, "typedef %s Self;\n", typebase);
		out_printf(out, "typedef %sClass SelfClass;\n\n", typebase);
	}

	if (privates > 0 ||
	    always_private_struct) {
		out_printf (outh, "\n/* Private structure type */\n");
		out_printf (outh, "typedef struct _%sPrivate %sPrivate;\n",
			   typebase, typebase);
		if (privates == 0)
			out_printf (outh, "/* There are no privates, this "
				    "structure is thus never defined */\n");
	}

	out_printf (outh, "\n/*\n"
		    " * Main object structure\n"
		    " */\n");
	s = replace_sep (c->otype, '_');
	gob_strup (s);
	out_printf (outh, "#ifndef __TYPEDEF_%s__\n"
		    "#define __TYPEDEF_%s__\n", s, s);
	g_free (s);
	out_printf (outh, "typedef struct _%s %s;\n"
		    "#endif\n", typebase, typebase);
	out_printf (outh, "struct _%s {\n\t%s __parent__;\n",
		    typebase, ptypebase);
	for (li = c->nodes; li; li=li->next) {
		static gboolean printed_public = FALSE;
		Node *n = li->data;
		Variable *v = (Variable *)n;
		if(n->type == VARIABLE_NODE &&
		   v->scope == PUBLIC_SCOPE) {
			if( ! printed_public) {
				out_printf(outh, "\t/*< public >*/\n");
				printed_public = TRUE;
			}
			put_variable((Variable *)n, outh);
		}
	}
	/* put protecteds always AFTER publics */
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		Variable *v = (Variable *)n;
		if (n->type == VARIABLE_NODE &&
		    v->scope == PROTECTED_SCOPE) {
			if ( ! printed_private) {
				out_printf (outh, "\t/*< private >*/\n");
				printed_private = TRUE;
			}
			put_variable ((Variable *)n, outh);
		}
	}
	if (privates > 0 ||
	    always_private_struct) {
		if ( ! printed_private)
			out_printf (outh, "\t/*< private >*/\n");
		out_printf (outh, "\t%sPrivate *_priv;\n", typebase);
	}
	out_printf (outh, "};\n");

	if (privates > 0) {
		FILE *outfp;

		/* if we are to stick this into the private
		   header, if not stick it directly into the
		   C file */
		if (outph != NULL) 
			outfp = outph;
		else
			outfp = out;

		out_printf (outfp, "struct _%sPrivate {\n",
			    typebase);
		if (privates > 0)
		{
		 for(li=c->nodes; li; li=li->next) {
			 Node *n = li->data;
			 Variable *v = (Variable *)n;
			 if(n->type == VARIABLE_NODE &&
					v->scope == PRIVATE_SCOPE) {
				 out_addline_infile(outfp, v->line_no);
				 put_variable(v, outfp);
			 }
			}
			out_addline_outfile(outfp);
		}
		out_printf(outfp, "};\n");
	}

	out_printf(outh, "\n/*\n"
		   " * Class definition\n"
		   " */\n");
	out_printf(outh, "typedef struct _%sClass %sClass;\n",
		   typebase, typebase);
	out_printf(outh,
		   "struct _%sClass {\n\t%sClass __parent__;\n",
		   typebase, ptypebase);
	for(li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if(n->type == METHOD_NODE)
			put_vs_method((Method *)n);
	}
	/* If BonoboX type class put down the epv */
	if (c->bonobo_object_class != NULL) {
		out_printf (outh,
			    "\t/* Bonobo object epv */\n"
			    "\tPOA_%s__epv _epv;\n",
			    c->bonobo_object_class);
	}
	/* put class scope variables */
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		Variable *v = (Variable *)n;
		if (n->type == VARIABLE_NODE &&
		    v->scope == CLASS_SCOPE)
			put_variable ((Variable *)n, outh);
	}
	out_printf (outh, "};\n\n");

	out_printf (out, "/* here are local prototypes */\n");
	if (set_properties > 0) {
		out_printf (out, "static void ___object_set_property "
			    "(GObject *object, guint property_id, "
			    "const GValue *value, GParamSpec *pspec);\n");
	}
	if (get_properties > 0) {
		out_printf (out, "static void ___object_get_property "
			    "(GObject *object, guint property_id, "
			    "GValue *value, GParamSpec *pspec);\n");
	}

	out_printf (outh, "\n/*\n"
		    " * Public methods\n"
		    " */\n");

	if ( ! overrode_get_type) {
		out_printf (outh, "GType\t%s_get_type\t(void) G_GNUC_CONST;\n", funcbase);
	}

	for(li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if(n->type == METHOD_NODE) {
			put_pub_method((Method *)n);
			put_prot_method((Method *)n);
			put_priv_method_prot((Method *)n);
		}
	}

	/* this idea is less and less apealing to me */
	if (signals > 0) {
		out_printf (outh, "\n/*\n"
			    " * Signal connection wrapper macros\n"
			    " */\n");
		if( ! no_gnu) {
			out_printf(outh, "#if defined(__GNUC__) && !defined(__STRICT_ANSI__)\n");
			put_signal_macros (c, TRUE);
			out_printf(outh, "#else /* __GNUC__ && !__STRICT_ANSI__ */\n");
			put_signal_macros (c, FALSE);
			out_printf(outh, "#endif /* __GNUC__ && !__STRICT_ANSI__ */\n");
		} else {
			put_signal_macros (c, FALSE);
			out_printf(outh, "\n");
		}

		out_printf (out, "\n/*\n"
			    " * Signal connection wrapper macro shortcuts\n"
			    " */\n");
		put_local_signal_macros (c);
		out_printf(outh, "\n");
	}

	/* argument wrapping macros */
	if(get_properties > 0 || set_properties > 0) {
		out_printf(outh, "\n/*\n"
			   " * Argument wrapping macros\n"
			   " */\n");
		if( ! no_gnu) {
			out_printf(outh, "#if defined(__GNUC__) && !defined(__STRICT_ANSI__)\n");
			put_argument_gnu_wrappers(c);
			out_printf(outh, "#else /* __GNUC__ && !__STRICT_ANSI__ */\n");
			put_argument_nongnu_wrappers(c);
			out_printf(outh, "#endif /* __GNUC__ && !__STRICT_ANSI__ */\n\n");
		} else {
			put_argument_nongnu_wrappers(c);
		}
	}

	if(signals > 0) {
		for(li = c->nodes; li != NULL; li = li->next) {
			Node *n = li->data;
			if(n->type == METHOD_NODE)
				add_signal_prots((Method *)n);
		}
	}

	add_enums (c);

	if(any_method_to_alias(c)) {
		out_printf (out, "/* Short form macros */\n");
		make_method_aliases (c);
	}

	add_interface_inits (c);

	if ( ! overrode_get_type) {
		if (c->bonobo_object_class != NULL)
			add_bonobo_object_get_type ();
		else
			add_get_type ();
	}

	out_printf (out, "/* a macro for creating a new object of our type */\n");
	out_printf (out,
		    "#define GET_NEW ((%s *)g_object_new(%s_get_type(), NULL))\n\n",
		    typebase, funcbase);

	out_printf (out, "/* a function for creating a new object of our type */\n");
	out_printf (out, "#include <stdarg.h>\n");
	out_printf (out,
		    "static %s * GET_NEW_VARG (const char *first, ...)%s;\n"
		    "static %s *\nGET_NEW_VARG (const char *first, ...)\n"
		    "{\n\t%s *ret;\n\tva_list ap;\n"
		    "\tva_start (ap, first);\n"
		    "\tret = (%s *)g_object_new_valist (%s_get_type (), "
		    "first, ap);\n"
		    "\tva_end (ap);\n"
		    "\treturn ret;\n}\n\n",
		    typebase,
		    no_gnu ? "" : " G_GNUC_UNUSED",
		    typebase, typebase, typebase, funcbase);

	if (c->glade_xml)
	{
		out_printf (out, "/* a function to connect glade callback */\n");
		out_printf (out,"static void\n"
			    "___glade_xml_connect_foreach(const gchar *handler_name,\n"
			    "GObject *object,\n"
			    "const gchar *signal_name,\n"
			    "const gchar *signal_data,\n"
			    "GObject *connect_object,\n"
			    "gboolean after,\n"
			    "gpointer user_data)\n"
			    "{\n"
			    "\tstatic GModule * allsymbols = NULL;\n"
			    "	\n"
			    "\tif (!allsymbols) allsymbols = g_module_open(NULL, 0);\n"
			    "\tif (allsymbols) {\n"
			    "\t\tgchar * func_name = g_strdup_printf(\"%s_%%s\", handler_name);\n"
			    "\t\tGCallback func;\n"
			    "\n"
			    "\t\tif (!g_module_symbol(allsymbols, func_name, (gpointer)&func)){\n"
			    "\t\t\tif (!g_module_symbol(allsymbols, handler_name, (gpointer)&func)) {\n"
			    "\t\t\t\tg_warning(\"could not find signal handler '%%s'.\", func_name);\n"
			    "\t\t\t\tg_free(func_name);\n"
			    "\t\t\t\treturn;\n"
			    "\t\t\t}\n"
			    "\t\t}\n"
			    "\t\tif (after)\n"
			    "\t\t\tg_signal_connect_data(object, signal_name, func, user_data, NULL, G_CONNECT_AFTER | G_CONNECT_SWAPPED);\n"
			    "\t\telse\n"
			    "\t\t\tg_signal_connect_data(object, signal_name, func, user_data, NULL, G_CONNECT_SWAPPED);\n"
			    "\t\tg_free(func_name);\n"
			    "\t}\n"
			    "}\n"
			    "\n",
			    funcbase);
	}

	for (li = nodes; li != NULL; li = li->next) {
		Node *node = li->data;
		if (node->type == CCODE_NODE) {
			CCode *cc = (CCode *)node;
			if (cc->cctype == AD_CCODE)
				print_ccode_block (cc);
		}
	}

	if (need_constructor)
		add_constructor (c);

	if (need_dispose)
		add_dispose (c);

	if (need_finalize)
		add_finalize (c);

	add_inits(c);

	if(set_properties > 0) {
		add_getset_arg(c, TRUE);
	}

	if(get_properties > 0) {
		add_getset_arg(c, FALSE);
	}

	for(li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if(n->type == METHOD_NODE)
			put_method((Method *)n);
	}

	add_bad_hack_to_avoid_unused_warnings(c);
}

static void
print_useful_macros(void)
{
	int major = 0, minor = 0, pl = 0;

	/* Version stuff */
	sscanf (VERSION, "%d.%d.%d", &major, &minor, &pl);
	out_printf (out, "#define GOB_VERSION_MAJOR %d\n", major);
	out_printf (out, "#define GOB_VERSION_MINOR %d\n", minor);
	out_printf (out, "#define GOB_VERSION_PATCHLEVEL %d\n\n", pl);

	/* Useful priv macro thingie */
	/* FIXME: this should be done the same way that priv is, as a var,
	 * not a define */
	out_printf (out, "#define selfp (self->_priv)\n\n");
}

static void
print_more_useful_macros (void)
{
	if (no_gnu) {
		out_printf (out, "#define ___GOB_LIKELY(expr) (expr)\n");
		out_printf (out, "#define ___GOB_UNLIKELY(expr) (expr)\n");
	} else {
		out_printf (out, "#ifdef G_LIKELY\n");
		out_printf (out, "#define ___GOB_LIKELY(expr) G_LIKELY(expr)\n");
		out_printf (out, "#define ___GOB_UNLIKELY(expr) G_UNLIKELY(expr)\n");
		out_printf (out, "#else /* ! G_LIKELY */\n");
		out_printf (out, "#define ___GOB_LIKELY(expr) (expr)\n");
		out_printf (out, "#define ___GOB_UNLIKELY(expr) (expr)\n");
		out_printf (out, "#endif /* G_LIKELY */\n");
	}
}

static void
print_file_comments(void)
{
	out_printf(outh, "/* Generated by GOB (v%s)"
		   "   (do not edit directly) */\n\n", VERSION);
	if(outph)
		out_printf(outph, "/* Generated by GOB (v%s)"
			   "   (do not edit directly) */\n\n", VERSION);
	out_printf(out, "/* Generated by GOB (v%s)"
		   "   (do not edit directly) */\n\n", VERSION);

	out_printf(out, "/* End world hunger, donate to the World Food Programme, http://www.wfp.org */\n\n");
}

static void
print_includes(void)
{
	gboolean found_header;
	char *p;

	/* We may need string.h for memset */
	if ( ! g_list_find_custom(include_files, "string.h", (GCompareFunc)strcmp)) {
		out_printf(out, "#include <string.h> /* memset() */\n\n");
	}

	p = g_strconcat(filebase, ".h", NULL);
	found_header = TRUE;
	if( ! g_list_find_custom(include_files, p, (GCompareFunc)strcmp)) {
		out_printf(out, "#include \"%s.h\"\n\n", filebase);
		found_header = FALSE;
	}
	g_free(p);

	/* if we are creating a private header see if it was included */
	if(outph) {
		char sep[2] = {0,0};
		if (file_sep != 0)
			sep[0] = file_sep;
		p = g_strconcat(filebase, sep, "private.h", NULL);
		if( ! g_list_find_custom(include_files, p,
					 (GCompareFunc)strcmp)) {
			out_printf(out, "#include \"%s%sprivate.h\"\n\n",
				   filebase,
				   sep);
			if(found_header)
				error_printf(GOB_WARN, 0,
					    "Implicit private header include "
					    "added to top of\n"
					    "\tsource file, while public "
					    "header is at a custom location, "
					    "you should\n"
					    "\texplicitly include "
					    "the private header below the "
					    "public one.");
		}
		g_free(p);
	}
}

static void
print_header_prefixes(void)
{
	char *p;

	p = replace_sep(((Class *)class)->otype, '_');
	gob_strup (p);
	out_printf(outh, "#ifndef __%s_H__\n#define __%s_H__\n\n", p, p);
	if(outph)
		out_printf(outph, "#ifndef __%s_PRIVATE_H__\n"
			   "#define __%s_PRIVATE_H__\n\n"
			   "#include \"%s.h\"\n\n", p, p, filebase);
	g_free(p);

	if( ! no_extern_c) {
		out_printf(outh, "#ifdef __cplusplus\n"
			   "extern \"C\" {\n"
			   "#endif /* __cplusplus */\n\n");
		if(outph)
			out_printf(outph, "#ifdef __cplusplus\n"
				   "extern \"C\" {\n"
				   "#endif /* __cplusplus */\n\n");
	}
}

static void
print_header_postfixes(void)
{
	if( ! no_extern_c)
		out_printf(outh, "\n#ifdef __cplusplus\n"
			   "}\n"
			   "#endif /* __cplusplus */\n");
	out_printf(outh, "\n#endif\n");
	if(outph) {
		if( ! no_extern_c)
			out_printf(outph, "\n#ifdef __cplusplus\n"
				   "}\n"
				   "#endif /* __cplusplus */\n");
		out_printf(outph, "\n#endif\n");
	}
}

static void
print_all_top(void)
{
	GList *li;

	/* print the AT_CCODE and CT_CCODE blocks */
	for(li = nodes; li != NULL; li = li->next) {
		Node *node = li->data;
		if(node->type == CCODE_NODE) {
			CCode *cc = (CCode *)node;
			if (cc->cctype == AT_CCODE ||
			    cc->cctype == CT_CCODE)
				print_ccode_block((CCode *)node);
		}
	}
}

static void
print_header_top(void)
{
	GList *li;

	/* mandatory includes */
	out_printf (outh, "#include <glib.h>\n");
	out_printf (outh, "#include <glib-object.h>\n");

	/* print the HT_CCODE blocks */
	for (li = nodes; li != NULL; li = li->next) {
		Node *node = li->data;
		if (node->type == CCODE_NODE) {
			CCode *cc = (CCode *)node;
			if (cc->cctype == HT_CCODE)
				print_ccode_block ((CCode *)node);
		}
	}
}

static void
print_enum (EnumDef *enode)
{
	GList *li;
	char *funcprefix;
	char *type;
	char *str;

	funcprefix = replace_sep (enode->etype, '_');
	gob_strdown (funcprefix);
	out_printf (out, "static const GEnumValue _%s_values[] = {\n",
		    funcprefix);
	type = remove_sep (enode->etype);

	out_printf (outh, "\ntypedef enum {\n");

	for (li = enode->values; li != NULL; li = li->next) {
		EnumValue *value = li->data; 
		char *p;
		char *sname = gob_strdown (g_strdup (value->name));

		while ((p = strchr (sname, '_')) != NULL)
			*p = '-';

		out_printf (outh, "\t%s_%s", enode->prefix, value->name);
		if (value->value != NULL)
			out_printf (outh, " = %s", value->value);
		if (li->next != NULL)
			out_printf (outh, ",\n");
		else
			out_printf (outh, "\n");

		out_printf (out, "\t{ %s_%s, (char *)\"%s_%s\", (char *)\"%s\" },\n",
			    enode->prefix, value->name,
			    enode->prefix, value->name,
			    sname);

		g_free (sname);
	}

	out_printf (out, "\t{ 0, NULL, NULL }\n};\n\n");

	out_printf (outh, "} %s;\n", type);

	str = make_pre_macro (enode->etype, "TYPE");
	out_printf (outh, "#define %s ", str);
	g_free (str);

	out_printf (outh, "%s_get_type()\n", funcprefix);
	out_printf (outh, "GType %s_get_type (void) G_GNUC_CONST;\n\n", funcprefix);

	out_printf (out,
		    "GType\n%s_get_type (void)\n"
		    "{\n"
		    "\tstatic GType type = 0;\n"
		    "\tif ___GOB_UNLIKELY(type == 0)\n"
		    "\t\ttype = g_enum_register_static (\"%s\", _%s_values);\n"
		    "\treturn type;\n"
		    "}\n\n",
		    funcprefix, type, funcprefix);

	g_free (funcprefix);
	g_free (type);
}

static void
print_flags (Flags *fnode)
{
	GList *li;
	char *funcprefix;
	char *type;
	char *str;
	int i;

	funcprefix = replace_sep (fnode->ftype, '_');
	gob_strdown (funcprefix);
	out_printf (out, "static const GFlagsValue _%s_values[] = {\n",
		    funcprefix);
	type = remove_sep (fnode->ftype);

	out_printf (outh, "\ntypedef enum {\n");

	for (i = 0, li = fnode->values; li != NULL; i++, li = li->next) {
		const char *name = li->data; 
		char *p;
		char *sname = gob_strdown (g_strdup (name));

		while ((p = strchr (sname, '_')) != NULL)
			*p = '-';

		out_printf (outh, "\t%s_%s = 1<<%d",
			    fnode->prefix, name, i);
		if (li->next != NULL)
			out_printf (outh, ",\n");
		else
			out_printf (outh, "\n");

		out_printf (out, "\t{ %s_%s, (char *)\"%s_%s\", (char *)\"%s\" },\n",
			    fnode->prefix, name,
			    fnode->prefix, name,
			    sname);

		g_free (sname);
	}

	out_printf (out, "\t{ 0, NULL, NULL }\n};\n\n");

	out_printf (outh, "} %s;\n", type);

	str = make_pre_macro (fnode->ftype, "TYPE");
	out_printf (outh, "#define %s ", str);
	g_free (str);

	out_printf (outh, "%s_get_type()\n", funcprefix);
	out_printf (outh, "GType %s_get_type (void) G_GNUC_CONST;\n\n", funcprefix);

	out_printf (out,
		    "GType\n%s_get_type (void)\n"
		    "{\n"
		    "\tstatic GType type = 0;\n"
		    "\tif ___GOB_UNLIKELY(type == 0)\n"
		    "\t\ttype = g_flags_register_static (\"%s\", _%s_values);\n"
		    "\treturn type;\n"
		    "}\n\n",
		    funcprefix, type, funcprefix);

	g_free (funcprefix);
	g_free (type);
}

static void
print_error (Error *enode)
{
	GList *li;
	char *funcprefix;
	char *type;
	char *str;

	funcprefix = replace_sep (enode->etype, '_');
	gob_strdown (funcprefix);
	out_printf (out, "static const GEnumValue _%s_values[] = {\n",
		    funcprefix);
	type = remove_sep (enode->etype);

	out_printf (outh, "\ntypedef enum {\n");

	for (li = enode->values; li != NULL; li = li->next) {
		const char *name = li->data;
		char *p;
		char *sname = gob_strdown (g_strdup (name));

		while ((p = strchr (sname, '_')) != NULL)
			*p = '-';

		out_printf (outh, "\t%s_%s", enode->prefix, name);
		if (li->next != NULL)
			out_printf (outh, ",\n");
		else
			out_printf (outh, "\n");

		out_printf (out, "\t{ %s_%s, (char *)\"%s_%s\", (char *)\"%s\" },\n",
			    enode->prefix, name,
			    enode->prefix, name,
			    sname);

		g_free (sname);
	}

	out_printf (out, "\t{ 0, NULL, NULL }\n};\n\n");

	out_printf (outh, "} %s;\n", type);

	str = make_pre_macro (enode->etype, "TYPE");
	out_printf (outh, "#define %s ", str);
	g_free (str);

	out_printf (outh, "%s_get_type ()\n", funcprefix);
	out_printf (outh, "GType %s_get_type (void) G_GNUC_CONST;\n\n", funcprefix);

	out_printf (out,
		    "GType\n%s_get_type (void)\n"
		    "{\n"
		    "\tstatic GType type = 0;\n"
		    "\tif ___GOB_UNLIKELY(type == 0)\n"
		    "\t\ttype = g_enum_register_static (\"%s\", _%s_values);\n"
		    "\treturn type;\n"
		    "}\n\n",
		    funcprefix, type, funcprefix);

	out_printf (outh, "#define %s %s_quark ()\n", enode->prefix, funcprefix);
	out_printf (outh, "GQuark %s_quark (void);\n\n", funcprefix);

	str = replace_sep (enode->etype, '-');
	gob_strdown (str);

	out_printf (out,
		    "GQuark\n%s_quark (void)\n"
		    "{\n"
		    "\tstatic GQuark q = 0;\n"
		    "\tif (q == 0)\n"
		    "\t\tq = g_quark_from_static_string (\"%s\");\n"
		    "\treturn q;\n"
		    "}\n\n",
		    funcprefix, str);

	g_free (str);

	g_free (funcprefix);
	g_free (type);
}

static void
generate_outfiles(void)
{
	GList *li;

	print_file_comments();

	print_all_top();

	print_header_top();

	print_header_prefixes();

	print_useful_macros();

	print_includes();

	print_more_useful_macros ();

	for (li = nodes; li != NULL; li = li->next) {
		Node *node = li->data;
		if (node->type == CCODE_NODE) {
			CCode *cc = (CCode *)node;
			if (cc->cctype != HT_CCODE &&
			    cc->cctype != AT_CCODE &&
			    cc->cctype != AD_CCODE)
				print_ccode_block ((CCode *)node);
		} else if (node->type == CLASS_NODE) {
			print_class_block ((Class *)node);
		} else if (node->type == ENUMDEF_NODE) {
			print_enum ((EnumDef *)node);
		} else if (node->type == FLAGS_NODE) {
			print_flags ((Flags *)node);
		} else if (node->type == ERROR_NODE) {
			print_error ((Error *)node);
		} else {
			g_assert_not_reached();
		}
	}

	print_header_postfixes();
}

static void
print_help(void)
{
	fprintf(stderr, "Gob version %s\n\n", VERSION);
	fprintf(stderr, "gob [options] file.gob\n\n");
	fprintf(stderr, "Options:\n"
		"\t--help,-h,-?            Display this help\n"
		"\t--version               Display version\n"
		"\t--exit-on-warn,-w       Exit with an error on warnings\n"
		"\t--no-exit-on-warn       Don't exit on warnings [default]\n"
		"\t--for-cpp               Create C++ files\n"
		"\t--no-extern-c           Never print extern \"C\" into the "
	       				  "header\n"
		"\t--no-gnu                Never use GNU extentions\n"
		"\t--no-touch              Don't touch output files unless they "
		                          "really\n"
		"\t                        changed (implies --no-touch-headers)\n"
		"\t--no-touch-headers      Don't touch headers unless they "
		                          "really changed\n"
		"\t--always-private-header Always create a private header "
					  "file,\n"
		"\t                        even if it would be empty\n"
		"\t--ondemand-private-header Create private header only when "
					  "needed\n"
		"\t                        [default]\n"
		"\t--no-private-header     Don't create a private header, "
					  "put private\n"
	  	"\t                        structure and protected "
					  "prototypes inside c file\n"
		"\t--always-private-struct Always create a private pointer "
					  "in\n"
		"\t                        the object structure\n"
		"\t--m4                    Preprocess source with m4. "
					  "Following args will\n"
		"\t                        be passed to m4\n"
		"\t--m4-dir                Print directory that will be "
					  "searched for m4\n"
		"\t                        files\n"
		"\t--no-write,-n           Don't write output files, just "
					  "check syntax\n"
		"\t--no-lines              Don't print '#line' to output\n"
		"\t--no-self-alias         Don't create self type and macro "
		                          "aliases\n"
		"\t--no-kill-underscores   Ignored for compatibility\n"
		"\t-o,--output-dir         The directory where output "
			                  "should be placed\n"
		"\t--file-sep[=c]          replace default \'-\' file "
		                          "name separator\n\n"
		"\t--gtk3                  Use gtk+3\n"
		);
	fprintf(stderr, "End world hunger, donate to the World Food Programme, http://www.wfp.org\n");
}

static void
parse_options(int argc, char *argv[])
{
	int i;
	int got_file = FALSE;
	int no_opts = FALSE;
	int m4_opts = FALSE; /* if we are just passing on args to m4 */

	filename = NULL;

	for(i = 1 ; i < argc; i++) {
		if(m4_opts) {
			char *new_commandline;
			g_assert(m4_commandline!=NULL);

			/* check whether this one looks like the filename */
			if((!strcmp(argv[i],"-") || argv[i][0] != '-') 
			   && !got_file) {
			        const gchar *m4_flags=use_m4_clean?"":M4_FLAGS;
				filename = argv[i];
				got_file = TRUE;
				
				/* insert flags before the filename */
				new_commandline=g_strconcat(m4_commandline,
							    " ",
							    m4_flags, 
							    " ",
							    argv[i],
							    NULL);
			}

			/* just an ordinary option */
			else			  
			  new_commandline=g_strconcat(m4_commandline,
						      " ",
						      argv[i],
						      NULL);

			/* free old commandline */
			g_free(m4_commandline);
			m4_commandline=new_commandline;

		} else if(no_opts ||
		   argv[i][0] != '-') {
			/*must be a file*/
			if(got_file) {
				fprintf(stderr, "Specify only one file!\n");
				print_help();
				exit(1);
			}
			filename = argv[i];
			got_file = TRUE;
		} else if(strcmp(argv[i], "--help")==0) {
			print_help();
			exit(0);
		} else if(strcmp(argv[i], "--version")==0) {
			fprintf(stderr, "Gob version %s\n", VERSION);
			exit(0);
		} else if(strcmp(argv[i], "--exit-on-warn")==0) {
			exit_on_warn = TRUE;
		} else if(strcmp(argv[i], "--no-exit-on-warn")==0) {
			exit_on_warn = FALSE;
		} else if(strcmp(argv[i], "--for-cpp")==0) {
			for_cpp = TRUE;
		} else if(strcmp(argv[i], "--no-touch")==0) {
			no_touch = TRUE;
			no_touch_headers = TRUE;
		} else if(strcmp(argv[i], "--no-touch-headers")==0) {
			no_touch_headers = TRUE;
		} else if(strcmp(argv[i], "--ondemand-private-header")==0) {
			private_header = PRIVATE_HEADER_ONDEMAND;
		} else if(strcmp(argv[i], "--always-private-header")==0) {
			private_header = PRIVATE_HEADER_ALWAYS;
		} else if(strcmp(argv[i], "--no-private-header")==0) {
			private_header = PRIVATE_HEADER_NEVER;
		} else if(strcmp(argv[i], "--no-gnu")==0) {
			no_gnu = TRUE;
		} else if(strcmp(argv[i], "--no-extern-c")==0) {
			no_extern_c = TRUE;
		} else if(strcmp(argv[i], "--no-write")==0) {
			no_write = TRUE;
		} else if(strcmp(argv[i], "--no-lines")==0) {
			no_lines = TRUE;
		} else if(strcmp(argv[i], "--no-self-alias")==0) {
			no_self_alias = TRUE;
		} else if(strcmp(argv[i], "--no-kill-underscores")==0) {
			/* no op */;
		} else if(strcmp(argv[i], "--always-private-struct")==0) {
			always_private_struct = TRUE;
		} else if(strcmp(argv[i], "--m4-dir")==0) {
			printf("%s\n",M4_INCLUDE_DIR);
			exit(0);
		} else if(strcmp(argv[i], "--m4")==0) {
			use_m4 = TRUE;
			use_m4_clean=FALSE;
			m4_opts = TRUE;
			m4_commandline=g_strdup(M4_COMMANDLINE);
		} else if(strcmp(argv[i], "--m4-clean")==0) {
			use_m4 = TRUE;
			use_m4_clean=TRUE;
			m4_opts = TRUE;
			m4_commandline=g_strdup(M4_COMMANDLINE);
		} else if (strcmp (argv[i], "-o") == 0 || 
			   strcmp (argv[i], "--output-dir") == 0) {
			if (i+1 < argc) {
				output_dir = g_strdup (argv[i+1]);
				i++;
			} else {
				output_dir = NULL;
			}
		} else if (strncmp (argv[i], "-o=", strlen ("-o=")) == 0 || 
			   strncmp (argv[i],
				    "--output-dir=",
				    strlen ("--output-dir=")) == 0) {
			char *p = strchr (argv[i], '=');
			g_assert (p != NULL);
			output_dir = g_strdup (p+1);
		} else if (strncmp (argv[i], "--file-sep=",
                                    strlen ("--file-sep=")) == 0) {
			char *p = strchr (argv[i], '=');
			g_assert (p != NULL);
			file_sep = *(p+1);
		} else if (strncmp (argv[i], "--file-sep",
                                    strlen ("--file-sep")) == 0) {
			if (i+1 < argc) {
				file_sep = (argv[i+1])[0];
				i++;
			} else {
				file_sep = 0;
			}
		} else if(strcmp(argv[i], "--gtk3")==0) {
			gtk3_ok = TRUE;
		} else if(strcmp(argv[i], "--")==0) {
			/*further arguments are files*/
			no_opts = TRUE;
		} else if(strncmp(argv[i], "--", 2)==0) {
			/*unknown long option*/
			fprintf(stderr, "Unknown option '%s'!\n", argv[i]);
			print_help();
			exit(1);
		} else {
			/*by now we know we have a string starting with
			  - which is a short option string*/
			char *p;
			for(p = argv[i] + 1; *p; p++) {
				switch(*p) {
				case 'w':
					exit_on_warn=TRUE;
					break;
				case 'n':
					no_write = TRUE;
					break;
				case 'h':
				case '?':
					print_help();
					exit(0);
				default:
					fprintf(stderr,
						"Unknown option '%c'!\n", *p);
					print_help();
					exit(1);
				}
			}
		}
	}

#if 0
	/* if we are using m4, and got no filename, append m4 flags now */
	if(!got_file && use_m4 && !use_m4_clean) {
		char *new_commandline;
	        new_commandline=g_strconcat(m4_commandline,
					    " ",
					    M4_FLAGS,
					    NULL);
		g_free(m4_commandline);
		m4_commandline=new_commandline;
	}
#endif
}

static void
compare_and_move (const char *old_filename)
{
	char *new_filename = g_strconcat (old_filename, "#gob#", NULL);
	FILE *old_f;
	gboolean equal = FALSE;

	old_f = fopen (old_filename, "r");
	if (old_f) {
		FILE *new_f;
		gboolean error = FALSE;

		new_f = fopen (new_filename, "r");
		if (new_f) {
			char new_buf[1024];
			char old_buf[1024];

			while (TRUE) {
				size_t new_n;
				size_t old_n;

				new_n = fread (new_buf, 1, sizeof (new_buf), new_f);
				if (ferror (new_f)) {
					error = TRUE;
					error_printf (GOB_ERROR, 0,
						      "Can't read %s: %s",
						      new_filename,
						      g_strerror (errno));
					break;
				}

				old_n = fread (old_buf, 1, sizeof (old_buf), old_f);
				if (ferror (old_f)
				    || feof (new_f) != feof (old_f)
				    || new_n != old_n
				    || memcmp (new_buf, old_buf, new_n) != 0)
					break;

				if (feof (new_f)) {
					equal = TRUE;
					break;
				}
			}
		} else
			error_printf (GOB_ERROR, 0, "Can't open %s: %s",
				      new_filename, g_strerror (errno));

		fclose (old_f);
		fclose (new_f);

		if (error)
			goto end;

		if (! equal && unlink (old_filename) != 0) {
			error_printf (GOB_ERROR, 0, "Can't remove %s: %s",
				      old_filename, g_strerror (errno));
			goto end;
		}
	}

	if (equal) {
		if (unlink (new_filename) != 0)
			error_printf (GOB_ERROR, 0, "Can't remove %s: %s",
				      new_filename, g_strerror (errno));
	} else {
		if (rename (new_filename, old_filename) != 0)
			error_printf (GOB_ERROR, 0, "Can't rename %s to %s: %s",
				      new_filename, old_filename,
				      g_strerror (errno));
	}

 end:
	g_free (new_filename);
}

int
main(int argc, char *argv[])
{
	parse_options(argc, argv);

	if(use_m4) {
		yyin = popen(m4_commandline, "r");
		if(!yyin) {
			fprintf(stderr, "Error: can't open pipe from '%s'\n",
				m4_commandline);
			exit(1);
		}
	} else if(filename) {
		yyin = fopen(filename, "r");
		if(!yyin) {
			fprintf(stderr, "Error: can't open file '%s'\n",
				filename);
			exit(1);
		}
	}

	if(filename==NULL)
		filename = "stdin";

	/* This is where parsing is done */
	/*yydebug = 1;*/
	if(yyparse() != 0)
		error_print (GOB_ERROR, 0, "Parsing errors, quitting");

	/* close input file */
	if(use_m4) pclose(yyin);
	else fclose(yyin);
	yyin=stdin;

	if( ! class)
		error_print (GOB_ERROR, 0, "no class defined");
	

	exit_on_error = FALSE;

	signals = count_signals ((Class *)class);
	set_properties = count_set_properties ((Class *)class) +
		count_set_arguments ((Class *)class);
	get_properties = count_get_properties ((Class *)class) +
		count_get_arguments ((Class *)class);
	overrides = count_overrides ((Class *)class);
	privates = count_privates ((Class *)class);
	protecteds = count_protecteds ((Class *)class);
	unreftors = count_unreftors ((Class *)class);
	destructors = count_destructors ((Class *)class);
	initializers = count_initializers ((Class *)class);
	glade_widgets = count_glade_widgets ((Class *)class);
	overrode_get_type = find_get_type ((Class *)class);

	make_bases ();
	make_inits ((Class *)class);

	find_constructor ((Class *)class);
	if (user_constructor != NULL)
		need_constructor = TRUE;

	find_dispose ((Class *)class);
	if (unreftors > 0 ||
	    dispose_handler != NULL ||
	    user_dispose_method != NULL)
		need_dispose = TRUE;

	find_finalize ((Class *)class);
	if (destructors > 0 ||
	    privates > 0 ||
	    user_finalize_method != NULL) {
		need_finalize = TRUE;
	}

	check_bad_symbols ((Class *)class);
	check_duplicate_symbols ((Class *)class);
	check_duplicate_overrides ((Class *)class);
	check_duplicate_signals_args ((Class *)class);
	check_public_new ((Class *)class);
	check_vararg ((Class *)class);
	check_firstarg ((Class *)class);
	check_nonvoidempty ((Class *)class);
	check_signal_args ((Class *)class);
	check_property_types ((Class *)class);
	check_argument_types ((Class *)class);
	check_func_arg_checks ((Class *)class);
	check_func_attrs ((Class *)class);
	check_for_class_destructors ((Class *)class);

	exit_on_error = TRUE;
	
	if (got_error)
		exit (1);

	any_special = setup_special_array ((Class *)class, special_array);

	open_files ();
	
	generate_outfiles ();

	if (out)
		fclose (out);
	if (outh)
		fclose (outh);
	if (outph)
		fclose (outph);

	if (! no_write) {
		if (no_touch) {
			compare_and_move (outfilebase);
			if (outfilephbase)
				compare_and_move (outfilephbase);
		}
		if (no_touch_headers)
			compare_and_move (outfilehbase);
	}
	
	return 0;
}
