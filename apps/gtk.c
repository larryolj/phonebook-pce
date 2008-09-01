/*
 *
 *  Bluetooth PhoneBook Client Access
 *
 *  Copyright (C) 2008  Larry Junior <larry.olj@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>

#define W 500
#define H 500

GtkListStore	*model;

void quit(GtkWidget *w, gpointer p)
{
	gtk_main_quit();
}

GtkTreeViewColumn* init_column(gchar *titulo)
{
	GtkTreeViewColumn *col;
	GtkCellRenderer *renderer;

	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, titulo);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_set_attributes(col, renderer, "text", 0, NULL);

	return col;
}

static GtkWidget *create_panel(void)
{
	GtkWidget *panel, *list, *scroll_list;

	panel = gtk_hpaned_new();
	list = gtk_tree_view_new();

	gtk_tree_view_append_column(GTK_TREE_VIEW (list),
			GTK_TREE_VIEW_COLUMN (init_column("name")));

	gtk_tree_view_append_column(GTK_TREE_VIEW (list),
			GTK_TREE_VIEW_COLUMN (init_column("handle")));

	model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW (list), GTK_TREE_MODEL (model));

	scroll_list = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER (scroll_list), GTK_WIDGET (list));
	g_object_unref(model);

	gtk_paned_add1(GTK_PANED (panel), GTK_WIDGET (scroll_list));


	return panel;
}

void add_list_item( const char *name, const char *handler)
{
	GtkTreeIter iter;

	gtk_tree_store_append(GTK_TREE_STORE (model), &iter, NULL);
	gtk_tree_store_set(GTK_TREE_STORE (model), &iter, 0, name,
						1, handler, -1);
}

void show_vcard(const char *vcard, unsigned int len)
{

}

int init(int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *panel;

	gtk_init(&argc, &argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW (window), "Obex Phonebook Client");

	gtk_signal_connect(GTK_OBJECT (window), "destroy",
					GTK_SIGNAL_FUNC (quit), NULL);
	gtk_window_set_default_size(GTK_WINDOW (window), W, H);

	panel = create_panel();
	gtk_container_add(GTK_CONTAINER (window), panel);

	gtk_widget_show_all(window);

	gtk_main();

	return  0;
}
