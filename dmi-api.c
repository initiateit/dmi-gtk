#include "dmi-api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VCP_BRIGHTNESS 0x10
#define VCP_CONTRAST 0x12
#define VCP_CTEMP 0x14
#define VCP_VOL 0x62
#define VCP_INPUT 0x60

#define MAX_DISPLAYS 10
#define MAX_LINE_LEN 512
#define DEBUG_MODE 0

#if DEBUG_MODE
#define DEBUG_PRINT(fmt, ...) g_print("[DMI-API] " fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...) ((void)0)
#endif

const InputSource known_inputs[] = {
    {0x0f, "DisplayPort-1"},
    {0x10, "DisplayPort-2"},
    {0x11, "HDMI-1"},
    {0x12, "HDMI-2"},
    {0x01, "VGA"},
    {0x03, "DVI"},
    {0x14, "Composite"},
    {0x15, "S-Video"},
    {0x1e, "USB-C"},
    {0x1f, "Thunderbolt"},
    {0x20, "Internal DisplayPort"},
};

const size_t known_inputs_count = sizeof(known_inputs) / sizeof(known_inputs[0]);

typedef struct {
    char model_name[64];
    char mfg_id[32];
    int bus_number;
} BusInfo;

static int get_all_display_bus_numbers(BusInfo **bus_info_array);
static int find_bus_for_display(BusInfo *bus_info_array, int count, const char *model_name,
                                const char *mfg_id);
static void free_bus_info_array(BusInfo *array);

int dmi_display_get_brightness(dmi_display *disp) {
    if (!disp || !disp->dh) return -1;

    DDCA_Non_Table_Vcp_Value valrec;
    DDCA_Status ddcrc = ddca_get_non_table_vcp_value(disp->dh, VCP_BRIGHTNESS, &valrec);

    if (ddcrc != 0) {
        DEBUG_PRINT("Failed to get brightness: %d\n", ddcrc);
        return ddcrc;
    }

    disp->brightness_val = (valrec.sh << 8) | valrec.sl;
    disp->brightness_max = (valrec.mh << 8) | valrec.ml;

    DEBUG_PRINT("Brightness: %d/%d\n", disp->brightness_val, disp->brightness_max);
    return 0;
}

int dmi_display_set_brightness(dmi_display *disp, guint16 new_val) {
    if (!disp || !disp->dh) return -1;
    if (new_val > disp->brightness_max) return -1;

    guint8 high = new_val >> 8;
    guint8 low = new_val & 0xFF;

    DDCA_Status ddcrc = ddca_set_non_table_vcp_value(disp->dh, VCP_BRIGHTNESS, high, low);
    if (ddcrc != 0) {
        DEBUG_PRINT("Failed to set brightness: %d\n", ddcrc);
        return ddcrc;
    }

    disp->brightness_val = new_val;
    return 0;
}

int dmi_display_get_contrast(dmi_display *disp) {
    if (!disp || !disp->dh) return -1;

    DDCA_Non_Table_Vcp_Value valrec;
    DDCA_Status ddcrc = ddca_get_non_table_vcp_value(disp->dh, VCP_CONTRAST, &valrec);

    if (ddcrc != 0) {
        DEBUG_PRINT("Failed to get contrast: %d\n", ddcrc);
        return ddcrc;
    }

    guint16 current = (valrec.sh << 8) | valrec.sl;
    guint16 maximum = (valrec.mh << 8) | valrec.ml;

    if (maximum == 0 || maximum > 1000) {
        DEBUG_PRINT("Invalid contrast max value: %d\n", maximum);
        return -1;
    }

    disp->contrast_val = current;
    disp->contrast_max = maximum;

    DEBUG_PRINT("Contrast: %d/%d\n", disp->contrast_val, disp->contrast_max);
    return 0;
}

int dmi_display_set_contrast(dmi_display *disp, guint16 new_val) {
    if (!disp || !disp->dh) return -1;
    if (new_val > disp->contrast_max) return -1;

    guint8 high = new_val >> 8;
    guint8 low = new_val & 0xFF;

    DDCA_Status ddcrc = ddca_set_non_table_vcp_value(disp->dh, VCP_CONTRAST, high, low);
    if (ddcrc != 0) {
        DEBUG_PRINT("Failed to set contrast: %d\n", ddcrc);
        return ddcrc;
    }

    disp->contrast_val = new_val;
    return 0;
}

