/**
 * CPU usage plugin to lxpanel
 *
 * Copyright (c) 2008 LxDE Developers, see the file AUTHORS for details.
 * Copyright (C) 2004 by Alexandre Pereira da Silva <alexandre.pereira@poli.usp.br>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
/*A little bug fixed by Mykola <mykola@2ka.mipt.ru>:) */

#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#include "plugin.h"
#include "panel.h"
#include "misc.h"

#define BORDER_SIZE 2

#include "dbg.h"

typedef unsigned long CPUTick;			/* Value from /proc/stat */
typedef float CPUSample;			/* Saved CPU utilization value as 0.0..1.0 */

struct cpu_stat {
    CPUTick u, n, s, i;				/* User, nice, system, idle */
};

/* Private context for CPU plugin. */
typedef struct {
    GdkGC * graphics_context;			/* Graphics context for drawing area */
    GdkColor foreground_color;			/* Foreground color for drawing area */

    GdkGC * bg_graphics_context;		/* Graphics context for drawing background */
    GdkColor background_color;			/* Background color for drawing area */

    GtkWidget * da;				/* Drawing area */
    GdkPixmap * pixmap;				/* Pixmap to be drawn on drawing area */

    guint timer;				/* Timer for periodic update */
    CPUSample * stats_cpu;			/* Ring buffer of CPU utilization values */
    unsigned int ring_cursor;			/* Cursor for ring buffer */
    int pixmap_width;				/* Width of drawing area pixmap; also size of ring buffer; does not include border size */
    int pixmap_height;				/* Height of drawing area pixmap; does not include border size */
    struct cpu_stat previous_cpu_stat;		/* Previous value of cpu_stat */

    char * fg_color;
    char * bg_color;
    int update_interval;
} CPUPlugin;

static void redraw_pixmap(CPUPlugin * c);
static gboolean cpu_update(CPUPlugin * c);
static gboolean configure_event(GtkWidget * widget, GdkEventConfigure * event, CPUPlugin * c);
static gboolean expose_event(GtkWidget * widget, GdkEventExpose * event, CPUPlugin * c);
static int cpu_constructor(Plugin * p, char ** fp);
static void cpu_destructor(Plugin * p);
static void cpu_save_configuration(Plugin * p, FILE * fp);

/* Redraw after timer callback or resize. */
static void redraw_pixmap(CPUPlugin * c)
{
    /* Erase pixmap. */
    gdk_draw_rectangle(c->pixmap, c->bg_graphics_context, TRUE, 0, 0, c->pixmap_width, c->pixmap_height);

    /* Recompute pixmap. */
    unsigned int i;
    unsigned int drawing_cursor = c->ring_cursor;
    for (i = 0; i < c->pixmap_width; i++)
    {
        /* Draw one bar of the CPU usage graph. */
        if (c->stats_cpu[drawing_cursor] != 0.0)
            gdk_draw_line(c->pixmap, c->graphics_context,
                i, c->pixmap_height,
                i, c->pixmap_height - c->stats_cpu[drawing_cursor] * c->pixmap_height);

        /* Increment and wrap drawing cursor. */
        drawing_cursor += 1;
	if (drawing_cursor >= c->pixmap_width)
            drawing_cursor = 0;
    }

    /* Redraw pixmap. */
    gtk_widget_queue_draw(c->da);
}

/* Periodic timer callback. */
static gboolean cpu_update(CPUPlugin * c)
{
    if ((c->stats_cpu != NULL) && (c->pixmap != NULL))
    {
        /* Open statistics file and scan out CPU usage. */
        struct cpu_stat cpu;
        FILE * stat = fopen("/proc/stat", "r");
        if (stat == NULL)
            return TRUE;
        int fscanf_result = fscanf(stat, "cpu %lu %lu %lu %lu", &cpu.u, &cpu.n, &cpu.s, &cpu.i);
        fclose(stat);

        /* Ensure that fscanf succeeded. */
        if (fscanf_result == 4)
        {
            /* Compute delta from previous statistics. */
            struct cpu_stat cpu_delta;
            cpu_delta.u = cpu.u - c->previous_cpu_stat.u;
            cpu_delta.n = cpu.n - c->previous_cpu_stat.n;
            cpu_delta.s = cpu.s - c->previous_cpu_stat.s;
            cpu_delta.i = cpu.i - c->previous_cpu_stat.i;

            /* Copy current to previous. */
            memcpy(&c->previous_cpu_stat, &cpu, sizeof(struct cpu_stat));

            /* Compute user+nice+system as a fraction of total.
             * Introduce this sample to ring buffer, increment and wrap ring buffer cursor. */
            float cpu_uns = cpu_delta.u + cpu_delta.n + cpu_delta.s;
            c->stats_cpu[c->ring_cursor] = cpu_uns / (cpu_uns + cpu_delta.i);
            c->ring_cursor += 1;
            if (c->ring_cursor >= c->pixmap_width)
                c->ring_cursor = 0;

            /* Redraw with the new sample. */
            redraw_pixmap(c);
        }
    }
    return TRUE;
}

