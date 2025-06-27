#include "dmi-api.h"
#include <stdio.h>
#include <stdlib.h>

#define BRIGHTNESS_CODE 0x10

int dmi_display_get_brightness(dmi_display *disp) {
    DDCA_Non_Table_Vcp_Value valrec;
    DDCA_Status ddcrc = ddca_get_non_table_vcp_value(disp->dh, BRIGHTNESS_CODE, &valrec);

    if (ddcrc != 0) return ddcrc;

    disp->brightness_val = (valrec.sh << 8) | valrec.sl;
    disp->brightness_max = (valrec.mh << 8) | valrec.ml;

    return 0;
}

int dmi_display_set_brightness(dmi_display *disp, guint16 new_val) {
    if (new_val > disp->brightness_max)
        return -1;

    guint8 high = new_val >> 8;
    guint8 low  = new_val & 0xFF;

    DDCA_Status ddcrc = ddca_set_non_table_vcp_value(disp->dh, BRIGHTNESS_CODE, high, low);
    if (ddcrc != 0) return ddcrc;

    disp->brightness_val = new_val;
    return 0;
}

#define CONTRAST_CODE 0x12

int dmi_display_get_contrast(dmi_display *disp) {
    DDCA_Non_Table_Vcp_Value valrec;
    DDCA_Status ddcrc = ddca_get_non_table_vcp_value(disp->dh, CONTRAST_CODE, &valrec);

    if (ddcrc != 0) return ddcrc;

    guint16 current = (valrec.sh << 8) | valrec.sl;
    guint16 maximum = (valrec.mh << 8) | valrec.ml;

    if (maximum == 0 || maximum > 1000)
        return -1;

    disp->contrast_val = current;
    disp->contrast_max = maximum;

    return 0;
}

int dmi_display_set_contrast(dmi_display *disp, guint16 new_val) {
    if (new_val > disp->contrast_max)
        return -1;

    guint8 high = new_val >> 8;
    guint8 low  = new_val & 0xFF;

    DDCA_Status ddcrc = ddca_set_non_table_vcp_value(disp->dh, CONTRAST_CODE, high, low);
    if (ddcrc != 0) return ddcrc;

    disp->contrast_val = new_val;
    return 0;
}

dmi_display_list dmi_display_list_init(gboolean wait) {
    DDCA_Display_Info_List *dinfos;
    ddca_get_display_info_list2(FALSE, &dinfos);

    dmi_display_list dlist;
    dlist.ct = 0;
    dlist.list = g_array_new(FALSE, FALSE, sizeof(dmi_display*));

    for (guint i = 0; i < dinfos->ct; i++) {
        dmi_display *disp = malloc(sizeof(dmi_display));
        disp->info = dinfos->info[i];
        disp->contrast_val = 0;
        disp->contrast_max = 0;

        if (ddca_open_display2(disp->info.dref, wait, &disp->dh) != 0) {
            free(disp);
            continue;
        }

        if (dmi_display_get_brightness(disp) != 0) {
            ddca_close_display(disp->dh);
            free(disp);
            continue;
        }

        if (dmi_display_get_contrast(disp) != 0) {
            disp->contrast_val = 0;
            disp->contrast_max = 0;
        }
        disp->i2c_busno = i + 3;
        g_array_append_val(dlist.list, disp);
        dlist.ct++;
    }

    ddca_free_display_info_list(dinfos);
    return dlist;
}

void dmi_display_list_free(dmi_display_list *dlist) {
    for (guint i = 0; i < dlist->ct; i++) {
        dmi_display *disp = g_array_index(dlist->list, dmi_display*, i);
        ddca_close_display(disp->dh);
        free(disp);
    }
    g_array_free(dlist->list, TRUE);
}

dmi_display* dmi_display_list_get(dmi_display_list *dlist, guint index) {
    return g_array_index(dlist->list, dmi_display*, index);
}

int dmi_display_set_vcp_value(dmi_display *disp, guint8 code, guint16 value) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "ddcutil --bus=%d setvcp 0x%02X %d >/dev/null 2>&1",
             disp->i2c_busno, code, value);
    int rc = system(cmd);
    return rc == 0 ? 0 : 1;
}

int dmi_display_set_input(dmi_display *disp, guint8 input_code) {
    return dmi_display_set_vcp_value(disp, 0x60, input_code);
}
