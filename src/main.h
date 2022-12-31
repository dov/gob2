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

#ifndef _MAIN_H_
#define _MAIN_H_

enum {
	PRIVATE_HEADER_ALWAYS,
	PRIVATE_HEADER_ONDEMAND,
	PRIVATE_HEADER_NEVER
};

extern gboolean no_touch_headers;
extern gboolean for_cpp;
extern gboolean no_gnu;
extern gboolean exit_on_warn;
extern gboolean exit_on_error;
extern gboolean got_error;
extern gint private_header;
extern gboolean no_extern_c;
extern gboolean no_write;
extern gboolean no_lines;
extern gboolean no_self_alias;
extern gboolean no_kill_underscores;
extern gboolean always_private_struct;
extern gint prealloc;

extern char *filename;
extern char *filebase;

extern FILE *out;
extern FILE *outh;
extern FILE *outph;

extern int method_unique_id;


#endif
