#include "dmi-api.h"

#include <gtk/gtk.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW_WIDTH 550
#define AUTO_CLOSE_DELAY_SEC 4
#define DEBUG_MODE 0

#if DEBUG_MODE
#define DEBUG_PRINT(fmt, ...) g_print("[DMI-GTK] " fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...) ((void)0)
#endif

typedef struct {
    guint8 code;
    const char *name;
} ColorTempPreset;

static const ColorTempPreset color_temp_presets[] = {
    {0x05, "6500K (sRGB)"}, {0x08, "9300K (Cool)"}, {0x0b, "User 1"}, {0x0c, "User 2"}};

static const size_t color_temp_presets_count =
    sizeof(color_temp_presets) / sizeof(color_temp_presets[0]);

typedef struct {
    dmi_display *ddc;
    int i2c_busno;
} DisplayWrapper;

typedef struct {
    GtkWidget *frame;
    GtkWidget *label;
    GtkWidget *brightness_label;
    GtkWidget *brightness_scale;
    GtkWidget *contrast_label;
    GtkWidget *contrast_scale;
    GtkWidget *ctemp_label;
    GtkWidget *ctemp_combo;
    GtkWidget *volume_label;
    GtkWidget *volume_scale;
    GtkWidget *input_combo;
    DisplayWrapper *wrapper;
    GArray *supported_inputs;
    GtkNotebook *notebook;
    guint display_number;
} DisplaySection;

static gboolean mouse_inside = FALSE;
static gint64 mouse_leave_time = 0;
static guint close_timeout_id = 0;

static GtkWidget *main_window = NULL;
static dmi_display_list *global_dlist = NULL;

static int get_current_color_temp_preset(dmi_display *disp) {
    if (!disp || !disp->dh) return -1;

    DDCA_Non_Table_Vcp_Value valrec;
    DDCA_Status rc = ddca_get_non_table_vcp_value(disp->dh, 0x14, &valrec);

    if (rc == 0) {
        return valrec.sl;
    }

    if (disp->i2c_busno >= 0) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "ddcutil getvcp 14 --bus=%d 2>/dev/null", disp->i2c_busno);

        FILE *fp = popen(cmd, "r");
        if (!fp) return -1;

        char line[512];
        int value = -1;

        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "current value") != NULL) {
                char *eq = strchr(line, '=');
                if (eq && sscanf(eq + 1, " 0x%x", &value) == 1) {
                    break;
                }
            }
        }

        pclose(fp);
        return value;
    }

    return -1;
}

static int set_color_temp_preset(dmi_display *disp, guint8 preset_code) {
    if (!disp) return -1;

    return dmi_display_set_vcp_value(disp, 0x14, preset_code);
}

static void on_color_temp_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data) {
    dmi_display *disp = user_data;
    if (!disp) return;

    guint selected = gtk_drop_down_get_selected(dropdown);
    if (selected >= color_temp_presets_count) return;

    guint8 preset_code = color_temp_presets[selected].code;
    const char *preset_name = color_temp_presets[selected].name;

    DEBUG_PRINT("Setting color temperature to: 0x%02x (%s)\n", preset_code, preset_name);

    int rc = set_color_temp_preset(disp, preset_code);
    if (rc != 0) {
        g_printerr("Failed to set color temperature preset: %d\n", rc);
    } else {
        g_print("Color temperature set to %s\n", preset_name);
    }
}

static void on_brightness_changed(GtkRange *range, gpointer user_data);
static void on_contrast_changed(GtkRange *range, gpointer user_data);
static void on_volume_changed(GtkRange *range, gpointer user_data);
static void on_input_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data);
static void on_mouse_motion(GtkEventControllerMotion *controller, double x, double y,
                            gpointer user_data);
