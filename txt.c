// -- api's
static struct tm_api_registry_api *tm_global_api_registry;
static struct tm_the_truth_api *tm_the_truth_api;
static struct tm_properties_view_api *tm_properties_view_api;
static struct tm_os_api *tm_os_api;
static struct tm_path_api *tm_path_api;
static struct tm_temp_allocator_api *tm_temp_allocator_api;
static struct tm_logger_api *tm_logger_api;
static struct tm_localizer_api *tm_localizer_api;
static struct tm_asset_io_api *tm_asset_io_api;
static struct tm_task_system_api *task_system;
static struct tm_allocator_api *tm_allocator_api;

// -- inlcudes
#include <foundation/api_registry.h>
#include <foundation/asset_io.h>
#include <foundation/buffer.h>
#include <foundation/carray_print.inl>
#include <foundation/localizer.h>
#include <foundation/log.h>
#include <foundation/macros.h>
#include <foundation/os.h>
#include <foundation/path.h>
#include <foundation/string.inl>
#include <foundation/task_system.h>
#include <foundation/temp_allocator.h>
#include <foundation/the_truth.h>
#include <foundation/the_truth_assets.h>
#include <foundation/undo.h>

#include <plugins/editor_views/asset_browser.h>
#include <plugins/editor_views/properties.h>

#include "txt.h"

// -- struct definitions

struct task__import_txt
{
    uint64_t bytes;
    struct tm_asset_io_import args;
    char file[8];
};
/////
// -- functions:
////

// --- importer

static void task__import_txt(void *data, uint64_t task_id)
{
    struct task__import_txt *task = (struct task__import_txt *)data;
    const struct tm_asset_io_import *args = &task->args;
    const char *txt_file = task->file;
    tm_the_truth_o *tt = args->tt;

    tm_file_stat_t stat = tm_os_api->file_system->stat(txt_file);
    if (stat.exists) {
        tm_buffers_i *buffers = tm_the_truth_api->buffers(tt);

        void *buffer = buffers->allocate(buffers->inst, stat.size, false);
        tm_file_o f = tm_os_api->file_io->open_input(txt_file);
        const int64_t read = tm_os_api->file_io->read(f, buffer, stat.size);
        tm_os_api->file_io->close(f);

        if (read == (int64_t)stat.size) {
            const uint32_t buffer_id = buffers->add(buffers->inst, buffer, stat.size, 0);

            const uint64_t plugin_asset_type = tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__MY_ASSET);
            const tm_tt_id_t asset_id = tm_the_truth_api->create_object_of_type(tt, plugin_asset_type, TM_TT_NO_UNDO_SCOPE);
            tm_the_truth_object_o *asset_obj = tm_the_truth_api->write(tt, asset_id);
            tm_the_truth_api->set_buffer(tt, asset_obj, 1, buffer_id);
            tm_the_truth_api->set_string(tt, asset_obj, 0, txt_file);

            if (args->reimport_into.u64) {
                tm_the_truth_api->retarget_write(tt, asset_obj, args->reimport_into);
                tm_the_truth_api->commit(tt, asset_obj, args->undo_scope);
                tm_the_truth_api->destroy_object(tt, asset_id, args->undo_scope);
            } else {
                tm_the_truth_api->commit(tt, asset_obj, args->undo_scope);
                TM_INIT_TEMP_ALLOCATOR(ta);
                const char *ext;
                const char *name = tm_path_api->split(txt_file, &ext);
                const char *asset_name = tm_temp_allocator_api->printf(ta, "%.*s", ext - name, name);
                tm_asset_browser_add_asset_api *add_asset = tm_global_api_registry->get(TM_ASSET_BROWSER_ADD_ASSET_API_NAME);
                const tm_tt_id_t current_dir = add_asset->current_directory(add_asset->inst, args->ui);
                const bool should_select = args->asset_browser.u64 && tm_the_truth_api->version(tt, args->asset_browser) == args->asset_browser_version_at_start;
                add_asset->add(add_asset->inst, current_dir, asset_id, asset_name, args->undo_scope, should_select, args->ui);
                TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
            }
        } else {
            tm_logger_api->printf(TM_LOG_TYPE_INFO, "import txt:cound not read %s\n", txt_file);
        }
    } else {
        tm_logger_api->printf(TM_LOG_TYPE_INFO, "import txt:cound not find %s \n", txt_file);
    }
    tm_free(args->allocator, task, task->bytes);
}

static bool asset_io__enabled(struct tm_asset_io_o *inst)
{
    return true;
}

static bool asset_io__can_import(struct tm_asset_io_o *inst, const char *extension)
{
    return tm_strcmp_ignore_case(extension, "txt") == 0;
}

static bool asset_io__can_reimport(struct tm_asset_io_o *inst, struct tm_the_truth_o *tt, tm_tt_id_t asset)
{
    const tm_tt_id_t object = tm_the_truth_api->get_subobject(tt, tm_tt_read(tt, asset), TM_TT_PROP__ASSET__OBJECT);
    return object.type == tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__MY_ASSET);
}
static void asset_io__importer_extensions_string(struct tm_asset_io_o *inst, char **output, struct tm_temp_allocator_i *ta, const char *separator)
{
    tm_carray_temp_printf(output, ta, "txt");
}

