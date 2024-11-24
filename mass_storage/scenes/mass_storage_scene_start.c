#include "../mass_storage_app_i.h"

// List of available image sizes with their corresponding size in bytes.
static const struct {
    char* name; // Display name for the image size (e.g., "1.44M").
    uint32_t value; // Size in bytes.
} image_size[] = {
    {"1.44M", 1440 * 1024},
    {"2M", 2 * 1024 * 1024},
    {"4M", 4 * 1024 * 1024},
    {"8M", 8 * 1024 * 1024},
    {"16M", 16 * 1024 * 1024},
    {"32M", 32 * 1024 * 1024},
    {"64M", 64 * 1024 * 1024},
    {"128M", 128 * 1024 * 1024},
    {"256M", 256 * 1024 * 1024},
    {"512M", 512 * 1024 * 1024},
    {"700M", 700 * 1024 * 1024},
    {"1G", 1024 * 1024 * 1024},
    {"2G", 2u * 1024 * 1024 * 1024},
};

// Callback triggered when an item in the variable item list is selected.
static void mass_storage_item_select(void* context, uint32_t index) {
    MassStorageApp* app = context; // Cast the context to the app structure.

    if (index == 0) {
        // If the first item is selected, send a custom event to handle file selection.
        view_dispatcher_send_custom_event(app->view_dispatcher, MassStorageCustomEventFileSelect);
    }
    else {
        // For any other item, send a custom event to create a new image.
        view_dispatcher_send_custom_event(app->view_dispatcher, MassStorageCustomEventNewImage);
    }
}

// Callback to update the image size text and set the selected size in the app's state.
static void mass_storage_image_size(VariableItem* item) {
    MassStorageApp* app = variable_item_get_context(item); // Retrieve app context from the item.
    uint8_t index = variable_item_get_current_value_index(item); // Get the current index.

    // Update the current value text with the selected image size name.
    variable_item_set_current_value_text(item, image_size[index].name);

    // Update the app's state with the selected image size in bytes.
    app->new_file_size = image_size[index].value;
}

// Function called when the "start" scene is entered.
void mass_storage_scene_start_on_enter(void* context) {
    MassStorageApp* app = context; // Cast the context to the app structure.

    // Add an option to select an existing disk image.
    VariableItem* item =
        variable_item_list_add(app->variable_item_list, "Select disk image", 0, NULL, NULL);

    // Add an option to create a new disk image, with a list of sizes to choose from.
    item = variable_item_list_add(
        app->variable_item_list, "New image", COUNT_OF(image_size), mass_storage_image_size, app);

    // Set the callback for when an item is selected in the list.
    variable_item_list_set_enter_callback(app->variable_item_list, mass_storage_item_select, app);

    // Preselect the third size in the list (4MB) and update the text and app state.
    variable_item_set_current_value_index(item, 2);
    variable_item_set_current_value_text(item, image_size[2].name);
    app->new_file_size = image_size[2].value;

    // Switch to the start view in the view dispatcher.
    view_dispatcher_switch_to_view(app->view_dispatcher, MassStorageAppViewStart);
}

/ Function called to handle events in the "start" scene.
bool mass_storage_scene_start_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context); // Mark unused parameters to avoid compiler warnings.
    UNUSED(event);
    MassStorageApp* app = context; // Cast the context to the app structure.

    // Handle custom events for this scene.
    if (event.type == SceneManagerEventTypeCustom) {
        if (event.event == MassStorageCustomEventFileSelect) {
            // Navigate to the file selection scene.
            scene_manager_next_scene(app->scene_manager, MassStorageSceneFileSelect);
        }
        else if (event.event == MassStorageCustomEventNewImage) {
            // Navigate to the "file name" scene for creating a new image.
            scene_manager_next_scene(app->scene_manager, MassStorageSceneFileName);
        }
    }
    return false; // Return false to indicate the event was not consumed by this function.
}

// Function called when the "start" scene is exited.
void mass_storage_scene_start_on_exit(void* context) {
    UNUSED(context); // Mark unused parameters to avoid compiler warnings.
    MassStorageApp* app = context; // Cast the context to the app structure.

    // Reset the variable item list for reuse in other scenes.
    variable_item_list_reset(app->variable_item_list);
}
