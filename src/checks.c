/* GOB C Preprocessor
 * Copyright (C) 1999-2000 the Free Software Foundation.
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2001-2004 George Lebl
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
#include <string.h>
#include <stdio.h>
#include <glib.h>

#include "treefuncs.h"
#include "main.h"
#include "util.h"

#include "checks.h"

static void
check_duplicate (Class *c, Node *node, const char *id, int line_no)
{
	GList *li;
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		const char *nid;
		int nline_no;
		if(n->type == METHOD_NODE) {
			Method *m = (Method *)n;

			/* override methods are checked separately */
			if(m->method == OVERRIDE_METHOD)
				continue;

			nid = m->id;
			nline_no = m->line_no;
		} else if(n->type == VARIABLE_NODE) {
			Variable *v = (Variable *)n;
			nid = v->id;
			nline_no = v->line_no;
		} else
			continue;
		if(n == node ||
		   line_no > nline_no ||
		   n->type != node->type ||
		   strcmp(nid, id) != 0)
			continue;

		error_printf (GOB_ERROR, nline_no,
			      "symbol '%s' redefined, "
			      "first defined on line %d",
			      id, line_no);
	}
}

void
check_duplicate_symbols (Class *c)
{
	GList *li;
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;

		if (n->type == METHOD_NODE) {
			Method *m = (Method *)n;
			/* override methods are checked separately */
			if (m->method != OVERRIDE_METHOD) {
				check_duplicate (c, n, m->id, m->line_no);
			}
		} else if (n->type == VARIABLE_NODE) {
			Variable *v = (Variable *)n;
			check_duplicate (c, n, v->id, v->line_no);
		}
	}
}

static void
check_duplicate_override (Class *c, Method *method)
{
	GList *li;
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		Method *m = (Method *)n;
		if(n->type != METHOD_NODE ||
		   m->method != OVERRIDE_METHOD)
			continue;

		if(method == m ||
		   method->line_no > m->line_no ||
		   strcmp(m->id, method->id) != 0 ||
		   strcmp(m->otype, method->otype) != 0)
			continue;
		error_printf(GOB_ERROR, m->line_no,
			     "override '%s(%s)' redefined, "
			     "first defined on line %d",
			     m->id, m->otype, method->line_no);
	}
}

void
check_duplicate_overrides(Class *c)
{
	GList *li;
	for(li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		Method *m = (Method *)n;
		if(n->type != METHOD_NODE ||
		   m->method != OVERRIDE_METHOD)
			continue;
		check_duplicate_override(c, m);
	}
}

void
check_bad_symbols(Class *c)
{
	GList *li;
	for(li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if(n->type == METHOD_NODE) {
			Method *m = (Method *)n;
			if((m->method == SIGNAL_LAST_METHOD ||
			    m->method == SIGNAL_FIRST_METHOD ||
			    m->method == VIRTUAL_METHOD) &&
			   (strcmp(m->id, "_epv")==0 ||
			    strcmp(m->id, "__parent__")==0 ||
			    strcmp(m->id, "___parent__")==0)) {
				error_printf(GOB_ERROR, m->line_no,
					     "'%s' not allowed as an "
					     "identifier of signal "
					     "or virtual methods",
					     m->id);
			}
			if(m->method != INIT_METHOD &&
			   m->method != CLASS_INIT_METHOD &&
			   (strcmp(m->id, "init")==0 ||
			    strcmp(m->id, "class_init")==0)) {
				error_print(GOB_ERROR, m->line_no,
					    "init, or class_init not "
					    "allowed as an "
					    "identifier of non-"
					    "constructor methods");
			}
		} else if(n->type == VARIABLE_NODE) {
			Variable *v = (Variable *)n;
			if(strcmp(v->id, "_priv")==0 ||
			   strcmp(v->id, "__parent__")==0) {
				error_printf(GOB_ERROR, v->line_no,
					     "'%s' not allowed as a "
					     "data member name", v->id);
			}
		}
	}
}

