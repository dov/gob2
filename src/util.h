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

#ifndef UTIL_H
#define UTIL_H

enum {
	GOB_WARN,
	GOB_ERROR
};

/* print an error, or warning referring to 'line'
 * (line can be 0 or negative for no line) */
void error_print(int type, int line, const char *error);
void error_printf(int type, int line, const char *error, ...) G_GNUC_PRINTF (3, 4);

/* remove the : separator from a typename */
char * remove_sep(const char *base);
/* replace the : separator from a typename with a different character*/
char * replace_sep(const char *base, char r);
char * gob_strup (char *s);
char * gob_strdown (char *s);
char * gob_str_delete_quotes(char *str);

/*separate the namespace part and then replace rest of
  separators with r*/
void separns_replace_sep(const char *base, char **ns, char **name, char r);

/* make a macro with some prefix before the name but after
   namespace */
char * make_pre_macro(const char *base,const  char *pre);

/* get a name usable for a cast from a GObject (without G_TYPE_)*/
const char * get_cast (const char *type, gboolean simple_only);
Type * get_tree_type (const char *type, gboolean simple_only);
char * get_type (const Type *t, gboolean postfix_to_stars);

enum {
	SPECIAL_2POINTER,
	SPECIAL_3POINTER,
	SPECIAL_INT_POINTER,
	SPECIAL_LAST
};
/* returns TRUE if there are any special types at all */
gboolean setup_special_array(Class *c, gboolean *special_array);

char * make_me_type (const char *type, const char *alt);

#endif /* UTIL_H */