/* Handler for configure_event on drawing area. */
static gboolean configure_event(GtkWidget * widget, GdkEventConfigure * event, CPUPlugin * c)
{
    /* Allocate pixmap and statistics buffer without border pixels. */
    int new_pixmap_width = widget->allocation.width - BORDER_SIZE * 2;
    int new_pixmap_height = widget->allocation.height - BORDER_SIZE * 2;
    if ((new_pixmap_width > 0) && (new_pixmap_height > 0))
    {
        /* If statistics buffer does not exist or it changed size, reallocate and preserve existing data. */
        if ((c->stats_cpu == NULL) || (new_pixmap_width != c->pixmap_width))
        {
            CPUSample * new_stats_cpu = g_new0(typeof(*c->stats_cpu), new_pixmap_width);
            if (c->stats_cpu != NULL)
            {
                if (new_pixmap_width > c->pixmap_width)
                {
                    /* New allocation is larger.
                     * Introduce new "oldest" samples of zero following the cursor. */
                    memcpy(&new_stats_cpu[0],
                        &c->stats_cpu[0], c->ring_cursor * sizeof(CPUSample));
                    memcpy(&new_stats_cpu[new_pixmap_width - c->pixmap_width + c->ring_cursor],
                        &c->stats_cpu[c->ring_cursor], (c->pixmap_width - c->ring_cursor) * sizeof(CPUSample));
                }
                else if (c->ring_cursor <= new_pixmap_width)
                {
                    /* New allocation is smaller, but still larger than the ring buffer cursor.
                     * Discard the oldest samples following the cursor. */
                    memcpy(&new_stats_cpu[0],
                        &c->stats_cpu[0], c->ring_cursor * sizeof(CPUSample));
                    memcpy(&new_stats_cpu[c->ring_cursor],
                        &c->stats_cpu[c->pixmap_width - new_pixmap_width + c->ring_cursor], (new_pixmap_width - c->ring_cursor) * sizeof(CPUSample));
                }
                else
                {
                    /* New allocation is smaller, and also smaller than the ring buffer cursor.
                     * Discard all oldest samples following the ring buffer cursor and additional samples at the beginning of the buffer. */
                    memcpy(&new_stats_cpu[0],
                        &c->stats_cpu[c->ring_cursor - new_pixmap_width], new_pixmap_width * sizeof(CPUSample));
                    c->ring_cursor = 0;
                }
                g_free(c->stats_cpu);
            }
            c->stats_cpu = new_stats_cpu;
        }

        /* Allocate or reallocate pixmap. */
        c->pixmap_width = new_pixmap_width;
        c->pixmap_height = new_pixmap_height;
        if (c->pixmap)
            g_object_unref(c->pixmap);
        c->pixmap = gdk_pixmap_new(widget->window, c->pixmap_width, c->pixmap_height, -1);

        /* Redraw pixmap at the new size. */
        redraw_pixmap(c);
    }
    return TRUE;
}

/* Handler for expose_event on drawing area. */
static gboolean expose_event(GtkWidget * widget, GdkEventExpose * event, CPUPlugin * c)
{
    /* Draw the requested part of the pixmap onto the drawing area.
     * Translate it in both x and y by the border size. */
    if (c->pixmap != NULL)
    {
        gdk_draw_drawable (widget->window,
              c->da->style->black_gc,
              c->pixmap,
              event->area.x, event->area.y,
              event->area.x + BORDER_SIZE, event->area.y + BORDER_SIZE,
              event->area.width, event->area.height);
    }
    return FALSE;
}

/* Callback when the configuration dialog has recorded a configuration change. */
static void cpu_apply_configuration(Plugin * p)
{
    CPUPlugin * c = (CPUPlugin *) p->priv;

    /* Allocate top level widget and set into Plugin widget pointer. */
    if (!p->pwid)
    {
        p->pwid = gtk_event_box_new();
        gtk_container_set_border_width(GTK_CONTAINER(p->pwid), 1);
        GTK_WIDGET_SET_FLAGS(p->pwid, GTK_NO_WINDOW);
    }

    /* Allocate drawing area as a child of top level widget.  Enable button press events. */
    if (!c->da)
    {
        c->da = gtk_drawing_area_new();
        gtk_widget_set_size_request(c->da, 40, PANEL_HEIGHT_DEFAULT);
        gtk_widget_add_events(c->da, GDK_BUTTON_PRESS_MASK);
        gtk_container_add(GTK_CONTAINER(p->pwid), c->da);

        /* Connect signals. */
        g_signal_connect(G_OBJECT(c->da), "configure_event", G_CALLBACK(configure_event), (gpointer) c);
        g_signal_connect(G_OBJECT(c->da), "expose_event", G_CALLBACK(expose_event), (gpointer) c);
        g_signal_connect(c->da, "button-press-event", G_CALLBACK(plugin_button_press_event), p);
    }

    /* Clone a graphics context and set "green" as its foreground color.
     * We will use this to draw the graph. */
    if (c->graphics_context)
        g_object_unref(c->graphics_context);
    c->graphics_context = gdk_gc_new(p->panel->topgwin->window);

    gdk_color_parse(c->fg_color,  &c->foreground_color);
    gdk_colormap_alloc_color(gdk_drawable_get_colormap(p->panel->topgwin->window), &c->foreground_color, FALSE, TRUE);
    gdk_gc_set_foreground(c->graphics_context, &c->foreground_color);

    if (c->bg_graphics_context)
        g_object_unref(c->bg_graphics_context);
    c->bg_graphics_context = gdk_gc_new(p->panel->topgwin->window);

    gdk_color_parse(c->bg_color,  &c->background_color);
    gdk_colormap_alloc_color(gdk_drawable_get_colormap(p->panel->topgwin->window), &c->background_color, FALSE, TRUE);
    gdk_gc_set_foreground(c->bg_graphics_context, &c->background_color);

    /* Show the widget.  Connect a timer to refresh the statistics. */
    gtk_widget_show(c->da);
    if (c->timer)
        g_source_remove(c->timer);
    c->timer = g_timeout_add(c->update_interval, (GSourceFunc) cpu_update, (gpointer) c);
}

