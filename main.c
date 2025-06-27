#include <gtk/gtk.h>
#include "dmi-api.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    dmi_display *ddc;
    int i2c_busno;
} DisplayWrapper;

typedef struct display_section {
	GtkWidget *label;
	GtkWidget *brightness_label;
	GtkWidget *brightness_scale;
	GtkWidget *contrast_label;
	GtkWidget *contrast_scale;
	GtkWidget *input_combo;
	GtkWidget *frame;
} display_section;

const int MARGIN_UNIT = 8;

int find_i2c_bus_by_edid_hint(const char *hint) {
    FILE *fp = popen("ddcutil detect", "r");
    if (!fp) return -1;

    char line[512];
    int current_bus = -1;

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "I2C bus:")) {
            sscanf(line, "I2C bus: /dev/i2c-%d", &current_bus);
        } else if (strstr(line, hint)) {
            pclose(fp);
            return current_bus;
        }
    }

    pclose(fp);
    return -1;
}

gboolean
set_input(GtkComboBoxText *combo, gpointer data)
{
    DisplayWrapper *dw = data;
    const gchar *active_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo));
    if (!active_id) return FALSE;

    guint8 input_val = (guint8) atoi(active_id);

    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "ddcutil --bus=%d setvcp 0x60 %d >/dev/null 2>&1",
             dw->i2c_busno, input_val);

    int rc = system(cmd);
    if (rc != 0)
        g_printerr("Failed to set input via ddcutil on bus %d\n", dw->i2c_busno);

    return FALSE;
}

gboolean
set_brightness(GtkWidget *widget, GdkEvent *event, gpointer data) 
{
	dmi_display *disp = data;
	guint16 new_val = gtk_range_get_value(GTK_RANGE(widget));         
	int rc = dmi_display_set_brightness(disp, new_val);
	if (rc == 1)
		g_printerr("Partial success in setting the brightness of display no %d to %u. Code: %d\n", 
			disp->info.dispno, new_val, rc);	
	else if (rc != 0)
		g_printerr("An error occurred when setting the brightness of display no %d to %u. Code: %d\n", 
			disp->info.dispno, new_val, rc);	
	return FALSE;
}

gboolean
set_contrast(GtkWidget *widget, GdkEvent *event, gpointer data) 
{
	dmi_display *disp = data;
	guint16 new_val = gtk_range_get_value(GTK_RANGE(widget));         
	int rc = dmi_display_set_contrast(disp, new_val);
	if (rc == 1)
		g_printerr("Partial success in setting the contrast of display no %d to %u. Code: %d\n", 
			disp->info.dispno, new_val, rc);	
	else if (rc != 0)
		g_printerr("An error occurred when setting the contrast of display no %d to %u. Code: %d\n", 
			disp->info.dispno, new_val, rc);	
	return FALSE;
}

