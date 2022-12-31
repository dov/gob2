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

#ifndef CHECKS_H
#define CHECKS_H

void		check_duplicate_symbols		(Class *c);
void		check_duplicate_overrides	(Class *c);
void		check_bad_symbols		(Class *c);
void		check_duplicate_signals_args	(Class *c);
void		check_public_new		(Class *c);
void		check_vararg			(Class *c);
void		check_firstarg			(Class *c);
void		check_nonvoidempty		(Class *c);
void		check_signal_args		(Class *c);
void		check_argument_types		(Class *c);
void		check_property_types		(Class *c);
void		check_func_arg_checks		(Class *c);
void            check_func_attrs                (Class *c);
void		check_for_class_destructors	(Class *c);

int		count_signals			(Class *c);
int		count_set_properties		(Class *c);
int		count_get_properties		(Class *c);
int		count_set_arguments		(Class *c);
int		count_get_arguments		(Class *c);
int		count_overrides			(Class *c);
int		count_privates			(Class *c);
int		count_protecteds 		(Class *c);
int		count_unreftors 		(Class *c);
int		count_destructors 		(Class *c);
int		count_initializers 		(Class *c);
int		count_glade_widgets 		(Class *c);
gboolean	find_get_type 			(Class *c);

#endif
