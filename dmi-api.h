#ifndef DMI_API_H
#define DMI_API_H

#include <ddcutil_c_api.h>
#include <glib.h>

typedef struct _dmi_display dmi_display;
typedef struct _dmi_display_list dmi_display_list;

typedef struct {
    int code;
    const char *name;
} InputSource;

struct _dmi_display {
    DDCA_Display_Info info;
    DDCA_Display_Handle dh;
    guint16 brightness_val;
    guint16 brightness_max;
    guint16 contrast_val;
    guint16 contrast_max;
    guint16 ctemp_val;
    guint16 ctemp_max;
    guint16 volume_val;
    guint16 volume_max;
    int i2c_busno;
};

struct _dmi_display_list {
    guint ct;
    GArray *list;
};

dmi_display_list dmi_display_list_init(gboolean wait);
void dmi_display_list_free(dmi_display_list *dlist);
dmi_display *dmi_display_list_get(dmi_display_list *dlist, guint index);

int dmi_display_get_brightness(dmi_display *disp);
int dmi_display_set_brightness(dmi_display *disp, guint16 new_val);

int dmi_display_get_contrast(dmi_display *disp);
int dmi_display_set_contrast(dmi_display *disp, guint16 new_val);

int dmi_display_get_ctemp(dmi_display *disp);
int dmi_display_set_ctemp(dmi_display *disp, guint16 new_val);

int dmi_display_get_volume(dmi_display *disp);
int dmi_display_set_volume(dmi_display *disp, guint16 new_val);

int dmi_display_get_input(dmi_display *disp);
int dmi_display_set_input(dmi_display *disp, guint8 input_code);
GArray *dmi_display_get_supported_inputs(dmi_display *disp);

int dmi_display_set_vcp_value(dmi_display *disp, guint8 code, guint16 value);

extern const InputSource known_inputs[];
extern const size_t known_inputs_count;

#endif