/***************************************************************************
 *
 * ObExit
 *
 * A logout dialog based on GTK+.
 *
 * It is based on exitx by Mihai Coman aka z0id (https://github.com/z0id).
 * The icons are based on the splendid FAENZA dark theme.
 *
 * Author  : Manfred Mueller <manfred DOT mueller AT fluxflux DOT net>
 * License : GPL, General Public License
 *           
 * Copyright (C) 2014 Manfred Mueller.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 * 
 ****************************************************************************/

#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <sys/wait.h>
#include <memory.h>
#include <signal.h>
#include <stdio.h>
#include <glib/gi18n.h>
#include <glib.h>
#include <locale.h>

#ifndef  PROJECT
# define PROJECT "obexit"
#endif
#ifndef  VERSION
# define VERSION "0.4"
#endif
#ifndef PREFIX
# define PREFIX "/usr"
#endif
#ifndef SYSCONFDIR
# define SYSCONFDIR "/etc"
#endif
#ifndef CONFIG_FILE
# define CONFIG_FILE SYSCONFDIR "/" PROJECT ".conf"
#endif
#ifndef DATADIR
# define DATADIR PREFIX "/share"
#endif
#ifndef ICONDIR
# define ICONDIR DATADIR "/" PROJECT
#endif
#ifndef LOCALEDIR
# define LOCALEDIR DATADIR "/locale"
#endif

#define	COLOR		"black"
#define	BORDER		10

#define	CANCEL		0
#define	LOGOUT		1
#define LOCK		2
#define STANDBY		3
#define HIBERNATE	4
#define REBOOT		5
#define HALT		6

#define GENERAL	"GENERAL"
#define IMAGES	"IMAGES"
#define COMMANDS "COMMANDS"

typedef struct _Fadeout Fadeout;
typedef struct _FoScreen FoScreen;

struct _FoScreen {
	GdkWindow *window;
	GdkPixmap *backbuf;
};

struct _Fadeout {
	GdkColor color;
	GList *screens;
};

static GtkWidget *dialog = NULL;

static void fadeout_drawable_mono(Fadeout * fadeout, GdkDrawable * drawable)
{
	cairo_t *cr;

	cr = gdk_cairo_create(drawable);
	gdk_cairo_set_source_color(cr, &fadeout->color);
	cairo_paint_with_alpha(cr, 0.9);
	cairo_destroy(cr);
}

Fadeout *fadeout_new(GdkDisplay * display)
{
	GdkWindowAttr attr;
	GdkGCValues values;
	Fadeout *fadeout;
	GdkWindow *root;
	GdkCursor *cursor;
	FoScreen *screen;
	GdkGC *gc;
	GList *lp;
	gint width;
	gint height;
	gint n;

	fadeout = g_new0(Fadeout, 1);
	gdk_color_parse(COLOR, &fadeout->color);

	cursor = gdk_cursor_new(GDK_WATCH);

	attr.x = 0;
	attr.y = 0;
	attr.event_mask = 0;
	attr.wclass = GDK_INPUT_OUTPUT;
	attr.window_type = GDK_WINDOW_TEMP;
	attr.cursor = cursor;
	attr.override_redirect = TRUE;

	for (n = 0; n < gdk_display_get_n_screens(display); ++n) {
		screen = g_new(FoScreen, 1);

		root = gdk_screen_get_root_window(gdk_display_get_screen(display, n));
		gdk_drawable_get_size(GDK_DRAWABLE(root), &width, &height);

		values.function = GDK_COPY;
		values.graphics_exposures = FALSE;
		values.subwindow_mode = TRUE;
		gc = gdk_gc_new_with_values(root, &values, GDK_GC_FUNCTION | GDK_GC_EXPOSURES | GDK_GC_SUBWINDOW);

		screen->backbuf = gdk_pixmap_new(GDK_DRAWABLE(root), width, height, -1);
		gdk_draw_drawable(GDK_DRAWABLE(screen->backbuf), gc, GDK_DRAWABLE(root), 0, 0, 0, 0, width, height);
		fadeout_drawable_mono(fadeout, GDK_DRAWABLE(screen->backbuf));

		attr.width = width;
		attr.height = height;

		screen->window = gdk_window_new(root, &attr, GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR | GDK_WA_CURSOR);
		gdk_window_set_back_pixmap(screen->window, screen->backbuf, FALSE);

		g_object_unref(G_OBJECT(gc));

		fadeout->screens = g_list_append(fadeout->screens, screen);
	}

	for (lp = fadeout->screens; lp != NULL; lp = lp->next)
		gdk_window_show(((FoScreen *) lp->data)->window);

	gdk_cursor_unref(cursor);

	return fadeout;
}

