#include "../mass_storage_app_i.h"
#include "../views/mass_storage_view.h"
#include "../helpers/mass_storage_usb.h"
#include <lib/toolbox/path.h>

#define TAG "MassStorageSceneWork" // Tag used for logging.

// Function to handle reading from the file in response to USB mass storage read requests.
static bool file_read(
    void* ctx,             // Context passed to the callback (MassStorageApp instance).
    uint32_t lba,          // Logical Block Address (block index to read from).
    uint16_t count,        // Number of blocks to read.
    uint8_t* out,          // Output buffer to store the read data.
    uint32_t* out_len,     // Pointer to store the actual number of bytes read.
    uint32_t out_cap) {    // Capacity of the output buffer.
    MassStorageApp* app = ctx;

    FURI_LOG_T(TAG, "file_read lba=%08lX count=%04X out_cap=%08lX", lba, count, out_cap);

    // Seek to the position in the file corresponding to the LBA.
    if (!storage_file_seek(app->file, lba * SCSI_BLOCK_SIZE, true)) {
        FURI_LOG_W(TAG, "seek failed");
        return false; // Return false if seeking fails.
    }

    // Ensure the requested data fits within the output buffer.
    uint16_t clamp = MIN(out_cap, count * SCSI_BLOCK_SIZE);
    *out_len = storage_file_read(app->file, out, clamp); // Read the requested data.
    FURI_LOG_T(TAG, "%lu/%lu", *out_len, count * SCSI_BLOCK_SIZE);

    // Update the total bytes read.
    app->bytes_read += *out_len;

    return *out_len == clamp; // Return true if all requested bytes were read successfully.
}

// Function to handle writing to the file in response to USB mass storage write requests.
static bool file_write(
    void* ctx,        // Context passed to the callback (MassStorageApp instance).
    uint32_t lba,     // Logical Block Address (block index to write to).
    uint16_t count,   // Number of blocks to write.
    uint8_t* buf,     // Buffer containing the data to be written.
    uint32_t len) {   // Length of the data in the buffer.
    MassStorageApp* app = ctx;

    FURI_LOG_T(TAG, "file_write lba=%08lX count=%04X len=%08lX", lba, count, len);

    // Ensure the length of the data matches the number of blocks requested.
    if (len != count * SCSI_BLOCK_SIZE) {
        FURI_LOG_W(TAG, "bad write params count=%u len=%lu", count, len);
        return false; // Return false if parameters are invalid.
    }

    // Seek to the position in the file corresponding to the LBA.
    if (!storage_file_seek(app->file, lba * SCSI_BLOCK_SIZE, true)) {
        FURI_LOG_W(TAG, "seek failed");
        return false; // Return false if seeking fails.
    }

    // Update the total bytes written.
    app->bytes_written += len;

    // Write the data to the file and return whether the write was successful.
    return storage_file_write(app->file, buf, len) == len;
}

// Function to calculate the total number of blocks in the file.
static uint32_t file_num_blocks(void* ctx) {
    MassStorageApp* app = ctx;
    return storage_file_size(app->file) / SCSI_BLOCK_SIZE; // Return file size in blocks.
}

// Function to handle the USB mass storage eject operation.
static void file_eject(void* ctx) {
    MassStorageApp* app = ctx;
    FURI_LOG_D(TAG, "EJECT");
    // Send a custom event to handle the eject operation.
    view_dispatcher_send_custom_event(app->view_dispatcher, MassStorageCustomEventEject);
}

// Event handler for the "work" scene.
bool mass_storage_scene_work_on_event(void* context, SceneManagerEvent event) {
    MassStorageApp* app = context;
    bool consumed = false;

    if (event.type == SceneManagerEventTypeCustom) {
        // Handle custom events.
        if (event.event == MassStorageCustomEventEject) {
            // Switch to the previous scene when an eject event is received.
            consumed = scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MassStorageSceneFileSelect);
            if (!consumed) {
                consumed = scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, MassStorageSceneStart);
            }
        }
    }
    else if (event.type == SceneManagerEventTypeTick) {
        // Periodically update the statistics in the view.
        mass_storage_set_stats(app->mass_storage_view, app->bytes_read, app->bytes_written);
    }
    else if (event.type == SceneManagerEventTypeBack) {
        // Handle the "Back" button press by switching to the previous scene.
        consumed = scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, MassStorageSceneFileSelect);
        if (!consumed) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MassStorageSceneStart);
        }
    }

    return consumed; // Return whether the event was consumed.
}

// Function called when entering the "work" scene.
void mass_storage_scene_work_on_enter(void* context) {
    MassStorageApp* app = context;

    app->bytes_read = app->bytes_written = 0; // Reset read and write counters.

    // Check if the selected file exists. If not, return to the start scene.
    if (!storage_file_exists(app->fs_api, furi_string_get_cstr(app->file_path))) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, MassStorageSceneStart);
        return;
    }

    mass_storage_app_show_loading_popup(app, true); // Show a loading popup.

    app->usb_mutex = furi_mutex_alloc(FuriMutexTypeNormal); // Allocate a mutex for USB operations.

    // Extract the file name from the file path.
    FuriString* file_name = furi_string_alloc();
    path_extract_filename(app->file_path, file_name, true);

    // Set the file name in the view.
    mass_storage_set_file_name(app->mass_storage_view, file_name);

    // Open the selected file for reading and writing.
    app->file = storage_file_alloc(app->fs_api);
    furi_assert(storage_file_open(
        app->file,
        furi_string_get_cstr(app->file_path),
        FSAM_READ | FSAM_WRITE,
        FSOM_OPEN_EXISTING));

    // Configure USB mass storage device callbacks.
    SCSIDeviceFunc fn = {
        .ctx = app,
        .read = file_read,
        .write = file_write,
        .num_blocks = file_num_blocks,
        .eject = file_eject,
    };

    // Start the USB mass storage service with the specified file and callbacks.
    app->usb = mass_storage_usb_start(furi_string_get_cstr(file_name), fn);

    furi_string_free(file_name); // Free the allocated file name string.

    mass_storage_app_show_loading_popup(app, false); // Hide the loading popup.
    view_dispatcher_switch_to_view(app->view_dispatcher, MassStorageAppViewWork); // Switch to the "work" view.
}

// Function called when exiting the "work" scene.
void mass_storage_scene_work_on_exit(void* context) {
    MassStorageApp* app = context;

    mass_storage_app_show_loading_popup(app, true); // Show a loading popup.

    // Free resources allocated for USB operations and file access.
    if (app->usb_mutex) {
        furi_mutex_free(app->usb_mutex);
        app->usb_mutex = NULL;
    }
    if (app->usb) {
        mass_storage_usb_stop(app->usb);
        app->usb = NULL;
    }
    if (app->file) {
        storage_file_free(app->file);
        app->file = NULL;
    }

    mass_storage_app_show_loading_popup(app, false); // Hide the loading popup.
}
