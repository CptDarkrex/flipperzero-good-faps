#include "mass_storage_view.h"
#include "../mass_storage_app_i.h"
#include <gui/elements.h>

// Sturecture representing the mass storage app's view.
struct MassStorage {
    View* view; // The view object that handles rendering and user interaction
};

// Model to store the app's data (state).
typedef struct {
    FuriString *file_name, *status_string; // String for file name and status display.
    uint32_t read_speed, write_speed; // Speed of read amd wrote operations in bytes per second.
    uint32_t bytes_read, bytes_written; // Total bytes read and written.
    uint32_t update_time; // Last update time (in system ticks).
} MassStorageModel;

// Helper function to format a byte count with  a unit suffix (e.g., KB, MB)
static void append_suffixed_byte_count(FuriString* string, uint32_t count) {
    if(count < 1024) {
        furi_string_cat_printf(string, "%luB", count);// Bytes
    } else if(count < 1024 * 1024) {
        furi_string_cat_printf(string, "%luK", count / 1024); // Kilobytes
    } else if(count < 1024 * 1024 * 1024) {
        furi_string_cat_printf(string, "%.3fM", (double)count / (1024 * 1024)); // Megabytes
    } else {
        furi_string_cat_printf(string, "%.3fG", (double)count / (1024 * 1024 * 1024)); // Gigabytes
    }
}

// Callback function to draw the view onto the canvas.
static void mass_storage_draw_callback(Canvas* canvas, void* _model) {
    MassStorageModel* model = _model; // Cast the model context.

    // Draw an icon for the USB drive
    canvas_draw_icon(canvas, 8, 14, &I_Drive_112x35);

    // Draw the title text "USB Mass Storage".
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, canvas_width(canvas) / 2, 0, AlignCenter, AlignTop, "USB Mass Storage");

    // Draw the file name in a centered position.
    canvas_set_font(canvas, FontSecondary);
    elements_string_fit_width(canvas, model->file_name, 89 - 2); // Fit the string within width.
    canvas_draw_str_aligned(
        canvas, 50, 23, AlignCenter, AlignBottom, furi_string_get_cstr(model->file_name));

    // Prepare and draw the read stats
    furi_string_set_str(model->status_string, "R:"); // Prefix "R:" for read stats 
    append_suffixed_byte_count(model->status_string, model->bytes_read); // add bytes read
    if(model->read_speed) {
        furi_string_cat_str(model->status_string, "; "); // Separator
        append_suffixed_byte_count(model->status_string, model->read_speed); // add read speed
        furi_string_cat_str(model->status_string, "ps"); // add suffix per second
    }
    canvas_draw_str(canvas, 12, 34, furi_string_get_cstr(model->status_string));

    // Prepare and draw the write stats
    furi_string_set_str(model->status_string, "W:"); // Prefix "W:" for write stats
    append_suffixed_byte_count(model->status_string, model->bytes_written); // add bytes written
    if(model->write_speed) {
        furi_string_cat_str(model->status_string, "; "); // Separator
        append_suffixed_byte_count(model->status_string, model->write_speed); // add write speed
        furi_string_cat_str(model->status_string, "ps"); // suffix per second
    }
    canvas_draw_str(canvas, 12, 44, furi_string_get_cstr(model->status_string));
}

// Function to allocate and initialize the mass storage view
MassStorage* mass_storage_alloc() {
    MassStorage* mass_storage = malloc(sizeof(MassStorage)); // allocate memory

    mass_storage->view = view_alloc(); // allocate or store new view
    view_allocate_model(mass_storage->view, ViewModelTypeLocking, sizeof(MassStorageModel)); // allocate model for the view
    with_view_model(
        mass_storage->view,
        MassStorageModel * model,
        {
            model->file_name = furi_string_alloc(); // initialize the name string.
            model->status_string = furi_string_alloc(); // initialize status string.
        },
        false);
    view_set_context(mass_storage->view, mass_storage); // set the view context 
    view_set_draw_callback(mass_storage->view, mass_storage_draw_callback); // set draw callback

    return mass_storage; // return the allocated view
}

// Function to free resources associated with the Mass Storage View.
void mass_storage_free(MassStorage* mass_storage) {
    furi_assert(mass_storage); // Ensure the view is valid.
    with_view_model(
        mass_storage->view,
        MassStorageModel * model,
        {
            furi_string_free(model->file_name); // Free file name string.
            furi_string_free(model->status_string); // Free status string.
        },
        false);
    view_free(mass_storage->view); // Free the view.
    free(mass_storage); // Free the main structure.
}

// Function to retrieve the view from the Mass Storage structure.
View* mass_storage_get_view(MassStorage* mass_storage) {
    furi_assert(mass_storage);
    return mass_storage->view; // Return the view.
}

// Function to set the file name displayed in the view.
void mass_storage_set_file_name(MassStorage* mass_storage, FuriString* name) {
    furi_assert(name);
    with_view_model(
        mass_storage->view,
        MassStorageModel * model,
        { furi_string_set(model->file_name, name); },
        true);
}

// Function to update statistics (bytes read. written and speeds).
void mass_storage_set_stats(MassStorage* mass_storage, uint32_t read, uint32_t written) {
    with_view_model(
        mass_storage->view,
        MassStorageModel * model,
        {
            uint32_t now = furi_get_tick(); // get the current time in ticks.
            model->read_speed = (read - model->bytes_read) * 1000 / (now - model->update_time); // calculate read speed.
            model->write_speed =
                (written - model->bytes_written) * 1000 / (now - model->update_time); // calculate wrute soeed.
            model->bytes_read = read; // update bytes read.
            model->bytes_written = written; // Update bytes written.
            model->update_time = now; // Update time.
        },
        true);
}
