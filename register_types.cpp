#include "register_types.h"
#include <editor/editor_node.h>
#include "tr_level_importer.hpp"

#ifdef IS_MODULE
void initialize_tr_lib_module(ModuleInitializationLevel p_level) {
#else
void gdextension_initialize(ModuleInitializationLevel p_level) {
#endif
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		ClassDB::register_class<TRLevel>();
		ClassDB::register_class<TRLevelData>();

		EditorPlugins::add_by_type<TRLevelEditorPlugin>();
	}
}

#ifdef IS_MODULE
void uninitialize_tr_lib_module(ModuleInitializationLevel p_level) {
#else
void gdextension_terminate(ModuleInitializationLevel p_level) {
#endif
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
	}
}

#ifndef IS_MODULE
extern "C"
{
	GDNativeBool GDN_EXPORT gdextension_init(const GDNativeInterface *p_interface, const GDNativeExtensionClassLibraryPtr p_library, GDNativeInitialization *r_initialization) {
		godot::GDExtensionBinding::InitObject init_obj(p_interface, p_library, r_initialization);

		init_obj.register_initializer(gdextension_initialize);
		init_obj.register_terminator(gdextension_terminate);
		init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

		return init_obj.init();
	}
}
#endif