void fadeout_destroy(Fadeout * fadeout)
{
	FoScreen *screen;
	GList *lp;

	for (lp = fadeout->screens; lp != NULL; lp = lp->next) {
		screen = lp->data;

		gdk_window_destroy(screen->window);
		g_object_unref(G_OBJECT(screen->backbuf));
		g_free(screen);
	}

	g_list_free(fadeout->screens);
	g_free(fadeout);
}
static void cancel_button_clicked(GtkWidget * b, gint * shutdownType)
{
	*shutdownType = CANCEL;
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
}

static void logout_button_clicked(GtkWidget * b, gint * shutdownType)
{
	*shutdownType = LOGOUT;
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
}

static void lock_button_clicked(GtkWidget * b, gint * shutdownType)
{
	*shutdownType = LOCK;
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
}

static void reboot_button_clicked(GtkWidget * b, gint * shutdownType)
{
	*shutdownType = REBOOT;
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
}

static void standby_button_clicked(GtkWidget * b, gint * shutdownType)
{
	*shutdownType = STANDBY;
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
}

static void hibernate_button_clicked(GtkWidget * b, gint * shutdownType)
{
	*shutdownType = HIBERNATE;
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
}

static void poweroff_button_clicked(GtkWidget * b, gint * shutdownType)
{
	*shutdownType = HALT;
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
}

void gtk_window_center_on_monitor(GtkWindow * window, GdkScreen * screen, gint monitor)
{
	GtkRequisition requisition;
	GdkRectangle geometry;
	GdkScreen *widget_screen;
	gint x, y;

	gdk_screen_get_monitor_geometry(screen, monitor, &geometry);

	/* 
	 * Getting a size request requires the widget
	 * to be associated with a screen, because font
	 * information may be needed (Olivier).
	 */
	widget_screen = gtk_widget_get_screen(GTK_WIDGET(window));
	if (screen != widget_screen) {
		gtk_window_set_screen(GTK_WINDOW(window), screen);
	}
	/*
	 * We need to be realized, otherwise we may get 
	 * some odd side effects (Olivier). 
	 */
	if (!GTK_WIDGET_REALIZED(GTK_WIDGET(window))) {
		gtk_widget_realize(GTK_WIDGET(window));
	}
	/*
	 * Yes, I know -1 is useless here (Olivier).
	 */
	requisition.width = requisition.height = -1;
	gtk_widget_size_request(GTK_WIDGET(window), &requisition);

	x = geometry.x + (geometry.width - requisition.width) / 2;
	y = geometry.y + (geometry.height - requisition.height) / 2;

	gtk_window_move(window, x, y);
}

void window_add_border(GtkWindow * window)
{
	GtkWidget *box1, *box2;

	gtk_widget_realize(GTK_WIDGET(window));

	box1 = gtk_event_box_new();
	gtk_widget_modify_bg(box1, GTK_STATE_NORMAL, &(GTK_WIDGET(window)->style->bg[GTK_STATE_SELECTED]));
	gtk_widget_show(box1);

	box2 = gtk_event_box_new();
	gtk_widget_show(box2);
	gtk_container_add(GTK_CONTAINER(box1), box2);

	gtk_container_set_border_width(GTK_CONTAINER(box2), 10);
	gtk_widget_reparent(GTK_BIN(window)->child, box2);

	gtk_container_add(GTK_CONTAINER(window), box1);
}

void window_grab_input(GtkWindow * window)
{
	GdkWindow *xwindow = GTK_WIDGET(window)->window;

	gdk_pointer_grab(xwindow, TRUE, 0, NULL, NULL, GDK_CURRENT_TIME);
	gdk_keyboard_grab(xwindow, FALSE, GDK_CURRENT_TIME);
	XSetInputFocus(GDK_DISPLAY(), GDK_WINDOW_XWINDOW(xwindow), RevertToParent, CurrentTime);
}

