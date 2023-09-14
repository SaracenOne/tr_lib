#pragma once

#include "tr_module_extension_abstraction_layer.hpp"


#ifdef IS_MODULE
#include "tr_level.hpp"
#include "editor/import/editor_import_plugin.h"
#include "scene/3d/node_3d.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#else 
using namespace godot;
#include <godot_cpp/classes/node3D.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>
#endif

class TRLevelImporter : public EditorImportPlugin {
protected:
	static void _bind_methods();
public:
	TRLevelImporter() {};
	~TRLevelImporter() {};

	String _get_importer_name() const {
		return "trl_importer";
	}

	int get_import_order() const {
		return 0;
	}

	String get_visible_name() const {
		return "Tomb Raider Level";
	}

	void get_recognized_extensions(List<String>* p_extensions) const {
		p_extensions->push_back("phd");
	}

	String get_save_extension() const {
		return "scn";
	}

	String get_resource_type() const {
		return "PackedScene";
	}

	int get_preset_count() const {
		return 1;
	}

	String get_preset_name(int p_idx) {
		return "Default";
	}

	void _get_import_options(const String& p_path, List<ResourceImporter::ImportOption>* r_options, int p_preset) const {
		return;
	}

	float get_priority() const {
		return 1.0;
	}

	Error import(const String& p_source_file, const String& p_save_path, const HashMap<StringName, Variant>& p_options, List<String>* r_platform_variants, List<String>* r_gen_files, Variant* r_metadata) {
		TRLevel *level = memnew(TRLevel);

		level->set_level_path(p_source_file);
		level->load_level();

		return FAILED;
	}
};