static void on_mouse_leave(GtkEventControllerMotion *controller, gpointer user_data);
static gboolean check_and_close(gpointer data);
static DisplaySection *display_section_new(dmi_display *disp);
static void display_section_free(DisplaySection *section);
static void display_section_attach_to_notebook(DisplaySection *section, GtkNotebook *notebook,
                                               const char *display_name, const char *input_name);
static int get_input_code_from_index(guint index);

static void on_brightness_changed(GtkRange *range, gpointer user_data) {
    dmi_display *disp = user_data;
    if (!disp) return;

    guint16 new_val = (guint16)gtk_range_get_value(range);
    int rc = dmi_display_set_brightness(disp, new_val);
    if (rc != 0) {
        g_printerr("Failed to set brightness: %d\n", rc);
    }
}

static void on_contrast_changed(GtkRange *range, gpointer user_data) {
    dmi_display *disp = user_data;
    if (!disp) return;

    guint16 new_val = (guint16)gtk_range_get_value(range);
    int rc = dmi_display_set_contrast(disp, new_val);
    if (rc != 0) {
        g_printerr("Failed to set contrast: %d\n", rc);
    }
}

static void on_volume_changed(GtkRange *range, gpointer user_data) {
    dmi_display *disp = user_data;
    if (!disp) return;

    guint16 new_val = (guint16)gtk_range_get_value(range);
    int rc = dmi_display_set_volume(disp, new_val);
    if (rc != 0) {
        g_printerr("Failed to set volume: %d\n", rc);
    }
}

static void toggle_window_visibility() {
    if (!main_window) return;

    gboolean visible = gtk_widget_get_visible(main_window);
    if (visible) {
        gtk_widget_set_visible(main_window, FALSE);

        while (g_main_context_pending(NULL)) {
            g_main_context_iteration(NULL, FALSE);
        }
    } else {

        mouse_inside = TRUE;
        mouse_leave_time = 0;

        gtk_widget_set_visible(main_window, TRUE);
        gtk_window_present(GTK_WINDOW(main_window));

        gtk_widget_grab_focus(main_window);

        GdkDisplay *display = gtk_widget_get_display(main_window);
        GListModel *monitors = gdk_display_get_monitors(display);
        if (monitors && g_list_model_get_n_items(monitors) > 0) {
            GdkMonitor *monitor = g_list_model_get_item(monitors, 0);
            if (monitor) {
                GdkRectangle geometry;
                gdk_monitor_get_geometry(monitor, &geometry);

                int window_width, window_height;
                gtk_window_get_default_size(GTK_WINDOW(main_window), &window_width, &window_height);

                int x = geometry.x + (geometry.width - window_width) / 2;
                int y = geometry.y + (geometry.height - window_height) / 2;

                gtk_window_present(GTK_WINDOW(main_window));
                g_object_unref(monitor);
            }
        }
    }
}

static void on_app_activate_existing(GtkApplication *app, gpointer user_data) {
    if (main_window) {
        toggle_window_visibility();
    }
}

static void update_input_dropdown_labels(DisplaySection *section, int active_code) {
    if (!section || !section->input_combo || !section->supported_inputs) return;

    GtkDropDown *dropdown = GTK_DROP_DOWN(section->input_combo);
    GListModel *model = gtk_drop_down_get_model(dropdown);
    if (!GTK_IS_STRING_LIST(model)) return;

    GtkStringList *str_list = GTK_STRING_LIST(model);

    guint n_items = g_list_model_get_n_items(G_LIST_MODEL(str_list));
    for (guint i = n_items; i > 0; i--) {
        gtk_string_list_remove(str_list, i - 1);
    }

    for (guint i = 0; i < section->supported_inputs->len; i++) {
        guint input_idx = g_array_index(section->supported_inputs, guint, i);
        if (input_idx < known_inputs_count) {
            char label[128];
            if (known_inputs[input_idx].code == active_code) {
                snprintf(label, sizeof(label), "   %s", known_inputs[input_idx].name);
            } else {
                snprintf(label, sizeof(label), "   %s", known_inputs[input_idx].name);
            }
            gtk_string_list_append(str_list, label);
        }
    }
}