int dmi_display_get_ctemp(dmi_display *disp) {
    if (!disp || !disp->dh) return -1;

    DDCA_Non_Table_Vcp_Value valrec;
    DDCA_Status ddcrc = ddca_get_non_table_vcp_value(disp->dh, VCP_CTEMP, &valrec);

    if (ddcrc != 0) {
        DEBUG_PRINT("Failed to get Color Temp: %d\n", ddcrc);
        return ddcrc;
    }

    guint16 current = (valrec.sh << 8) | valrec.sl;
    guint16 maximum = (valrec.mh << 8) | valrec.ml;

    if (maximum == 0 || maximum > 1000) {
        DEBUG_PRINT("Invalid contrast max value: %d\n", maximum);
        return -1;
    }

    disp->ctemp_val = current;
    disp->ctemp_max = maximum;

    DEBUG_PRINT("Color Temp: %d/%d\n", disp->ctemp_val, disp->ctemp_max);
    return 0;
}

int dmi_display_set_ctemp(dmi_display *disp, guint16 new_val) {
    if (!disp || !disp->dh) return -1;
    if (new_val > disp->ctemp_max) return -1;

    guint8 high = new_val >> 8;
    guint8 low = new_val & 0xFF;

    DDCA_Status ddcrc = ddca_set_non_table_vcp_value(disp->dh, VCP_CTEMP, high, low);
    if (ddcrc != 0) {
        DEBUG_PRINT("Failed to set color temp: %d\n", ddcrc);
        return ddcrc;
    }

    disp->ctemp_val = new_val;
    return 0;
}

int dmi_display_get_volume(dmi_display *disp) {
    if (!disp || !disp->dh) return -1;

    DDCA_Non_Table_Vcp_Value valrec;
    DDCA_Status ddcrc = ddca_get_non_table_vcp_value(disp->dh, VCP_VOL, &valrec);

    if (ddcrc != 0) {
        DEBUG_PRINT("Failed to get volume: %d\n", ddcrc);
        return ddcrc;
    }

    guint16 current = (valrec.sh << 8) | valrec.sl;
    guint16 maximum = (valrec.mh << 8) | valrec.ml;

    if (maximum == 0 || maximum > 1000) {
        DEBUG_PRINT("Invalid volume max value: %d\n", maximum);
        return -1;
    }

    disp->volume_val = current;
    disp->volume_max = maximum;

    DEBUG_PRINT("volume: %d/%d\n", disp->volume_val, disp->volume_max);
    return 0;
}

int dmi_display_set_volume(dmi_display *disp, guint16 new_val) {
    if (!disp || !disp->dh) return -1;
    if (new_val > disp->volume_max) return -1;

    guint8 high = new_val >> 8;
    guint8 low = new_val & 0xFF;

    DDCA_Status ddcrc = ddca_set_non_table_vcp_value(disp->dh, VCP_VOL, high, low);
    if (ddcrc != 0) {
        DEBUG_PRINT("Failed to set volume: %d\n", ddcrc);
        return ddcrc;
    }

    disp->volume_val = new_val;
    return 0;
}

int dmi_display_get_input(dmi_display *disp) {
    if (!disp || !disp->dh) return -1;

    DDCA_Non_Table_Vcp_Value valrec;
    DDCA_Status rc = ddca_get_non_table_vcp_value(disp->dh, VCP_INPUT, &valrec);

    if (rc == 0) {

        int value = valrec.sl;
        DEBUG_PRINT("Current input: 0x%02x (via handle)\n", value);
        return value;
    }

    if (disp->i2c_busno < 0) return -1;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ddcutil getvcp %02x --bus=%d 2>/dev/null", VCP_INPUT,
             disp->i2c_busno);

    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;

    char line[MAX_LINE_LEN];
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
    DEBUG_PRINT("Current input: 0x%02x (via command)\n", value);
    return value;
}

int dmi_display_set_input(dmi_display *disp, guint8 input_code) {
    if (!disp) return -1;

    if (disp->i2c_busno < 0) {
        g_printerr("No I2C bus number available for input switching\n");
        return -1;
    }

    char cmd[256];

    snprintf(cmd, sizeof(cmd), "timeout 5 ddcutil --bus=%d setvcp 0x60 0x%02x >/dev/null 2>&1",
             disp->i2c_busno, input_code);

    DEBUG_PRINT("Executing: %s\n", cmd);

    int rc = system(cmd);
    if (rc == 124) {
        g_printerr("WARNING: Input switch command timed out\n");
        return -1;
    }

    return (rc == 0) ? 0 : -1;
}

