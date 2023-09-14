#include "tr_module_extension_abstraction_layer.hpp"

#ifdef IS_MODULE
#include "modules/register_module_types.h"
#include "core/object/class_db.h"
#else 
using namespace godot;
#include <godot/gdnative_interface.h>
#include <godot_cpp/core/class_db.hpp>
#endif

#include "tr_level.hpp"
#include "editor/tr_level_editor_plugin.hpp"

#ifdef IS_MODULE
void initialize_tr_lib_module(ModuleInitializationLevel p_level);
void uninitialize_tr_lib_module(ModuleInitializationLevel p_level);
#else
using namespace godot;

void gdextension_initialize(ModuleInitializationLevel p_level);
void gdextension_terminate(ModuleInitializationLevel p_level);
#endif