GdkPixbuf *themed_icon_load(const gchar * name, gint size)
{
	g_return_val_if_fail(name, NULL);

	return gdk_pixbuf_new_from_file_at_size(name, size, size, NULL);
}

static gboolean screen_contains_pointer(GdkScreen * screen, int *x, int *y)
{
	GdkWindow *root_window;
	Window root, child;
	Bool retval;
	int rootx, rooty;
	int winx, winy;
	unsigned int xmask;

	root_window = gdk_screen_get_root_window(screen);

	retval =
	    XQueryPointer(GDK_SCREEN_XDISPLAY(screen), GDK_DRAWABLE_XID(root_window), &root, &child, &rootx, &rooty,
			  &winx, &winy, &xmask);

	if (x)
		*x = retval ? rootx : -1;
	if (y)
		*y = retval ? rooty : -1;

	return retval;
}

GdkScreen *gdk_display_locate_monitor_with_pointer(GdkDisplay * display, gint * monitor_return)
{
	int n_screens, i;

	if (display == NULL)
		display = gdk_display_get_default();

	n_screens = gdk_display_get_n_screens(display);
	for (i = 0; i < n_screens; i++) {
		GdkScreen *screen;
		int x, y;

		screen = gdk_display_get_screen(display, i);

		if (screen_contains_pointer(screen, &x, &y)) {
			if (monitor_return)
				*monitor_return = gdk_screen_get_monitor_at_point(screen, x, y);

			return screen;
		}
	}

	if (monitor_return)
		*monitor_return = 0;

	return NULL;
}