static void
check_duplicate_named (Class *c, Node *node, const char *id, int line_no)
{
	GList *li;
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		const char *nid;
		int nline_no;
		if (n->type == METHOD_NODE) {
			Method *m = (Method *)n;
			if (m->method == SIGNAL_LAST_METHOD ||
			    m->method == SIGNAL_FIRST_METHOD) {
				nid = m->id;
				nline_no = m->line_no;
			} else {
				continue;
			}
		} else if (n->type == ARGUMENT_NODE) {
			Argument *a = (Argument *)n;
			nid = a->name;
			nline_no = a->line_no;
		} else if (n->type == PROPERTY_NODE) {
			Property *p = (Property *)n;
			nid = p->name;
			nline_no = p->line_no;
		} else {
			continue;
		}
		if (n == node ||
		    line_no >= nline_no ||
		    g_strcasecmp (nid, id) != 0)
			continue;
		error_printf (GOB_ERROR, nline_no,
			      "named symbol (argument or signal) '%s' "
			      "redefined, first defined on line %d "
			      "(case insensitive)",
			      id, line_no);
	}
}

void
check_duplicate_signals_args (Class *c)
{
	GList *li;
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if (n->type == METHOD_NODE) {
			Method *m = (Method *)n;
			if(m->method == SIGNAL_LAST_METHOD ||
			   m->method == SIGNAL_FIRST_METHOD)
				check_duplicate_named (c, n, m->id,
						       m->line_no);
		} else if (n->type == PROPERTY_NODE) {
			Property *p = (Property *)n;
			check_duplicate_named (c, n, p->name, p->line_no);
		} else if (n->type == ARGUMENT_NODE) {
			Argument *a = (Argument *)n;
			check_duplicate_named (c, n, a->name, a->line_no);
		}
	}
}

void
check_public_new(Class *c)
{
	GList *li;
	for(li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if(n->type == METHOD_NODE) {
			Method *m = (Method *)n;
			if((strcmp(m->id, "new")==0) &&
			   (m->method != REGULAR_METHOD ||
			    m->scope != PUBLIC_SCOPE))
				error_print(GOB_WARN, m->line_no,
					    "'new' should be a regular\n"
					    "public method");
		}
	}
}

void
check_vararg(Class *c)
{
	GList *li;
	for(li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if(n->type == METHOD_NODE) {
			Method *m = (Method *)n;
			if( ! m->vararg)
				continue;
			if(m->method == OVERRIDE_METHOD ||
			   m->method == SIGNAL_LAST_METHOD ||
			   m->method == SIGNAL_FIRST_METHOD ||
			   m->method == VIRTUAL_METHOD) {
				error_print(GOB_ERROR, m->line_no,
					    "signals, overrides and virtuals, "
					    "can't have variable argument "
					    "lists");
			}
		}
	}
}

void
check_firstarg(Class *c)
{
	GList *li;
	for(li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if(n->type == METHOD_NODE) {
			Method *m = (Method *)n;
			if(m->args)
				continue;
			if(m->method == OVERRIDE_METHOD ||
			   m->method == SIGNAL_LAST_METHOD ||
			   m->method == SIGNAL_FIRST_METHOD ||
			   m->method == VIRTUAL_METHOD) {
				error_print(GOB_ERROR, m->line_no,
					    "signals, overrides and virtuals, "
					    "can't have no arguments");
			}
		}
	}
}

void
check_nonvoidempty(Class *c)
{
	GList *li;
	for(li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if(n->type == METHOD_NODE) {
			Method *m = (Method *)n;
			if(m->method != REGULAR_METHOD)
				continue;
			if(!(strcmp(m->mtype->name, "void")==0 &&
			     m->mtype->pointer == NULL) &&
			   !m->cbuf) {
				error_print(GOB_WARN, m->line_no,
					    "non-void empty method found, "
					    "regular non-void function should "
					    "not be empty.");
				/* add a body here, so that the user will also
				   get a warning from gcc, and so that it will
				   at least point him to the prototype of the
				   function in the .gob file */
				m->cbuf = g_strdup("/*empty*/");
				m->ccode_line = m->line_no;
			}
		}
	}
}