int dmi_display_set_vcp_value(dmi_display *disp, guint8 code, guint16 value) {
    if (!disp) return -1;

    if (code == VCP_INPUT || code == 0xAC || code == 0xAA) {
        if (disp->i2c_busno < 0) return -1;

        char cmd[256];
        snprintf(cmd, sizeof(cmd), "timeout 3 ddcutil --bus=%d setvcp 0x%02X %d >/dev/null 2>&1",
                 disp->i2c_busno, code, value);

        int rc = system(cmd);
        if (rc == 124) {
            g_printerr("WARNING: DDC command timed out for VCP 0x%02x\n", code);
            return -1;
        }
        return (rc == 0) ? 0 : -1;
    }

    if (disp->dh) {
        guint8 high = value >> 8;
        guint8 low = value & 0xFF;

        DDCA_Status rc = ddca_set_non_table_vcp_value(disp->dh, code, high, low);

        if (rc == 0) {
            return 0;
        }

        DEBUG_PRINT("Failed to set VCP 0x%02x via handle: %d, trying command line\n", code, rc);
    }

    if (disp->i2c_busno >= 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "timeout 2 ddcutil --bus=%d setvcp 0x%02X %d >/dev/null 2>&1",
                 disp->i2c_busno, code, value);

        int cmd_rc = system(cmd);
        if (cmd_rc == 124) {
            g_printerr("WARNING: DDC command timed out for VCP 0x%02x\n", code);
            return -1;
        }
        return (cmd_rc == 0) ? 0 : -1;
    }

    return -1;
}

GArray *dmi_display_get_supported_inputs(dmi_display *disp) {
    if (!disp || disp->i2c_busno < 0) return NULL;

    GArray *supported = g_array_new(FALSE, FALSE, sizeof(int));

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ddcutil capabilities --bus=%d 2>/dev/null", disp->i2c_busno);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        g_array_free(supported, TRUE);
        return NULL;
    }

    char line[MAX_LINE_LEN];
    gboolean in_feature_60 = FALSE;

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "Feature: 60")) {
            in_feature_60 = TRUE;
            continue;
        }

        if (in_feature_60 && strstr(line, "Feature:")) {
            break;
        }

        if (in_feature_60) {
            int code;
            if (sscanf(line, " %x:", &code) == 1) {
                for (size_t i = 0; i < known_inputs_count; i++) {
                    if (known_inputs[i].code == code) {
                        g_array_append_val(supported, i);
                        DEBUG_PRINT("Display supports input: 0x%02x (%s)\n", code,
                                    known_inputs[i].name);
                        break;
                    }
                }
            }
        }
    }

    pclose(fp);
    return supported;
}

static int get_all_display_bus_numbers(BusInfo **bus_info_array) {
    FILE *fp = popen("ddcutil detect 2>/dev/null", "r");
    if (!fp) return 0;

    BusInfo *info_array = malloc(sizeof(BusInfo) * MAX_DISPLAYS);
    if (!info_array) {
        pclose(fp);
        return 0;
    }

    int count = 0;
    char line[MAX_LINE_LEN];
    int current_bus = -1;
    char current_model[64] = "";
    char current_mfg[32] = "";

    while (fgets(line, sizeof(line), fp) && count < MAX_DISPLAYS) {
        if (strstr(line, "I2C bus:")) {
            char *i2c_pos = strstr(line, "/dev/i2c-");
            if (i2c_pos) {
                sscanf(i2c_pos, "/dev/i2c-%d", &current_bus);
            }
        } else if (strstr(line, "Model:")) {
            char *model_start = strstr(line, "Model:");
            if (model_start) {
                model_start += 6;
                while (*model_start == ' ') model_start++;

                strncpy(current_model, model_start, sizeof(current_model) - 1);
                current_model[sizeof(current_model) - 1] = '\0';

                char *newline = strchr(current_model, '\n');
                if (newline) *newline = '\0';
            }
        } else if (strstr(line, "Mfg id:")) {
            sscanf(line, " Mfg id: %31s", current_mfg);

            if (current_bus != -1 && current_model[0] && current_mfg[0]) {
                strcpy(info_array[count].model_name, current_model);
                strcpy(info_array[count].mfg_id, current_mfg);
                info_array[count].bus_number = current_bus;
                count++;

                current_bus = -1;
                current_model[0] = '\0';
                current_mfg[0] = '\0';
            }
        }
    }

    pclose(fp);
    *bus_info_array = info_array;
    return count;
}

