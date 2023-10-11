#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <dialogs/dialogs.h>
#include "imu.h"

#define TAG "SensorModule"

#define ACCEL_GYRO_RAW_RATE DataRate200Hz

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* input_queue;

    FuriHalSpiBusHandle* icm42688p_device;
    ICM42688P* icm42688p;
    bool icm42688p_valid;

    ImuThread* imu_thread;
} SensorModuleApp;

static void render_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 12, "Motion Mouse");
}

static void input_callback(InputEvent* input_event, void* ctx) {
    SensorModuleApp* app = ctx;
    if(input_event->type == InputTypeShort) {
        furi_message_queue_put(app->input_queue, input_event, 0);
    }
}

static SensorModuleApp* sensor_module_alloc(void) {
    SensorModuleApp* app = malloc(sizeof(SensorModuleApp));

    app->icm42688p_device = malloc(sizeof(FuriHalSpiBusHandle));
    memcpy(app->icm42688p_device, &furi_hal_spi_bus_handle_external, sizeof(FuriHalSpiBusHandle));
    app->icm42688p_device->cs = &gpio_ext_pc3;
    app->icm42688p = icm42688p_alloc(app->icm42688p_device, &gpio_ext_pb2);
    app->icm42688p_valid = icm42688p_init(app->icm42688p);
    if(app->icm42688p_valid) {
        icm42688p_accel_config(app->icm42688p, AccelFullScale16G, ACCEL_GYRO_RAW_RATE);
        icm42688p_gyro_config(app->icm42688p, GyroFullScale2000DPS, ACCEL_GYRO_RAW_RATE);
    }

    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    return app;
}

static void sensor_module_free(SensorModuleApp* app) {
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);

    furi_message_queue_free(app->input_queue);

    if(app->imu_thread) {
        imu_stop(app->imu_thread);
        app->imu_thread = NULL;
    }

    if(!icm42688p_deinit(app->icm42688p)) {
        FURI_LOG_E(TAG, "Failed to deinitialize ICM42688P");
    }

    icm42688p_free(app->icm42688p);
    free(app->icm42688p_device);

    free(app);
}

int32_t motion_mouse_app(void* arg) {
    UNUSED(arg);
    SensorModuleApp* app = sensor_module_alloc();

    if(!app->icm42688p_valid) {
        DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
        DialogMessage* message = dialog_message_alloc();
        dialog_message_set_header(message, "Sensor Module error", 63, 0, AlignCenter, AlignTop);

        dialog_message_set_text(message, "Module not conntected", 63, 30, AlignCenter, AlignTop);
        dialog_message_show(dialogs, message);
        dialog_message_free(message);
        furi_record_close(RECORD_DIALOGS);

        sensor_module_free(app);
        return 0;
    }

    view_port_update(app->view_port);
    app->imu_thread = imu_start(app->icm42688p);

    while(1) {
        InputEvent input;
        if(furi_message_queue_get(app->input_queue, &input, FuriWaitForever) == FuriStatusOk) {
            if(input.key == InputKeyBack) {
                break;
            }
        }
    }

    sensor_module_free(app);
    return 0;
}