void
check_signal_args (Class *c)
{
	GList *li;
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if(n->type == METHOD_NODE) {
			Method *m = (Method *)n;
			GList *l;
			if(m->method != SIGNAL_LAST_METHOD &&
			   m->method != SIGNAL_FIRST_METHOD)
				continue;

			for (l = m->gtktypes; l != NULL; l = l->next) {
				if (strcmp (l->data, "BOXED") == 0) {
 					error_printf (GOB_ERROR, m->line_no,
 						      "BOXED not allowed as "
						      "a signal argument, use "
 						      "POINTER, or BOXED_*");
 					continue;
				} else if (strcmp (l->data, "FLAGS") == 0) {
					error_printf (GOB_ERROR, m->line_no,
						      "FLAGS not allowed as "
						      "a signal argument, use "
						      "UINT");
					continue;
				}
				if (get_cast (l->data, FALSE))
					continue;
				error_printf (GOB_ERROR, m->line_no,
					      "Unknown GTK+ type '%s' "
					      "among signal types",
					      (char *)l->data);
			}
		}
	}
}

void
check_argument_types (Class *c)
{
	GList *li;
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if(n->type == ARGUMENT_NODE) {
			Argument *a = (Argument *)n;
			if(get_cast(a->gtktype, FALSE))
				continue;
			error_printf(GOB_ERROR, a->line_no,
				     "Unknown GLib type '%s' "
				     "as argument type",
				     a->gtktype);
		}
	}
}

void
check_property_types (Class *c)
{
	GList *li;
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if (n->type == PROPERTY_NODE) {
			Property *p = (Property *)n;
			if (get_cast (p->gtktype, FALSE))
				continue;
			error_printf (GOB_ERROR, p->line_no,
				      "Unknown GLib type '%s' "
				      "as property type",
				      p->gtktype);
		}
	}
}

static void
check_func_arg_check_func_arg(Method *m, FuncArg *fa)
{
	GList *li;
	char *s;

	if( ! fa->checks)
		return;

	if(strcmp(fa->atype->name, "void") == 0 &&
	   fa->atype->pointer == NULL) {
		error_print(GOB_ERROR, m->line_no,
			    "Running checks on a void function argument");
		return;
	}
	
	for(li = fa->checks; li; li = li->next) {
		Check *ch = li->data;
		if(ch->chtype == TYPE_CHECK) {
			char *p;
			gboolean got_type = FALSE;
			s = g_strdup(fa->atype->name);
			p = strtok(s, " ");
			if( ! p) {
				g_free(s);
				goto type_check_error;
			}
			while(p) {
				if(strcmp(p, "const") != 0) {
					if(got_type) {
						g_free(s);
						goto type_check_error;
					}
					got_type = TRUE;
				}
				p = strtok(NULL, " ");
			}
			g_free(s);
			if( ! got_type)
				goto type_check_error;

			if(fa->atype->pointer == NULL ||
			   (strcmp(fa->atype->pointer, "*") != 0 &&
			    strcmp(fa->atype->pointer, "* const") != 0 &&
			    strcmp(fa->atype->pointer, "const *") != 0))
				goto type_check_error;
		}
	}
	return;

type_check_error:
	if(fa->atype->pointer)
		error_printf(GOB_ERROR, m->line_no,
			     "Cannot check the type of '%s %s'",
			     fa->atype->name, fa->atype->pointer);
	else
		error_printf(GOB_ERROR, m->line_no,
			     "Cannot check the type of '%s'",
			     fa->atype->name);
}

static void
check_func_arg_check_method(Method *m)
{
	GList *li;
	for(li = m->args; li; li = li->next) {
		FuncArg *fa = li->data;
		check_func_arg_check_func_arg(m, fa);
	}
}

void
check_func_arg_checks(Class *c)
{
	GList *li;
	for(li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if(n->type == METHOD_NODE) {
			Method *m = (Method *)n;
			check_func_arg_check_method(m);
		}
	}
}

void
check_func_attrs(Class *c)
{
  GList *li;
  for (li = c->nodes; li != NULL; li = li->next) {
    Node *n = li->data;
    if (n->type == METHOD_NODE) {
      Method *m = (Method *)n;
      if ((m->method == INIT_METHOD ||
	   m->method == CLASS_INIT_METHOD)
	  && (m->funcattrs != NULL && strlen(m->funcattrs) != 0)) {
	/* This is actually dead code at the moment, since the parser
	   doesn't accept attributes to the init or class_init
	   syntactic forms anyway.  But it could easily be made to do
	   so, and also for virtual override and signal methods, and
	   then we could give kinder error messages here.  */
	error_print (GOB_ERROR, m->line_no,
		     "function attributes (G_GNUC_PRINTF, etc.) aren't "
                     "supported for the init or class_init methods");
      }
    }
  }
}

