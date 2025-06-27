#ifndef dmi_API_H
#define dmi_API_H

#include <glib.h>
#include <ddcutil_c_api.h>

typedef struct {
    DDCA_Display_Info info;
    DDCA_Display_Handle dh;
    guint16 brightness_val;
    guint16 brightness_max;
    guint16 contrast_val;
    guint16 contrast_max;
    int i2c_busno;
} dmi_display;

typedef struct {
    guint ct;
    GArray *list;
} dmi_display_list;

dmi_display_list dmi_display_list_init(gboolean wait);
void dmi_display_list_free(dmi_display_list *dlist);
dmi_display* dmi_display_list_get(dmi_display_list *dlist, guint index);

int dmi_display_get_brightness(dmi_display *disp);
int dmi_display_set_brightness(dmi_display *disp, guint16 new_val);
int dmi_display_get_contrast(dmi_display *disp);
int dmi_display_set_contrast(dmi_display *disp, guint16 new_val);
int dmi_display_set_vcp_value(dmi_display *disp, guint8 code, guint16 value);
int dmi_display_set_input(dmi_display *disp, guint8 input_code);

#endif
