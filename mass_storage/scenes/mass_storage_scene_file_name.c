#include "../mass_storage_app_i.h"

#define WRITE_BUF_LEN 4096 // Buffer size for writing data (4KB)

// Callback triggered when the user submits the file name.
static void mass_storage_file_name_text_callback(void* context) {
    furi_assert(context); // Ensure the context is valid.

    MassStorageApp* app = context; // Cast the context to the app structure.
    // Send a custom even to the vuew duspatcher to handle the input event.
    view_dispatcher_send_custom_event(app->view_dispatcher, MassStorageCustomEventNameInput);  
}

// Function to create a new image file with a specified size in storage.
static bool mass_storage_create_image(Storage* storage, const char* file_path, uint32_t size) {
    FURI_LOG_I("TAG", "Creating image %s, len:%lu", file_path, size); // Log the operation.
    File* file = storage_file_alloc(storage); // Allocate a file object.

    bool success = false; // Indicates if the operation succeeds.
    uint8_t* buffer = malloc(WRITE_BUF_LEN); // Allocatea buffer for writing data.
    
    do {
        // Open or create the file for writing. If it fails, exit the loop.
        if(!storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) break;

        // Seek to the specified size to preallocate the file. If it fails, exit the look.
        if(!storage_file_seek(file, size, true)) break;

        // Reset the file pointer to the beginning for subsequent operations.
        if(!storage_file_seek(file, 0, true)) break;

        // Zero out first 4B of the file - partition table and adjacent data (metadata)
        if(!storage_file_write(file, buffer, WRITE_BUF_LEN)) break;

        success = true; // If all operations succeed, mark the opearation as successful.
    } while(false); // Single iteration loop for structured error handling.

    free(buffer); // Free the allocatied buffer.
    storage_file_close(file); // close the file.
    storage_file_free(file); // Free the file object
    return success; // Return wether the opeartion succeeded.
}

// Function called when the "file name" scene is entered.
void mass_storage_scene_file_name_on_enter(void* context) {
    MassStorageApp* app = context; // Cast the context tot the app structure.

    // Set up the header text for the text input view.
    text_input_set_header_text(app->text_input, "Enter image name");

    // Create and configure a fuke anem validator.
    ValidatorIsFile* validator_is_file =
        validator_is_file_alloc_init(MASS_STORAGE_APP_PATH_FOLDER, MASS_STORAGE_APP_EXTENSION, "");

    // Set the validator for the text input field.
    text_input_set_validator(app->text_input, validator_is_file_callback, validator_is_file);

    // Set the result callback for handling the input submission.
    text_input_set_result_callback(
        app->text_input,
        mass_storage_file_name_text_callback, // Callback to execute when the input is 
        app,                                  // Pass the app as context.
        app->new_file_name,                   // Pointer to the buffer for the file name.
        MASS_STORAGE_FILE_NAME_LEN,           // Maximum length of the file name.
        true);                                // Allow overwriting of existing text.

    // Switch to the text input view in the view dispatcher.
    view_dispatcher_switch_to_view(app->view_dispatcher, MassStorageAppViewTextInput);
}


// Function called to handle events in the "file name" scene.
bool mass_storage_scene_file_name_on_event(void* context, SceneManagerEvent event) {
    UNUSED(event); // Mark unused parameters to avoid compiler warnings.
    MassStorageApp* app = context; // Cast the context to the app structure.

    bool consumed = false; // Indicates if the event was handled.

    // Handle custom events.
    if (event.type == SceneManagerEventTypeCustom) {
        if (event.event == MassStorageCustomEventNameInput) { // Check for "name input" event.
            mass_storage_app_show_loading_popup(app, true); // Show a loading popup.

            // Format the full file path: "<folder>/<file_name><extension>".
            furi_string_printf(
                app->file_path,
                "%s/%s%s",
                MASS_STORAGE_APP_PATH_FOLDER, // Base folder path.
                app->new_file_name,           // Entered file name.
                MASS_STORAGE_APP_EXTENSION);  // File extension.

            // Try to create the image file.
            if (mass_storage_create_image(
                app->fs_api, furi_string_get_cstr(app->file_path), app->new_file_size)) {
                // Check if USB is unlocked. If it is, proceed to the "work" scene.
                if (!furi_hal_usb_is_locked()) {
                    scene_manager_next_scene(app->scene_manager, MassStorageSceneWork);
                }
                else {
                    // Otherwise, proceed to the "USB locked" scene.
                    scene_manager_next_scene(app->scene_manager, MassStorageSceneUsbLocked);
                }
            }
            // TODO: Add error handling for failures (e.g., display an error message).
        }
    }
    return consumed; // Return whether the event was handled.
}

// Function called when the "file name" scene is exited.
void mass_storage_scene_file_name_on_exit(void* context) {
    UNUSED(context); // Mark unused parameters to avoid compiler warnings.
    MassStorageApp* app = context; // Cast the context to the app structure.

    // Free the validator context used by the text input.
    void* validator_context = text_input_get_validator_callback_context(app->text_input);
    text_input_set_validator(app->text_input, NULL, NULL); // Remove the validator.
    validator_is_file_free(validator_context); // Free the validator's memory.

    // Reset the text input for reuse.
    text_input_reset(app->text_input);
}
