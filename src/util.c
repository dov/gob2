/* GOB C Preprocessor
 * Copyright (C) 1999-2000 the Free Software Foundation.
 * Copyright (C) 2000 Eazel, Inc.
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

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>

#include "treefuncs.h"
#include "main.h"

#include "util.h"

void
error_print(int type, int line, const char *error)
{
	const char *w = NULL;
	const char *fname = NULL;

	switch(type) {
	case GOB_WARN:
		w = "Warning:";
		if (exit_on_warn)
			got_error = TRUE;
		break;
	case GOB_ERROR:
		w = "Error:";
		got_error = TRUE;
		break;
	default:
		g_assert_not_reached();
	}
	fname = filename;
	if (fname == NULL)
		fname = "gob2";
	if (line > 0)
		fprintf(stderr, "%s:%d: %s %s\n", fname, line, w, error);
	else
		fprintf(stderr, "%s: %s %s\n", fname, w, error);
	if (exit_on_error && got_error)
		exit(1);
}

void
error_printf(int type, int line, const char *error, ...)
{
	va_list ap;
	char *s;

	va_start(ap, error);
	s = g_strdup_vprintf(error, ap);
	va_end(ap);

	error_print(type, line, s);

	g_free(s);
}

char *
remove_sep(const char *base)
{
	char *p;
	char *s = g_strdup(base);
	char *q=s;

	/* don't eat C++ :: thingies */
	if (for_cpp && strstr (s, "::") != NULL)
		return s;

	for(p=(char *)base;*p;p++){
		if (*p!=':')
			*q++=*p;
	}
	*q='\0';
	return s;
}

char *
replace_sep(const char *base, char r)
{
	char *p;
	char *s = g_strdup(base);

	/* don't eat C++ :: thingies */
	if (for_cpp && strstr (s, "::") != NULL)
		return s;

	if (r == '\0') {
		while ((p=strchr(s,':')) != NULL) {
			char *t = p;
			while (*t != '\0') {
				*t = *(t+1);
				t++;
			}
		}
	} else {
		while ((p=strchr(s,':')) != NULL)
			*p = r;
	}
	if(*s == r) {
		p = g_strdup(s+1);
		g_free(s);
		return p;
	}
	return s;
}

/*separate the namespace part and then replace rest of
  separators with r*/
void
separns_replace_sep(const char *base, char **ns, char **name, char r)
{
	char *p;
	char *s = g_strdup(base);

	*ns = NULL;
	
	/* don't eat C++ :: thingies */
	if (for_cpp && strstr (s, "::") != NULL) {
		*name = s;
		return;
	}

	if((p=strchr(s,':')) && p!=s) {
		*p = '\0';
		*ns = g_strdup(s);
		p = g_strdup(p+1);
		g_free(s);
		s = p;
	}
	while((p=strchr(s,':')))
		*p = r;
	if(*s == r) {
		*name = g_strdup(s+1);
		g_free(s);
	} else
		*name = s;
}

/* make a macro with some prefix before the name but after
   namespace */
char *
make_pre_macro(const char *base, const char *pre)
{
	char *ns, *name;
	char *s;
	char **v = NULL;

	if(strchr(base, ' ')) {
		int i;
		v = g_strsplit(base, " ", 0);
		for(i = 0; v[i] != NULL; i++) {
			if(*v[i] && strcmp(v[i], "const") != 0) {
				base = v[i];
				break;
			}
		}
	}

	separns_replace_sep(base, &ns, &name, '_');
	if(ns)
		s = g_strconcat(ns, "_", pre, "_", name,NULL);
	else
		s = g_strconcat(pre, "_", name, NULL);

	gob_strup (s);
	
	g_free(ns);
	g_free(name);

	g_strfreev(v);

	return s;
}

/* here we will find out how inconsistent gtk really is :) */
/* the commented out types mean that these types don't actually
   exist. so we "emulate them" with an equivalent */