static void on_input_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data) {
    DisplaySection *section = user_data;
    if (!section || !section->wrapper || !section->wrapper->ddc) return;

    guint selected = gtk_drop_down_get_selected(dropdown);
    if (!section->supported_inputs || selected >= section->supported_inputs->len) return;

    guint input_idx = g_array_index(section->supported_inputs, guint, selected);
    if (input_idx >= known_inputs_count) return;

    int input_code = known_inputs[input_idx].code;
    const char *input_name = known_inputs[input_idx].name;

    int current_input = dmi_display_get_input(section->wrapper->ddc);
    if (current_input == input_code) {
        DEBUG_PRINT("Input already set to 0x%02x, skipping\n", input_code);
        return;
    }

    DEBUG_PRINT("Setting input to: 0x%02x (%s)\n", input_code, input_name);

    gtk_widget_set_sensitive(GTK_WIDGET(dropdown), FALSE);

    GtkRoot *root = gtk_widget_get_root(GTK_WIDGET(dropdown));
    GtkWidget *window = root ? GTK_WIDGET(root) : NULL;
    if (window && GTK_IS_WINDOW(window)) {
        gtk_widget_set_cursor_from_name(window, "wait");
    }

    g_print("Switching display input to %s...\n", input_name);
    g_print("Note: The display may go black temporarily during input switch.\n");

    int rc = dmi_display_set_input(section->wrapper->ddc, input_code);

    gtk_widget_set_sensitive(GTK_WIDGET(dropdown), TRUE);

    if (window && GTK_IS_WINDOW(window)) {
        gtk_widget_set_cursor(window, NULL);
    }

    if (rc != 0) {
        g_printerr("Failed to set input: %d\n", rc);
        g_printerr("The display may not support switching to this input via DDC/CI.\n");

        for (guint i = 0; i < section->supported_inputs->len; i++) {
            guint idx = g_array_index(section->supported_inputs, guint, i);
            if (idx < known_inputs_count && known_inputs[idx].code == current_input) {
                gtk_drop_down_set_selected(dropdown, i);
                break;
            }
        }
    } else {
        g_print("Input switch command sent successfully.\n");

        update_input_dropdown_labels(section, input_code);

        gtk_drop_down_set_selected(dropdown, selected);

        if (section->notebook && section->display_number > 0) {
            int page_num = gtk_notebook_page_num(section->notebook, section->frame);
            if (page_num >= 0) {
                GtkWidget *tab_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
                GtkWidget *tab_icon = gtk_image_new_from_icon_name("video-display");

                char tab_text[128];
                snprintf(tab_text, sizeof(tab_text), "Display %u • %s", section->display_number,
                         input_name);
                GtkWidget *tab_label = gtk_label_new(tab_text);

                gtk_box_append(GTK_BOX(tab_box), tab_icon);
                gtk_box_append(GTK_BOX(tab_box), tab_label);

                gtk_notebook_set_tab_label(section->notebook, section->frame, tab_box);
            }
        }
    }
}

static void on_mouse_motion(GtkEventControllerMotion *controller, double x, double y,
                            gpointer user_data) {
    mouse_inside = TRUE;
    mouse_leave_time = 0;
}

static void on_mouse_leave(GtkEventControllerMotion *controller, gpointer user_data) {
    mouse_inside = FALSE;
    mouse_leave_time = g_get_monotonic_time();

    if (close_timeout_id == 0) {
        close_timeout_id = g_timeout_add_seconds(1, check_and_close, user_data);
    }
}

static gboolean check_and_close(gpointer data) {
    if (!mouse_inside && mouse_leave_time > 0) {
        gint64 now = g_get_monotonic_time();
        if ((now - mouse_leave_time) >= AUTO_CLOSE_DELAY_SEC * G_USEC_PER_SEC) {

            gtk_widget_set_visible(GTK_WIDGET(data), FALSE);
            close_timeout_id = 0;
            return G_SOURCE_REMOVE;
        }
    }
    return G_SOURCE_CONTINUE;
}