static void asset_io__importer_description_string(struct tm_asset_io_o *inst, char **output, struct tm_temp_allocator_i *ta, const char *separator)
{
    tm_carray_temp_printf(output, ta, ".txt");
}
static uint64_t asset_io__import_asset(struct tm_asset_io_o *inst, const char *file, const struct tm_asset_io_import *args)
{
    const uint64_t bytes = sizeof(struct task__import_txt) + strlen(file);
    struct task__import_txt *task = tm_alloc(args->allocator, bytes);
    *task = (struct task__import_txt) {
        .bytes = bytes,
        .args = *args,
    };
    strcpy(task->file, file);
    return task_system->run_task(task__import_txt, task, "Import Text File");
}
static struct tm_asset_io_i txt_asset_io = {
    .enabled = asset_io__enabled,
    .can_import = asset_io__can_import,
    .can_reimport = asset_io__can_reimport,
    .importer_extensions_string = asset_io__importer_extensions_string,
    .importer_description_string = asset_io__importer_description_string,
    .import_asset = asset_io__import_asset
};

//custom ui
static float properties__custom_ui(struct tm_properties_ui_args_t *args, tm_rect_t item_rect, tm_tt_id_t object, uint32_t indent)
{
    tm_the_truth_o *tt = args->tt;
    bool picked = false;
    item_rect.y = tm_properties_view_api->ui_open_path(args, item_rect, TM_LOCALIZE_LATER("Import Path"), TM_LOCALIZE_LATER("Path that the text file was imported from."), object, 0, "txt", "text files", &picked);
    if (picked) {
        const char *file = tm_the_truth_api->get_string(tt, tm_tt_read(tt, object), 0);

        {
            tm_allocator_i *allocator = tm_allocator_api->system;
            const uint64_t bytes = sizeof(struct task__import_txt) + strlen(file);
            struct task__import_txt *task = tm_alloc(allocator, bytes);
            *task = (struct task__import_txt) {
                .bytes = bytes,
                .args = {
                    .allocator = allocator,
                    .tt = tt,
                    .reimport_into = object }
            };
            strcpy(task->file, file);
            task_system->run_task(task__import_txt, task, "Import Text File");
        }
    }
    return item_rect.y;
}

// -- create truth type
static void create_truth_types(struct tm_the_truth_o *tt)
{
    static tm_the_truth_property_definition_t my_asset_properties[] = {
        { "import_path", TM_THE_TRUTH_PROPERTY_TYPE_STRING },
        { "data", TM_THE_TRUTH_PROPERTY_TYPE_BUFFER },
    };
    const uint64_t type = tm_the_truth_api->create_object_type(tt, TM_TT_TYPE__MY_ASSET, my_asset_properties, TM_ARRAY_COUNT(my_asset_properties));
    tm_the_truth_api->set_aspect(tt, type, TM_TT_ASPECT__FILE_EXTENSION, "txt");
    static tm_properties_aspect_i properties_aspect = {
        .custom_ui = properties__custom_ui,
    };
    tm_the_truth_api->set_aspect(tt, type, TM_TT_ASPECT__PROPERTIES, &properties_aspect);
}
// -- asset browser regsiter interface
static tm_tt_id_t asset_browser_create(struct tm_asset_browser_create_asset_o *inst, tm_the_truth_o *tt, tm_tt_undo_scope_t undo_scope)
{
    const uint64_t type = tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__MY_ASSET);
    return tm_the_truth_api->create_object_of_type(tt, type, undo_scope);
}

static tm_asset_browser_create_asset_i asset_browser_create_my_asset = {
    .menu_name = TM_LOCALIZE_LATER("New Text File"),
    .asset_name = TM_LOCALIZE_LATER("New Text File"),
    .create = asset_browser_create,
};

// -- load plugin
TM_DLL_EXPORT void tm_load_plugin(struct tm_api_registry_api *reg, bool load)
{
    tm_the_truth_api = reg->get(TM_THE_TRUTH_API_NAME);
    tm_properties_view_api = reg->get(TM_PROPERTIES_VIEW_API_NAME);
    tm_os_api = reg->get(TM_OS_API_NAME);
    tm_path_api = reg->get(TM_PATH_API_NAME);
    tm_temp_allocator_api = reg->get(TM_TEMP_ALLOCATOR_API_NAME);
    tm_allocator_api = reg->get(TM_ALLOCATOR_API_NAME);
    tm_logger_api = reg->get(TM_LOGGER_API_NAME);
    tm_localizer_api = reg->get(TM_LOCALIZER_API_NAME);
    tm_asset_io_api = reg->get(TM_ASSET_IO_API_NAME);
    task_system = reg->get(TM_TASK_SYSTEM_API_NAME);
    tm_global_api_registry = reg;

    if (load)
        tm_asset_io_api->add_asset_io(&txt_asset_io);
    else
        tm_asset_io_api->remove_asset_io(&txt_asset_io);

    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_CREATE_TYPES_INTERFACE_NAME, create_truth_types);
    tm_add_or_remove_implementation(reg, load, TM_ASSET_BROWSER_CREATE_ASSET_INTERFACE_NAME, &asset_browser_create_my_asset);
}