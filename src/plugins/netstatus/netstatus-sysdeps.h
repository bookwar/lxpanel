/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2004 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *    Erwann Chenede  <erwann.chenede@sun.com>
 *    Mark McLoughlin  <mark@skynet.ie>  
 */

#ifndef __NETSTATUS_SYSDEPS_H__
#define __NETSTATUS_SYSDEPS_H__

#include <glib/gtypes.h>
#include <glib/gmacros.h>

G_BEGIN_DECLS

char *netstatus_sysdeps_read_iface_statistics       (const char *iface,
						     gulong     *in_packets,
						     gulong     *out_packets,
						     gulong     *in_bytes,
						     gulong     *out_bytes);
char *netstatus_sysdeps_read_iface_wireless_details (const char *iface,
						     gboolean   *is_wireless,
						     int        *signal_strength);

G_END_DECLS

#endif /* __NETSTATUS_SYSDEPS_H__ */
