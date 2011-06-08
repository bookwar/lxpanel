/*
 *      batt_sys.h
 *      
 *      Copyright 2009 Juergen Hötzel <juergen@archlinux.org>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */


#ifndef BATT_SYS_H
#define BATT_SYS_H


#define BUF_SIZE 1024
#define ACPI_PATH_SYS_POWER_SUPPY   "/sys/class/power_supply"
#define MIN_CAPACITY	 0.01
#define MIN_PRESENT_RATE 0.01
#define BATTERY_DESC	"Battery"

#include <glib.h>

typedef struct battery {
    int battery_num;
    /* path to battery dir */
    const gchar *path;			
    int remaining_capacity;
    int remaining_energy;
    int present_rate;
    int voltage;
    int design_capacity;
    int design_capacity_unit;
    int last_capacity;
    int last_capacity_unit;
    int hours, minutes, seconds;
    int percentage;
    char *state, *poststr;
    char* capacity_unit;
    int type_battery;
} battery;

battery *battery_get();
void battery_update( battery *b );
void battery_print(battery *b, int show_capacity);
gboolean battery_is_charging( battery *b );
gint battery_get_remaining( battery *b );

#endif