static int get_input_code_from_index(guint index) {
    if (index >= known_inputs_count) return -1;
    return known_inputs[index].code;
}

static DisplaySection *display_section_new(dmi_display *disp) {
    if (!disp) return NULL;

    if (dmi_display_get_ctemp(disp) != 0) {
        disp->ctemp_val = 0;
        disp->ctemp_max = 0;
    }

    if (dmi_display_get_volume(disp) != 0) {
        disp->volume_val = 0;
        disp->volume_max = 0;
    }

    DisplaySection *section = g_malloc0(sizeof(DisplaySection));

    section->wrapper = g_malloc0(sizeof(DisplayWrapper));
    section->wrapper->ddc = disp;
    section->wrapper->i2c_busno = disp->i2c_busno;

    section->frame = gtk_frame_new(NULL);
    gtk_widget_add_css_class(section->frame, "monitor-frame");
    gtk_widget_set_margin_bottom(section->frame, 20);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_frame_set_child(GTK_FRAME(section->frame), grid);

    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(header_box, 8);
    gtk_widget_set_margin_top(header_box, 14);
    gtk_widget_set_margin_bottom(header_box, 30);

    GtkWidget *icon = gtk_image_new_from_icon_name("video-display");
    gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(icon, 0);
    gtk_box_append(GTK_BOX(header_box), icon);

    section->label = gtk_label_new(disp->info.model_name);
    gtk_label_set_xalign(GTK_LABEL(section->label), 0.0);
    gtk_widget_set_valign(section->label, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(section->label, TRUE);
    gtk_box_append(GTK_BOX(header_box), section->label);

    section->brightness_label = gtk_label_new("Brightness");
    gtk_label_set_xalign(GTK_LABEL(section->brightness_label), 0.0);
    gtk_widget_set_margin_start(section->brightness_label, 8);

    section->brightness_scale =
        gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, disp->brightness_max, 1);
    gtk_range_set_value(GTK_RANGE(section->brightness_scale), disp->brightness_val);
    gtk_scale_set_value_pos(GTK_SCALE(section->brightness_scale), GTK_POS_RIGHT);
    gtk_scale_set_digits(GTK_SCALE(section->brightness_scale), 0);
    gtk_scale_set_draw_value(GTK_SCALE(section->brightness_scale), TRUE);
    gtk_widget_set_hexpand(section->brightness_scale, TRUE);
    g_signal_connect(section->brightness_scale, "value-changed", G_CALLBACK(on_brightness_changed),
                     disp);

    section->contrast_label = gtk_label_new("Contrast");
    gtk_label_set_xalign(GTK_LABEL(section->contrast_label), 0.0);
    gtk_widget_set_margin_start(section->contrast_label, 8);

    if (disp->contrast_max > 0) {
        section->contrast_scale =
            gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, disp->contrast_max, 1);
        gtk_range_set_value(GTK_RANGE(section->contrast_scale), disp->contrast_val);
        gtk_scale_set_value_pos(GTK_SCALE(section->contrast_scale), GTK_POS_RIGHT);
        gtk_scale_set_digits(GTK_SCALE(section->contrast_scale), 0);
        gtk_scale_set_draw_value(GTK_SCALE(section->contrast_scale), TRUE);
        gtk_widget_set_hexpand(section->contrast_scale, TRUE);
        g_signal_connect(section->contrast_scale, "value-changed", G_CALLBACK(on_contrast_changed),
                         disp);
    } else {

        section->contrast_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
        gtk_range_set_value(GTK_RANGE(section->contrast_scale), 0);
        gtk_scale_set_value_pos(GTK_SCALE(section->contrast_scale), GTK_POS_RIGHT);
        gtk_scale_set_digits(GTK_SCALE(section->contrast_scale), 0);
        gtk_scale_set_draw_value(GTK_SCALE(section->contrast_scale), TRUE);
        gtk_widget_set_hexpand(section->contrast_scale, TRUE);
        gtk_widget_set_sensitive(section->contrast_scale, FALSE);
        gtk_widget_set_sensitive(section->contrast_label, FALSE);
    }

    section->ctemp_label = gtk_label_new("Color Temperature");
    gtk_label_set_xalign(GTK_LABEL(section->ctemp_label), 0.0);
    gtk_widget_set_margin_start(section->ctemp_label, 8);

    GtkStringList *ctemp_list = gtk_string_list_new(NULL);
    section->ctemp_combo = gtk_drop_down_new(G_LIST_MODEL(ctemp_list), NULL);

    int current_preset = get_current_color_temp_preset(disp);
    guint selected_preset = 0;

    for (guint i = 0; i < color_temp_presets_count; i++) {
        gtk_string_list_append(ctemp_list, color_temp_presets[i].name);

        if (color_temp_presets[i].code == current_preset) {
            selected_preset = i;
        }
    }

    gtk_drop_down_set_selected(GTK_DROP_DOWN(section->ctemp_combo), selected_preset);
    gtk_widget_set_margin_start(section->ctemp_combo, 8);
    gtk_widget_set_margin_end(section->ctemp_combo, 8);
    gtk_widget_set_margin_bottom(section->ctemp_combo, 8);

    g_signal_connect(section->ctemp_combo, "notify::selected", G_CALLBACK(on_color_temp_changed),
                     disp);

    section->volume_label = gtk_label_new("Volume");
    gtk_label_set_xalign(GTK_LABEL(section->volume_label), 0.0);
    gtk_widget_set_margin_start(section->volume_label, 8);

    if (disp->volume_max > 0) {
        section->volume_scale =
            gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, disp->volume_max, 1);
        gtk_range_set_value(GTK_RANGE(section->volume_scale), disp->volume_val);
        gtk_scale_set_value_pos(GTK_SCALE(section->volume_scale), GTK_POS_RIGHT);
        gtk_scale_set_digits(GTK_SCALE(section->volume_scale), 0);
        gtk_scale_set_draw_value(GTK_SCALE(section->volume_scale), TRUE);
        gtk_widget_set_hexpand(section->volume_scale, TRUE);
        g_signal_connect(section->volume_scale, "value-changed", G_CALLBACK(on_volume_changed),
                         disp);
    } else {

        section->volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
        gtk_range_set_value(GTK_RANGE(section->volume_scale), 0);
        gtk_scale_set_value_pos(GTK_SCALE(section->volume_scale), GTK_POS_RIGHT);
        gtk_scale_set_digits(GTK_SCALE(section->volume_scale), 0);
        gtk_scale_set_draw_value(GTK_SCALE(section->volume_scale), TRUE);
        gtk_widget_set_hexpand(section->volume_scale, TRUE);
        gtk_widget_set_sensitive(section->volume_scale, FALSE);
        gtk_widget_set_sensitive(section->volume_label, FALSE);
    }

    GtkWidget *input_label = gtk_label_new("Input Source");
    gtk_label_set_xalign(GTK_LABEL(input_label), 0.0);
    gtk_widget_set_margin_start(input_label, 8);
    gtk_widget_set_margin_bottom(input_label, 10);

    section->supported_inputs = dmi_display_get_supported_inputs(disp);
    int current_input_code = dmi_display_get_input(disp);
    DEBUG_PRINT("Current input code: 0x%02x\n", current_input_code);

    GtkStringList *str_list = gtk_string_list_new(NULL);
    section->input_combo = gtk_drop_down_new(G_LIST_MODEL(str_list), NULL);

    guint selected_index = 0;
    if (section->supported_inputs && section->supported_inputs->len > 0) {
        for (guint i = 0; i < section->supported_inputs->len; i++) {
            guint input_idx = g_array_index(section->supported_inputs, guint, i);
            if (input_idx < known_inputs_count) {
                char label[128];
                if (known_inputs[input_idx].code == current_input_code) {

                    snprintf(label, sizeof(label), "   %s", known_inputs[input_idx].name);
                    selected_index = i;
                } else {
                    snprintf(label, sizeof(label), "   %s", known_inputs[input_idx].name);
                }
                gtk_string_list_append(str_list, label);
            }
        }
    } else {
        gtk_string_list_append(str_list, "(None available)");
        gtk_widget_set_sensitive(section->input_combo, FALSE);
    }

    gtk_drop_down_set_selected(GTK_DROP_DOWN(section->input_combo), selected_index);
    gtk_widget_set_margin_start(section->input_combo, 8);
    gtk_widget_set_margin_end(section->input_combo, 8);
    gtk_widget_set_margin_bottom(section->input_combo, 8);

    g_signal_connect(section->input_combo, "notify::selected", G_CALLBACK(on_input_changed),
                     section);

    int row = 0;
    gtk_grid_attach(GTK_GRID(grid), header_box, 0, row++, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), section->brightness_label, 0, row++, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), section->brightness_scale, 0, row++, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), section->contrast_label, 0, row++, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), section->contrast_scale, 0, row++, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), section->ctemp_label, 0, row++, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), section->ctemp_combo, 0, row++, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), section->volume_label, 0, row++, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), section->volume_scale, 0, row++, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), input_label, 0, row++, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), section->input_combo, 0, row++, 1, 1);

    return section;
}