gboolean main(int argc, char **argv)
{
	Fadeout *fadeout;
	GdkScreen *screen;
	GtkWidget *dbox;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *vbox3;
	GtkWidget *vbox4;
	GtkWidget *image;
	GtkWidget *hidden;
	GtkWidget *label;
	GtkWidget *lock_button;
	GtkWidget *logout_button;
	GtkWidget *reboot_button;
	GtkWidget *poweroff_button;
	GtkWidget *standby_button;
	GtkWidget *hibernate_button;
	GtkWidget *cancel_button;
	GdkPixbuf *icon;
	GdkPixbuf *logo;
	gchar *user;
	gchar *userstr;
	gchar *versionstr;
	gchar *logoimg;
	gint imagesize;
	gchar *logoutimg;
	gchar *logoutcmd;
	gchar *lockimg;
	gchar *lockcmd;
	gchar *rebootimg;
	gchar *rebootcmd;
	gchar *poweroffimg;
	gchar *poweroffcmd;
	gchar *standbyimg;
	gchar *standbycmd;
	gchar *hibernateimg;
	gchar *hibernatecmd;
	gchar *cancelimg;
	gint monitor;
	gint result;
	gint shutdownType;
	GKeyFile* gkf;
	GError* error = NULL;
	gboolean test;

	gkf = g_key_file_new();

	test = g_key_file_load_from_file(gkf, CONFIG_FILE, G_KEY_FILE_NONE, &error);
	if(test == FALSE)
	{
		if(error != NULL)
		{
			g_key_file_free(gkf);
			fprintf (stderr, CONFIG_FILE " not found or not readable!\n");
			g_error_free(error);
			return -1;
		}
	}
	imagesize = g_key_file_get_integer(gkf, GENERAL, "Imagesize", NULL);
	logoimg = g_key_file_get_string(gkf, IMAGES, "Logoimg", NULL);
	lockimg = g_key_file_get_string(gkf, IMAGES, "Lockimg", NULL);
	lockcmd = g_key_file_get_string(gkf, COMMANDS, "Lockcmd", NULL);
	logoutimg = g_key_file_get_string(gkf, IMAGES, "Logoutimg", NULL);
	logoutcmd = g_key_file_get_string(gkf, COMMANDS, "Logoutcmd", NULL);
	rebootimg = g_key_file_get_string(gkf, IMAGES, "Rebootimg", NULL);
	rebootcmd = g_key_file_get_string(gkf, COMMANDS, "Rebootcmd", NULL);
	poweroffimg = g_key_file_get_string(gkf, IMAGES, "Poweroffimg", NULL);
	poweroffcmd = g_key_file_get_string(gkf, COMMANDS, "Poweroffcmd", NULL);
	standbyimg = g_key_file_get_string(gkf, IMAGES, "Standbyimg", NULL);
	standbycmd = g_key_file_get_string(gkf, COMMANDS, "Standbycmd", NULL);
	hibernateimg = g_key_file_get_string(gkf, IMAGES, "Hibernateimg", NULL);
	hibernatecmd = g_key_file_get_string(gkf, COMMANDS, "Hibernatecmd", NULL);
	cancelimg = g_key_file_get_string(gkf, IMAGES, "Cancelimg", NULL);
	gtk_init(&argc, &argv);

	bindtextdomain(PROJECT, LOCALEDIR);
	textdomain(PROJECT);
	/* get screen with pointer */
	screen = gdk_display_locate_monitor_with_pointer(NULL, &monitor);
	if (screen == NULL) {
		screen = gdk_screen_get_default();
		monitor = 0;
	}

	/* Try to grab Input on a hidden window first */
	hidden = gtk_invisible_new_for_screen(screen);
	gtk_widget_show_now(hidden);

	for (;;) {
		if (gdk_pointer_grab(hidden->window, TRUE, 0, NULL, NULL, GDK_CURRENT_TIME) == GDK_GRAB_SUCCESS) {
			if (gdk_keyboard_grab(hidden->window, FALSE, GDK_CURRENT_TIME)
			    == GDK_GRAB_SUCCESS) {
				break;
			}

			gdk_pointer_ungrab(GDK_CURRENT_TIME);
		}

		g_usleep(50 * 1000);
	}

	/* display fadeout */
	fadeout = fadeout_new(gtk_widget_get_display(hidden));
	gdk_flush();

	dialog = g_object_new(GTK_TYPE_DIALOG, "type", GTK_WINDOW_POPUP, NULL);
	dialog = dialog;

	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_screen(GTK_WINDOW(dialog), screen);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);

	dbox = GTK_DIALOG(dialog)->vbox;

	/* - -------------------------------- - vbox */
	vbox = gtk_vbox_new(FALSE, BORDER);
	gtk_box_pack_start(GTK_BOX(dbox), vbox, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER);
	gtk_widget_show(vbox);

	/* - -------------------------------- - 2nd vbox */
	vbox2 = gtk_vbox_new(TRUE, BORDER);
	gtk_widget_show(vbox2);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, FALSE, 0);

	/* - -------------------------------- - */
	/* logo */
	logo = gdk_pixbuf_new_from_file_at_size(logoimg, 80, 80, NULL);
	image = gtk_image_new_from_pixbuf(logo);
	gtk_widget_show(image);
	gtk_box_pack_start(GTK_BOX(vbox2), image, TRUE, TRUE, 0);

	/* label */
	user = getenv ("USER");
	userstr = g_strconcat (_("<b><big>Leave System (Logged in: "), user, ")</big></b>", NULL);
	label = gtk_label_new(userstr);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(vbox2), label, TRUE, TRUE, 0);

	/* - -------------------------------- - 2nd hbox */
	hbox = gtk_hbox_new(TRUE, BORDER);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* - -------------------------------- - */
	/* lock */
	lock_button = gtk_button_new();
	if (lockcmd != NULL)
	gtk_widget_show(lock_button);
	gtk_box_pack_start(GTK_BOX(hbox), lock_button, TRUE, TRUE, 0);

	g_signal_connect(lock_button, "clicked", G_CALLBACK(lock_button_clicked), &shutdownType);

	vbox3 = gtk_vbox_new(FALSE, BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(vbox3), BORDER);
	gtk_widget_show(vbox3);
	gtk_container_add(GTK_CONTAINER(lock_button), vbox3);

	icon = gdk_pixbuf_new_from_file_at_size(lockimg, imagesize, imagesize, NULL);
	image = gtk_image_new_from_pixbuf(icon);
	gtk_widget_show(image);
	gtk_box_pack_start(GTK_BOX(vbox3), image, FALSE, FALSE, 0);
	g_object_unref(icon);

	label = gtk_label_new(_("Lock"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(vbox3), label, FALSE, FALSE, 0);

	/* logout */
	logout_button = gtk_button_new();
	if (logoutcmd != NULL)
	gtk_widget_show(logout_button);
	gtk_box_pack_start(GTK_BOX(hbox), logout_button, TRUE, TRUE, 0);

	g_signal_connect(logout_button, "clicked", G_CALLBACK(logout_button_clicked), &shutdownType);

	vbox3 = gtk_vbox_new(FALSE, BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(vbox3), BORDER);
	gtk_widget_show(vbox3);
	gtk_container_add(GTK_CONTAINER(logout_button), vbox3);

	icon = gdk_pixbuf_new_from_file_at_size(logoutimg, imagesize, imagesize, NULL);
	image = gtk_image_new_from_pixbuf(icon);
	gtk_widget_show(image);
	gtk_box_pack_start(GTK_BOX(vbox3), image, FALSE, FALSE, 0);
	g_object_unref(icon);

	label = gtk_label_new(_("Logout"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(vbox3), label, FALSE, FALSE, 0);

	/* reboot */
	reboot_button = gtk_button_new();
	if (rebootcmd != NULL)
	gtk_widget_show(reboot_button);
	gtk_box_pack_start(GTK_BOX(hbox), reboot_button, TRUE, TRUE, 0);

	g_signal_connect(reboot_button, "clicked", G_CALLBACK(reboot_button_clicked), &shutdownType);

	vbox3 = gtk_vbox_new(FALSE, BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(vbox3), BORDER);
	gtk_widget_show(vbox3);
	gtk_container_add(GTK_CONTAINER(reboot_button), vbox3);

	icon = gdk_pixbuf_new_from_file_at_size(rebootimg, imagesize, imagesize, NULL);
	image = gtk_image_new_from_pixbuf(icon);
	gtk_widget_show(image);
	gtk_box_pack_start(GTK_BOX(vbox3), image, FALSE, FALSE, 0);
	g_object_unref(icon);

	label = gtk_label_new(_("Reboot"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(vbox3), label, FALSE, FALSE, 0);

	/* standby */
	standby_button = gtk_button_new();
	if (standbycmd != NULL)
	gtk_widget_show(standby_button);
	gtk_box_pack_start(GTK_BOX(hbox), standby_button, TRUE, TRUE, 0);

	g_signal_connect(standby_button, "clicked", G_CALLBACK(standby_button_clicked), &shutdownType);

	vbox3 = gtk_vbox_new(FALSE, BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(vbox3), BORDER);
	gtk_widget_show(vbox3);
	gtk_container_add(GTK_CONTAINER(standby_button), vbox3);

	icon = gdk_pixbuf_new_from_file_at_size(standbyimg, imagesize, imagesize, NULL);
	image = gtk_image_new_from_pixbuf(icon);
	gtk_widget_show(image);
	gtk_box_pack_start(GTK_BOX(vbox3), image, FALSE, FALSE, 0);
	g_object_unref(icon);

	label = gtk_label_new(_("Standby"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(vbox3), label, FALSE, FALSE, 0);

	/* hibernate */
	hibernate_button = gtk_button_new();
	if (hibernatecmd != NULL)
	gtk_widget_show(hibernate_button);
	gtk_box_pack_start(GTK_BOX(hbox), hibernate_button, TRUE, TRUE, 0);

	g_signal_connect(hibernate_button, "clicked", G_CALLBACK(hibernate_button_clicked), &shutdownType);

	vbox3 = gtk_vbox_new(FALSE, BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(vbox3), BORDER);
	gtk_widget_show(vbox3);
	gtk_container_add(GTK_CONTAINER(hibernate_button), vbox3);

	icon = gdk_pixbuf_new_from_file_at_size(hibernateimg, imagesize, imagesize, NULL);
	image = gtk_image_new_from_pixbuf(icon);
	gtk_widget_show(image);
	gtk_box_pack_start(GTK_BOX(vbox3), image, FALSE, FALSE, 0);
	g_object_unref(icon);

	label = gtk_label_new(_("Hibernate"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(vbox3), label, FALSE, FALSE, 0);

	/* halt */
	poweroff_button = gtk_button_new();
	if (poweroffcmd != NULL)
	gtk_widget_show(poweroff_button);
	gtk_box_pack_start(GTK_BOX(hbox), poweroff_button, TRUE, TRUE, 0);

	g_signal_connect(poweroff_button, "clicked", G_CALLBACK(poweroff_button_clicked), &shutdownType);

	vbox3 = gtk_vbox_new(FALSE, BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(vbox3), BORDER);
	gtk_widget_show(vbox3);
	gtk_container_add(GTK_CONTAINER(poweroff_button), vbox3);

	icon = gdk_pixbuf_new_from_file_at_size(poweroffimg, imagesize, imagesize, NULL);
	image = gtk_image_new_from_pixbuf(icon);
	gtk_widget_show(image);
	gtk_box_pack_start(GTK_BOX(vbox3), image, FALSE, FALSE, 0);
	g_object_unref(icon);

	label = gtk_label_new(_("Poweroff"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(vbox3), label, FALSE, FALSE, 0);

	/* cancel */
	cancel_button = gtk_button_new();
	gtk_widget_show(cancel_button);
	gtk_box_pack_start(GTK_BOX(hbox), cancel_button, TRUE, TRUE, 0);

	g_signal_connect(cancel_button, "clicked", G_CALLBACK(cancel_button_clicked), &shutdownType);

	vbox3 = gtk_vbox_new(FALSE, BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(vbox3), BORDER);
	gtk_widget_show(vbox3);
	gtk_container_add(GTK_CONTAINER(cancel_button), vbox3);

	icon = gdk_pixbuf_new_from_file_at_size(cancelimg, imagesize, imagesize, NULL);
	image = gtk_image_new_from_pixbuf(icon);
	gtk_widget_show(image);
	gtk_box_pack_start(GTK_BOX(vbox3), image, FALSE, FALSE, 0);
	g_object_unref(icon);

	label = gtk_label_new(_("Cancel"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(vbox3), label, FALSE, FALSE, 0);

	/* - -------------------------------- - 3rd hbox */
	hbox = gtk_hbox_new(TRUE, BORDER);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	vbox4 = gtk_vbox_new(TRUE, BORDER);
	gtk_widget_show(vbox4);
	gtk_box_pack_start(GTK_BOX(hbox), vbox4, TRUE, TRUE, 0);

	vbox4 = gtk_vbox_new(TRUE, BORDER);
	gtk_widget_show(vbox4);
	gtk_box_pack_start(GTK_BOX(hbox), vbox4, TRUE, TRUE, 0);

	vbox4 = gtk_vbox_new(TRUE, BORDER);
	gtk_widget_show(vbox4);
	gtk_box_pack_start(GTK_BOX(hbox), vbox4, TRUE, TRUE, 0);

	/* label */

	versionstr = g_strconcat ("<span foreground='#dddddd'><small>", PROJECT, " ", VERSION " </small></span>", NULL);
	label = gtk_label_new(versionstr);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment ( (GtkMisc *)label , 1, 1 ) ;
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(vbox4), label, TRUE, TRUE, 0);


	/* create small border */
	window_add_border(GTK_WINDOW(dialog));

	/* center dialog on target monitor */
	gtk_window_center_on_monitor(GTK_WINDOW(dialog), screen, monitor);

	gtk_widget_grab_focus(GTK_WIDGET(cancel_button));
	/* connect to the shutdown helper */
	/* save portion of the root window covered by the dialog */
	/* need to realize the dialog first! */
	gtk_widget_show_now(dialog);

	/* Grab Keyboard and Mouse pointer */
	window_grab_input(GTK_WINDOW(dialog));

	/* run the logout dialog */
	result = gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_hide(dialog);

	gtk_widget_destroy(dialog);
	gtk_widget_destroy(hidden);

	dialog = NULL;
	/* Release Keyboard/Mouse pointer grab */
	fadeout_destroy(fadeout);

	gdk_pointer_ungrab(GDK_CURRENT_TIME);
	gdk_keyboard_ungrab(GDK_CURRENT_TIME);
	gdk_flush();

	/* process all pending events first */
	while (gtk_events_pending())
		g_main_context_iteration(NULL, FALSE);

	switch (shutdownType) {
	case CANCEL:
		break;
	case LOGOUT:
		system(logoutcmd);
		break;
	case LOCK:
		system(lockcmd);
		break;
	case REBOOT:
		system(rebootcmd);
		break;
	case STANDBY:
		system(standbycmd);
		break;
	case HIBERNATE:
		system(hibernatecmd);
		break;
	case HALT:
		system(poweroffcmd);
		break;
	default:
		break;
	}
	return (result == GTK_RESPONSE_OK);
}