display_section* display_section_init(dmi_display *disp) 
{
	display_section *ds = malloc(sizeof(display_section));

	GtkWidget *frame = gtk_frame_new(NULL);
	gtk_style_context_add_class(gtk_widget_get_style_context(frame), "monitor-frame");
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_widget_set_margin_bottom(frame, 20);
	gtk_widget_set_margin_top(frame, 10);
	gtk_widget_set_margin_start(frame, 0);
	gtk_widget_set_margin_end(frame, 0);

	GtkWidget *section_grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(section_grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(section_grid), 6);
	gtk_container_add(GTK_CONTAINER(frame), section_grid);

	GtkWidget *top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

	GtkWidget *icon = gtk_image_new_from_icon_name("video-display", GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_top(icon, 4);
	gtk_box_pack_start(GTK_BOX(top_row), icon, FALSE, FALSE, 0);

	GtkWidget *label = gtk_label_new(disp->info.model_name);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
	gtk_box_pack_start(GTK_BOX(top_row), label, FALSE, FALSE, 0);

	gtk_widget_set_margin_start(top_row, MARGIN_UNIT);
	gtk_widget_set_margin_top(top_row, 1.75 * MARGIN_UNIT);
	gtk_widget_set_margin_bottom(top_row, 20);

	ds->brightness_label = gtk_label_new("Brightness");
	gtk_label_set_xalign(GTK_LABEL(ds->brightness_label), 0.0);
	gtk_widget_set_hexpand(ds->brightness_label, TRUE);
	gtk_widget_set_halign(ds->brightness_label, GTK_ALIGN_START);
	gtk_widget_set_margin_start(ds->brightness_label, MARGIN_UNIT);
	gtk_widget_set_valign(ds->brightness_label, GTK_ALIGN_CENTER);

	ds->brightness_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, disp->brightness_max, 1);
	gtk_range_set_value(GTK_RANGE(ds->brightness_scale), disp->brightness_val);
	gtk_scale_set_value_pos(GTK_SCALE(ds->brightness_scale), GTK_POS_RIGHT);
	gtk_scale_set_digits(GTK_SCALE(ds->brightness_scale), 0);
	gtk_widget_set_hexpand(ds->brightness_scale, TRUE);
	g_signal_connect(ds->brightness_scale, "button-release-event", G_CALLBACK(set_brightness), disp);


	ds->contrast_label = gtk_label_new("Contrast");
	gtk_label_set_xalign(GTK_LABEL(ds->contrast_label), 0.0);
	gtk_widget_set_hexpand(ds->contrast_label, TRUE);
	gtk_widget_set_halign(ds->contrast_label, GTK_ALIGN_START);
	gtk_widget_set_margin_start(ds->contrast_label, MARGIN_UNIT);
	gtk_widget_set_valign(ds->contrast_label, GTK_ALIGN_CENTER);

	ds->contrast_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, disp->contrast_max, 1);
	gtk_range_set_value(GTK_RANGE(ds->contrast_scale), disp->contrast_val);
	gtk_scale_set_value_pos(GTK_SCALE(ds->contrast_scale), GTK_POS_RIGHT);
	gtk_scale_set_digits(GTK_SCALE(ds->contrast_scale), 0);
	gtk_widget_set_hexpand(ds->contrast_scale, TRUE);
	g_signal_connect(ds->contrast_scale, "button-release-event", G_CALLBACK(set_contrast), disp);


	if (disp->contrast_max == 0) {
		gtk_widget_set_sensitive(ds->contrast_scale, FALSE);
		gtk_widget_set_sensitive(ds->contrast_label, FALSE);
	}


	struct {
		const char *name;
		int code;
	} input_sources[] = {
		{ "VGA-1", 0x01 },
		{ "DVI-1", 0x03 },
		{ "HDMI-1", 0x11 },
		{ "HDMI-2", 0x12 },
		{ "DisplayPort-1", 0x0f },
		{ "DisplayPort-2", 0x10 },
	};


	GtkWidget *input_label = gtk_label_new("Input Source");
	gtk_label_set_xalign(GTK_LABEL(input_label), 0.0);
	gtk_widget_set_hexpand(input_label, TRUE);
	gtk_widget_set_halign(input_label, GTK_ALIGN_START);
	gtk_widget_set_margin_start(input_label, MARGIN_UNIT);
	gtk_widget_set_valign(input_label, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_start(input_label, MARGIN_UNIT);
	gtk_widget_set_margin_bottom(input_label, 10); 

	GtkWidget *combo = gtk_combo_box_text_new();
	for (unsigned i = 0; i < sizeof(input_sources)/sizeof(input_sources[0]); i++) {
		char val_str[8];
		snprintf(val_str, sizeof(val_str), "%d", input_sources[i].code);
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), val_str, input_sources[i].name);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
	gtk_widget_set_margin_start(combo, MARGIN_UNIT);
	gtk_widget_set_margin_bottom(combo, MARGIN_UNIT);
	ds->input_combo = combo;

	DisplayWrapper *dw = malloc(sizeof(DisplayWrapper));
	dw->ddc = disp;
	dw->i2c_busno = find_i2c_bus_by_edid_hint(disp->info.model_name);
	if (dw->i2c_busno == -1) {
	    g_warning("Could not find I2C bus for display '%s'; using fallback 3", disp->info.model_name);
	    dw->i2c_busno = 3;
	}
	g_signal_connect(combo, "changed", G_CALLBACK(set_input), dw);

	int row = 0;
	gtk_grid_attach(GTK_GRID(section_grid), top_row, 0, row++, 1, 1);
	gtk_grid_attach(GTK_GRID(section_grid), ds->brightness_label, 0, row++, 1, 1);
	gtk_grid_attach(GTK_GRID(section_grid), ds->brightness_scale, 0, row++, 1, 1);
	gtk_grid_attach(GTK_GRID(section_grid), ds->contrast_label, 0, row++, 1, 1);
	gtk_grid_attach(GTK_GRID(section_grid), ds->contrast_scale, 0, row++, 1, 1);
	gtk_grid_attach(GTK_GRID(section_grid), input_label, 0, row++, 1, 1);
	gtk_grid_attach(GTK_GRID(section_grid), ds->input_combo, 0, row++, 1, 1);

ds->frame = frame;
return ds;
}


static void
display_section_attach_next_to(display_section *ds, GtkGrid *grid, display_section *sibling)
{
    static int current_row = 0;

    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_widget_set_hexpand(ds->frame, TRUE);
    gtk_grid_attach(GTK_GRID(grid), ds->frame, 0, current_row++, 2, 1);
}


static void
activate(GtkApplication *app, gpointer data)
{
    GtkWidget *window;
    GtkWidget *grid;

    dmi_display_list *dlist = data;

    window = gtk_application_window_new(app);

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, "style.css", NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);

    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);

    GtkWidget *title_label = gtk_label_new("DDC Monitor & Input Utils");
    gtk_widget_set_halign(title_label, GTK_ALIGN_START);
    gtk_widget_set_valign(title_label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(title_label, 10);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), title_label);

    gtk_window_set_titlebar(GTK_WINDOW(window), header);
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 0);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *outer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_top(outer_box, 20);
    gtk_widget_set_margin_bottom(outer_box, 20);
    gtk_widget_set_margin_start(outer_box, 20);
    gtk_widget_set_margin_end(outer_box, 20);

    grid = gtk_grid_new();
    gtk_box_pack_start(GTK_BOX(outer_box), grid, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(window), outer_box);

    display_section **sections = malloc(dlist->ct * sizeof(display_section *));
    display_section *sibling = NULL;
    for (guint it = 0; it < dlist->ct; it++) {
        dmi_display *disp = dmi_display_list_get(dlist, it);
        sections[it] = display_section_init(disp);
        display_section_attach_next_to(sections[it], GTK_GRID(grid), sibling);
        sibling = sections[it];
    }

    gtk_widget_show_all(window);
}


int
main(int argc, char **argv)
{
	GtkApplication *app;
	int status;

	dmi_display_list dlist = dmi_display_list_init(FALSE);

	if (dlist.ct <= 0) {
		g_printerr("No supported displays found. Please check if ddcutil is properly installed and/or whether you have any supported monitors.\n");
		return 1;
	}

	app = gtk_application_new("org.gtk.dmi-gtk", G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(app, "activate", G_CALLBACK(activate), &dlist);
	status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);

	dmi_display_list_free(&dlist);
	return status;
}
