#include <gtk/gtk.h>
#include <stdlib.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "panel.h"
#include "misc.h"
#include "plugin.h"

#include "dbg.h"

#include "netstatus-icon.h"
#include "netstatus-dialog.h"

typedef struct {
    char *iface;
    char *config_tool;
    GtkWidget *mainw;
    GtkWidget *dlg;
} netstatus;


static void
netstatus_destructor(plugin *p)
{
    netstatus *ns = (netstatus *)p->priv;

    ENTER;
    gtk_widget_destroy(ns->mainw);
    g_free( ns->iface );
    g_free( ns->config_tool );
    g_free(ns);
    RET();
}

static void on_response( GtkDialog* dlg, gint response, netstatus *ns )
{
    switch( response )
    {
        case GTK_RESPONSE_CLOSE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_NONE:
            gtk_widget_destroy( dlg );
            ns->dlg = NULL;
    }
}

static void on_button_press( GtkWidget* widget, GdkEventButton* evt, plugin* p )
{
    NetstatusIface* iface;
    netstatus *ns = (netstatus*)p->priv;

    if( evt->button == 1 ) /*  Left click*/
    {
        if( ! ns->dlg )
        {
            iface = netstatus_icon_get_iface( NETSTATUS_ICON(widget) );
            ns->dlg = netstatus_dialog_new(iface);
            netstatus_dialog_set_configuration_tool( ns->dlg, ns->config_tool );
            g_signal_connect( ns->dlg, "response", on_response, ns );
        }
        gtk_window_present( GTK_WINDOW(ns->dlg) );
    }
}

static int
netstatus_constructor(plugin *p)
{
    netstatus *ns;
    line s;
    int w, h;
    NetstatusIface* iface;
    GtkWidget* icon;

    ENTER;
    s.len = 256;  
    ns = g_new0(netstatus, 1);
    g_return_val_if_fail(ns != NULL, 0);
    p->priv = ns;
    while (get_line(p->fp, &s) != LINE_BLOCK_END) {
        if (s.type == LINE_NONE) {
            ERR( "netstatus: illegal token %s\n", s.str);
            goto error;
        }
        if (s.type == LINE_VAR) {
            if (!g_ascii_strcasecmp(s.t[0], "iface"))
                ns->iface = g_strdup(s.t[1]);
            else if (!g_ascii_strcasecmp(s.t[0], "config_tool"))
                ns->config_tool = g_strdup(s.t[1]);
            else {
                ERR( "netstatus: unknown var %s\n", s.t[0]);
                goto error;
            }
        } else {
            ERR( "netstatus: illegal in this context %s\n", s.str);
            goto error;
        }
    }

    iface = netstatus_iface_new(ns->iface);
    ns->mainw = netstatus_icon_new( iface );
    gtk_widget_add_events( ns->mainw, GDK_BUTTON_PRESS_MASK );
    g_object_unref( iface );
    g_signal_connect( ns->mainw, "button-press-event",
                      G_CALLBACK(on_button_press), p );
    gtk_widget_set_size_request( ns->mainw, 24, 24 );

    gtk_widget_show_all(ns->mainw);

    gtk_container_add(GTK_CONTAINER(p->pwid), ns->mainw);

    RET(1);

 error:
    netstatus_destructor(p);
    RET(0);
}


plugin_class netstatus_plugin_class = {
    fname: NULL,
    count: 0,

    type : "netstatus",
    name : "netstatus",
    version: "1.0",
    description : "Net status",

    constructor : netstatus_constructor,
    destructor  : netstatus_destructor,
};