typedef struct _OurGtkType OurGtkType;
struct _OurGtkType {
	gboolean simple;
	char *gtkname;
	char *cast;
	char *type_name;
	char *type_pointer;
	int special;
};
const OurGtkType our_gtk_type_table[] = {
	{ TRUE, "NONE",		"void ",	"void",		NULL,	-1 },
	{ TRUE, "CHAR",		"gchar ",	"gchar",	NULL,	-1 },
	{ TRUE, "UCHAR",	"guchar ",	"guchar",	NULL,	-1 },
	{ TRUE, "UNICHAR",	"gunichar ",	"gunichar",	NULL,	-1 },
	{ TRUE, "BOOLEAN",	"gboolean ",	"gboolean",	NULL,	-1 },
	{ TRUE, "INT",		"gint ",	"gint",		NULL,	-1 },
	{ TRUE, "UINT",		"guint ", 	"guint",	NULL,	-1 },
	{ TRUE, "LONG",		"glong ",	"glong",	NULL,	-1 },
	{ TRUE, "ULONG",	"gulong ",	"gulong",	NULL,	-1 },
	{ TRUE, "INT64",	"gint64 ",	"gint64",	NULL,	-1 },
	{ TRUE, "UINT64",	"guint64 ", 	"guint64",	NULL,	-1 },
	{ TRUE, "ENUM",		/*"enum"*/"gint ", "gint",	NULL,	-1 },
	{ TRUE, "FLAGS",	/*"flags"*/"guint ", "guint",	NULL,	-1 },
	{ TRUE, "FLOAT",	"gfloat ",	"gfloat",	NULL,	-1 },
	{ TRUE, "DOUBLE",	"gdouble ",	"gdouble",	NULL,	-1 },
	{ TRUE, "STRING",	/*"string"*/"gchar *", "gchar",	"*",	-1 },
	{ TRUE, "POINTER",	"gpointer ",	"gpointer",	NULL,	-1 },
	{ TRUE, "BOXED",	/*"boxed"*/"gpointer ", "gpointer", NULL, -1 },
	{ TRUE, "OBJECT",	"GObject *",	"GObject",	"*",	-1 },
	{ TRUE, "PARAM",	"GParamSpec *",	"GParamSpec",	"*",	-1 },

	/* FIXME: VALUE_ARRAY, CLOSURE */
	/* Note that those have some issues with g_value_ calls etc... so
	 * we can't just add them */

	/* Do we need this??? */
#if 0
	{ FALSE, "SIGNAL",	/*"GtkSignal"*/"___twopointertype ",
		SPECIAL_2POINTER },
	{ FALSE, "ARGS",	/*"GtkArgs"*/"___intpointertype ",
		SPECIAL_INT_POINTER },
	{ FALSE, "CALLBACK",	/*"GtkCallback"*/"___threepointertype ",
		SPECIAL_3POINTER },
	{ FALSE, "C_CALLBACK",	/*"GtkCCallback"*/"___twopointertype ",
		SPECIAL_2POINTER },
	{ FALSE, "FOREIGN",	/*"GtkForeign"*/"___twopointertype ",
		SPECIAL_2POINTER },
#endif

	{ FALSE, NULL, NULL }
};

static GHashTable *type_hash = NULL;

static void
init_type_hash(void)
{
	int i;

	if(type_hash) return;

	type_hash = g_hash_table_new(g_str_hash, g_str_equal);

	for(i=0; our_gtk_type_table[i].gtkname; i++)
		g_hash_table_insert(type_hash,
				    our_gtk_type_table[i].gtkname,
				    (gpointer)&our_gtk_type_table[i]);
}

const char *
get_cast (const char *type, gboolean simple_only)
{
	OurGtkType *gtype;

	init_type_hash ();

	if(strncmp(type, "BOXED_", 6) == 0)	 
	  gtype = g_hash_table_lookup (type_hash, "BOXED");
	else
	  gtype = g_hash_table_lookup (type_hash, type);

	if (gtype == NULL ||
	    (simple_only &&
	     ! gtype->simple))
		return NULL;

	return gtype->cast;
}