static void display_section_free(DisplaySection *section) {
    if (!section) return;

    DEBUG_PRINT("Freeing display section\n");

    if (section->supported_inputs) {
        g_array_free(section->supported_inputs, TRUE);
    }
    if (section->wrapper) {
        g_free(section->wrapper);
    }

    g_free(section);
}

static void display_section_attach_to_notebook(DisplaySection *section, GtkNotebook *notebook,
                                               const char *display_name, const char *input_name) {

    GtkWidget *tab_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(tab_box, "tab-content");

    GtkWidget *tab_icon = gtk_image_new_from_icon_name("video-display");
    GtkWidget *tab_label = gtk_label_new(display_name);

    GtkWidget *pill_frame = gtk_frame_new(NULL);
    GtkWidget *pill_label = gtk_label_new(input_name);

    PangoAttrList *attrs = pango_attr_list_new();
    PangoAttribute *weight_attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    PangoAttribute *size_attr = pango_attr_size_new(8 * PANGO_SCALE);
    pango_attr_list_insert(attrs, weight_attr);
    pango_attr_list_insert(attrs, size_attr);
    gtk_label_set_attributes(GTK_LABEL(pill_label), attrs);
    pango_attr_list_unref(attrs);

    gtk_frame_set_child(GTK_FRAME(pill_frame), pill_label);
    gtk_widget_add_css_class(pill_frame, "tiny-pill");
    gtk_widget_set_valign(pill_frame, GTK_ALIGN_CENTER);

    gtk_box_append(GTK_BOX(tab_box), tab_icon);
    gtk_box_append(GTK_BOX(tab_box), tab_label);
    gtk_box_append(GTK_BOX(tab_box), pill_frame);

    gtk_notebook_append_page(notebook, section->frame, tab_box);

    section->notebook = notebook;
    section->display_number = gtk_notebook_get_n_pages(notebook);
}