/* Plugin constructor. */
static int cpu_constructor(Plugin * p, char ** fp)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    CPUPlugin * c = g_new0(CPUPlugin, 1);
    p->priv = c;

    c->fg_color = NULL;
    c->bg_color = NULL;
    c->update_interval = 1500;

    /* Load parameters from the configuration file. */
    line s;
    s.len = 256;
    if (fp)
    {
        while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END)
        {
            if (s.type == LINE_NONE)
            {
                ERR( "cpu: illegal token %s\n", s.str);
                return 0;
            }
            if (s.type == LINE_VAR)
            {
                if (g_ascii_strcasecmp(s.t[0], "FgColor") == 0)
                    c->fg_color = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "BgColor") == 0)
                    c->bg_color = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "UpdateInterval") == 0)
                    c->update_interval = atoi(s.t[1]);
                else
                    ERR( "dclock: unknown var %s\n", s.t[0]);
            }
            else
            {
                ERR( "dclock: illegal in this context %s\n", s.str);
                return 0;
            }
        }

    }


    #define DEFAULT_STRING(f, v) \
      if (c->f == NULL) \
          c->f = g_strdup(v);

    DEFAULT_STRING(fg_color, "green");
    DEFAULT_STRING(bg_color, "black");

    #undef DEFAULT_STRING

    cpu_apply_configuration(p);

    return 1;
}

/* Plugin destructor. */
static void cpu_destructor(Plugin * p)
{
    CPUPlugin * c = (CPUPlugin *) p->priv;

    /* Disconnect the timer. */
    g_source_remove(c->timer);

    /* Deallocate memory. */
    g_object_unref(c->graphics_context);
    g_object_unref(c->bg_graphics_context);
    g_object_unref(c->pixmap);
    g_free(c->stats_cpu);
    g_free(c->fg_color);
    g_free(c->bg_color);
    g_free(c);
}


/* Callback when the configuration dialog is to be shown. */
static void cpu_configure(Plugin * p, GtkWindow * parent)
{
    CPUPlugin * c = (CPUPlugin *) p->priv;

    int update_interval_min = 50;
    int update_interval_max = 5000;

    GtkWidget * dlg = create_generic_config_dlg(
        _(p->class->name),
        GTK_WIDGET(parent),
        (GSourceFunc) cpu_apply_configuration, (gpointer) p,
        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
        _("Foreground color"), &c->fg_color, (GType)CONF_TYPE_COLOR,
        _("Background color"), &c->bg_color, (GType)CONF_TYPE_COLOR,
        _("Update interval" ), &c->update_interval, (GType)CONF_TYPE_INT,
        "int-min-value", (gpointer)&update_interval_min, (GType)CONF_TYPE_SET_PROPERTY,
        "int-max-value", (gpointer)&update_interval_max, (GType)CONF_TYPE_SET_PROPERTY,
        NULL);
    if (dlg)
        gtk_window_present(GTK_WINDOW(dlg));
}


/* Callback when the configuration is to be saved. */
static void cpu_save_configuration(Plugin * p, FILE * fp)
{
    CPUPlugin * c = (CPUPlugin *) p->priv;
    lxpanel_put_str(fp, "FgColor", c->fg_color);
    lxpanel_put_str(fp, "BgColor", c->bg_color);
    lxpanel_put_int(fp, "UpdateInterval", c->update_interval);
}


/* Callback when panel configuration changes. */
static void cpu_panel_configuration_changed(Plugin * p)
{
    cpu_apply_configuration(p);
}


/* Plugin descriptor. */
PluginClass cpu_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "cpu",
    name : N_("CPU Usage Monitor"),
    version: "1.0",
    description : N_("Display CPU usage"),

    constructor : cpu_constructor,
    destructor  : cpu_destructor,
    config : cpu_configure,
    save : cpu_save_configuration,
    panel_configuration_changed : cpu_panel_configuration_changed
};