Type *
get_tree_type (const char *type, gboolean simple_only)
{
	OurGtkType *gtype;
	Node *node;

	init_type_hash ();

	gtype = g_hash_table_lookup (type_hash, type);

	if (gtype == NULL ||
	    (simple_only &&
	     ! gtype->simple))
		return NULL;

	node = node_new (TYPE_NODE,
			 "name", gtype->type_name,
			 "pointer", gtype->type_pointer,
			 NULL);

	return (Type *)node;
}

static void
mask_special_array (const char *type, gboolean *special_array, gboolean *any_special)
{
	OurGtkType *gtype;

	init_type_hash();

	gtype = g_hash_table_lookup(type_hash, type);

	if(gtype && gtype->special >= 0) {
		special_array[gtype->special] = TRUE;
		*any_special = TRUE;
	}
}

gboolean
setup_special_array(Class *c, gboolean *special_array)
{
	GList *li;
	gboolean any_special = FALSE;

	memset(special_array, 0, sizeof(gboolean)*SPECIAL_LAST);

	for(li=c->nodes; li; li=g_list_next(li)) {
		Node *n = li->data;
		if(n->type == METHOD_NODE) {
			Method *m = (Method *)n;
			GList *l;
			if(m->method != SIGNAL_LAST_METHOD &&
			   m->method != SIGNAL_FIRST_METHOD)
				continue;

			for(l=m->gtktypes; l; l=l->next)
				mask_special_array(l->data, special_array,
						   &any_special);
		} else if(n->type == ARGUMENT_NODE) {
			Argument *a = (Argument *)n;
			mask_special_array(a->gtktype, special_array,
					   &any_special);
		}
	}

	return any_special;
}

char *
get_type (const Type *t, gboolean postfix_to_stars)
{
	char *s;
	int i;
	int extra;
	GString *gs;

	s = remove_sep(t->name);
	gs = g_string_new(s);
	g_free(s);

	extra = 0;
	if (postfix_to_stars) {
		const char *p;
		/*XXX: this is ugly perhaps we can do this whole postfix thing
		  in a nicer way, we just count the number of '[' s and from
		  that we deduce the number of dimensions, so that we can print
		  that many stars */
		for (p = t->postfix; p && *p; p++)
			if(*p == '[') extra++;
	}
	g_string_append_c(gs, ' ');

	if (t->pointer != NULL) {
		g_string_append (gs, t->pointer);
		for (i=0; i < extra; i++)
			g_string_append_c (gs, '*');
		g_string_append_c (gs, ' ');
	}
	
	return g_string_free (gs, FALSE);
}

char *
gob_strup (char *str)
{
	char *s;
	for (s = str; *s; s++)
		*s = g_ascii_toupper (*s);

	return str;
}

char *
gob_strdown (char *str)
{
	char *s;
	for (s = str; *s; s++)
		*s = g_ascii_tolower (*s);

	return str;
}

char *
gob_str_delete_quotes(char *str)
{
	char *p, *i;
	p=i=str;
	while(*p!='\0')
	{
		if(*p=='\"')
		{
			p++;
			continue;
		}
		*i=*p;
		p++; i++;
	}
	*i=*p;
	return str;
}

char *
make_me_type (const char *type, const char *alt)
{
	if (type == NULL)
		return g_strdup (alt);
	/* HACK!  just in case someone made this
	 * work with 2.0.0 by using the TYPE
	 * macro directly */
	if ((strstr (type, "_TYPE_") != NULL ||
	     strstr (type, "TYPE_") == type) &&
	    strchr (type, ':') == NULL)
		return g_strdup (type);
	return make_pre_macro (type, "TYPE");
}