static void on_window_destroy(GtkWidget *window, gpointer user_data) {
    GList *sections = user_data;

    for (GList *l = sections; l != NULL; l = l->next) {
        display_section_free(l->data);
    }
    g_list_free(sections);

    if (close_timeout_id > 0) {
        g_source_remove(close_timeout_id);
        close_timeout_id = 0;
    }

    main_window = NULL;
}

static void app_activate(GtkApplication *app, gpointer user_data) {

    if (main_window) {
        toggle_window_visibility();
        return;
    }

    static gboolean initialized = FALSE;
    if (!initialized) {

        DDCA_Status init_status = ddca_init(NULL, -1, -1);
        if (init_status != 0) {
            g_printerr("Failed to initialize DDC library: %d\n", init_status);
            g_application_quit(G_APPLICATION(app));
            return;
        }

        g_print("Detecting displays...\n");
        static dmi_display_list dlist;
        dlist = dmi_display_list_init(false);
        global_dlist = &dlist;

        if (dlist.ct == 0) {
            g_printerr("No DDC/CI capable displays found.\n");
            g_printerr("Make sure:\n");
            g_printerr("  - Your monitor supports DDC/CI\n");
            g_printerr("  - DDC/CI is enabled in your monitor's OSD\n");
            g_printerr("  - You have permission to access /dev/i2c-* devices\n");
            g_printerr("  - The i2c-dev kernel module is loaded\n");
            g_application_quit(G_APPLICATION(app));
            return;
        }

        g_print("Found %u display(s)\n", dlist.ct);
        initialized = TRUE;
    }

    dmi_display_list *dlist = global_dlist;

    if (!dlist || dlist->ct == 0) {
        g_printerr("No displays to show\n");
        return;
    }

    GtkCssProvider *css = gtk_css_provider_new();
    GError *error = NULL;
    gtk_css_provider_load_from_path(css, "style.css");
    if (error) {
        g_warning("Failed to load CSS: %s", error->message);
        g_error_free(error);
    } else {
        gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                                   GTK_STYLE_PROVIDER(css),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    g_object_unref(css);

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(window), WINDOW_WIDTH, -1);
    gtk_window_set_title(GTK_WINDOW(window), "Display Controls");

    gtk_window_set_hide_on_close(GTK_WINDOW(window), TRUE);

    GtkEventController *motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", G_CALLBACK(on_mouse_motion), window);
    g_signal_connect(motion, "leave", G_CALLBACK(on_mouse_leave), window);
    gtk_widget_add_controller(window, motion);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);
    gtk_widget_set_margin_start(main_box, 20);
    gtk_widget_set_margin_end(main_box, 20);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_widget_set_hexpand(notebook, TRUE);
    gtk_widget_set_vexpand(notebook, TRUE);

    gtk_widget_set_can_focus(notebook, FALSE);

    gtk_box_append(GTK_BOX(main_box), notebook);

    GList *sections = NULL;

    for (guint it = 0; it < dlist->ct; it++) {
        dmi_display *disp = dmi_display_list_get(dlist, it);
        g_print("Creating section for display #%u: %p\n", it + 1, (void *)disp);

        DisplaySection *section = display_section_new(disp);
        if (!section) {
            g_printerr("Failed to create section for display %u\n", it + 1);
            continue;
        }

        sections = g_list_append(sections, section);

        int current_input_code = dmi_display_get_input(disp);
        const char *current_input_name = "Unknown";

        for (size_t i = 0; i < known_inputs_count; i++) {
            if (known_inputs[i].code == current_input_code) {
                current_input_name = known_inputs[i].name;
                break;
            }
        }

        char display_name[64];
        snprintf(display_name, sizeof(display_name), "Display %u", it + 1);

        display_section_attach_to_notebook(section, GTK_NOTEBOOK(notebook), display_name,
                                           current_input_name);
    }

    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), sections);

    gtk_window_set_child(GTK_WINDOW(window), main_box);

    main_window = window;

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {

    GtkApplication *app = gtk_application_new("com.github.dmi-gtk", G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(app, "activate", G_CALLBACK(app_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);

    g_object_unref(app);
    if (global_dlist) {
        dmi_display_list_free(global_dlist);
    }

    return status;
}