static int find_bus_for_display(BusInfo *bus_info_array, int count, const char *model_name,
                                const char *mfg_id) {
    if (!bus_info_array || !model_name || !mfg_id) return -1;

    for (int i = 0; i < count; i++) {
        if (strcmp(bus_info_array[i].model_name, model_name) == 0 &&
            strcmp(bus_info_array[i].mfg_id, mfg_id) == 0) {
            return bus_info_array[i].bus_number;
        }
    }
    return -1;
}

static void free_bus_info_array(BusInfo *array) {
    free(array);
}

dmi_display_list dmi_display_list_init(gboolean wait) {
    dmi_display_list dlist = {.ct = 0, .list = NULL};

    DDCA_Display_Info_List *dinfos = NULL;
    DDCA_Status rc = ddca_get_display_info_list2(FALSE, &dinfos);
    if (rc != 0 || !dinfos) {
        g_printerr("Failed to get display list: %d\n", rc);
        return dlist;
    }

    BusInfo *bus_info_array = NULL;
    int bus_count = get_all_display_bus_numbers(&bus_info_array);

    DEBUG_PRINT("Found %d displays with bus info\n", bus_count);

    dlist.list = g_array_new(FALSE, FALSE, sizeof(dmi_display *));

    for (guint i = 0; i < dinfos->ct; i++) {
        dmi_display *disp = g_malloc0(sizeof(dmi_display));
        disp->info = dinfos->info[i];

        if (ddca_open_display2(disp->info.dref, wait, &disp->dh) != 0) {
            g_printerr("Failed to open display %s\n", disp->info.model_name);
            g_free(disp);
            continue;
        }

        if (dmi_display_get_brightness(disp) != 0) {
            g_printerr("Failed to get brightness for display %s\n", disp->info.model_name);
            ddca_close_display(disp->dh);
            g_free(disp);
            continue;
        }

        if (dmi_display_get_contrast(disp) != 0) {
            disp->contrast_val = 0;
            disp->contrast_max = 0;
        }

        if (disp->info.path.io_mode == DDCA_IO_I2C) {
            disp->i2c_busno = disp->info.path.path.i2c_busno;
            DEBUG_PRINT("Display %s: I2C bus %d (from path)\n", disp->info.model_name,
                        disp->i2c_busno);
        } else {

            int actual_bus = find_bus_for_display(bus_info_array, bus_count, disp->info.model_name,
                                                  disp->info.mfg_id);

            if (actual_bus != -1) {
                disp->i2c_busno = actual_bus;
                DEBUG_PRINT("Display %s: I2C bus %d (detected)\n", disp->info.model_name,
                            actual_bus);
            } else {
                g_printerr("Warning: Could not determine I2C bus for display %s\n",
                           disp->info.model_name);
                disp->i2c_busno = -1;
            }
        }

        g_array_append_val(dlist.list, disp);
        dlist.ct++;
    }

    g_print("Successfully initialized %d displays\n", dlist.ct);

    free_bus_info_array(bus_info_array);
    ddca_free_display_info_list(dinfos);

    return dlist;
}

void dmi_display_list_free(dmi_display_list *dlist) {
    if (!dlist || !dlist->list) return;

    for (guint i = 0; i < dlist->ct; i++) {
        dmi_display *disp = g_array_index(dlist->list, dmi_display *, i);
        if (disp) {
            if (disp->dh) {
                ddca_close_display(disp->dh);
            }
            g_free(disp);
        }
    }
    g_array_free(dlist->list, TRUE);
    dlist->list = NULL;
    dlist->ct = 0;
}

dmi_display *dmi_display_list_get(dmi_display_list *dlist, guint index) {
    if (!dlist || !dlist->list || index >= dlist->ct) return NULL;
    return g_array_index(dlist->list, dmi_display *, index);
}