void
check_for_class_destructors (Class *c)
{
	GList *li;
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if (n->type == VARIABLE_NODE) {
			Variable *v = (Variable *)n;
			if (v->destructor != NULL &&
			    v->scope == CLASS_SCOPE) {
				error_print (GOB_WARN, v->line_no,
					     "classwide members cannot have "
					     "destructors since the classes "
					     "are static and never get "
					     "destroyed anyway");
			}
		}
	}
}

int
count_signals(Class *c)
{
	int num = 0;
	GList *li;
	for(li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if(n->type == METHOD_NODE) {
			Method *m = (Method *)n;
			if(m->method == SIGNAL_LAST_METHOD ||
			   m->method == SIGNAL_FIRST_METHOD)
				num++;
		}
	}
	return num;
}

int
count_set_properties (Class *c)
{
	int num = 0;
	GList *li;
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		Property *p = li->data;
		if (n->type == PROPERTY_NODE &&
		    p->set != NULL)
			num ++;
	}
	return num;
}

int
count_get_properties (Class *c)
{
	int num = 0;
	GList *li;
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		Property *p = li->data;
		if (n->type == PROPERTY_NODE &&
		    p->get != NULL)
			num ++;
	}
	return num;
}


int
count_set_arguments(Class *c)
{
	int num = 0;
	GList *li;
	for(li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		Argument *a = li->data;
		if(n->type == ARGUMENT_NODE &&
		   a->set)
			num ++;
	}
	return num;
}

int
count_get_arguments(Class *c)
{
	int num = 0;
	GList *li;
	for(li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		Argument *a = li->data;
		if(n->type == ARGUMENT_NODE &&
		   a->get)
			num ++;
	}
	return num;
}

int
count_overrides(Class *c)
{
	int num = 0;
	GList *li;
	for(li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if(n->type == METHOD_NODE) {
			Method *m = (Method *)n;
			if(m->method == OVERRIDE_METHOD)
				num++;
		}
	}
	return num;
}

int
count_privates(Class *c)
{
	int num = 0;
	GList *li;
	for(li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if(n->type == VARIABLE_NODE) {
			Variable *v = (Variable *)n;
			if(v->scope == PRIVATE_SCOPE)
				num++;
		}
	}
	return num;
}

int
count_protecteds (Class *c)
{
	int num = 0;
	GList *li;
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if(n->type == METHOD_NODE) {
			Method *m = (Method *)n;
			if(m->scope == PROTECTED_SCOPE)
				num++;
		}
	}
	return num;
}

int
count_unreftors (Class *c)
{
	int num = 0;
	GList *li;
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if (n->type == VARIABLE_NODE) {
			Variable *v = (Variable *)n;
			if (v->destructor != NULL &&
			    v->destructor_unref &&
			    v->scope != CLASS_SCOPE)
				num++;
		}
	}
	return num;
}

int
count_destructors (Class *c)
{
	int num = 0;
	GList *li;
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if (n->type == VARIABLE_NODE) {
			Variable *v = (Variable *)n;
			if (v->destructor != NULL &&
			    ! v->destructor_unref &&
			    v->scope != CLASS_SCOPE)
				num++;
		}
	}
	return num;
}

int
count_initializers (Class *c)
{
	int num = 0;
	GList *li;
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if (n->type == VARIABLE_NODE) {
			Variable *v = (Variable *)n;
			if (v->initializer != NULL)
				num++;
		}
	}
	return num;
}

int
count_glade_widgets (Class *c)
{
	int num = 0;
	GList *li;
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		if (n->type == VARIABLE_NODE) {
			Variable *v = (Variable *)n;
			if (v->glade_widget)
				num++;
		}
	}
	return num;
}

gboolean
find_get_type (Class *c)
{
	GList *li;
	for (li = c->nodes; li != NULL; li = li->next) {
		Node *n = li->data;
		Method *m = (Method *)n;
		if (n->type == METHOD_NODE &&
		    strcmp (m->id, "get_type") == 0) {
			if (m->method != REGULAR_METHOD ||
			    m->scope != PUBLIC_SCOPE ||
			    m->args != NULL) {
				error_printf (GOB_ERROR, m->line_no,
					      "get_type method must be a "
					      "regular public method with "
					      "no arguments");
			}
			return TRUE;
		}
	}

	return FALSE;
}
