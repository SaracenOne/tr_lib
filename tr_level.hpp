#pragma once

#include "tr_module_extension_abstraction_layer.hpp"

#include "tr_file_parser.hpp"

#ifdef IS_MODULE
#include "scene/3d/node_3d.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#else 
using namespace godot;
#include <godot_cpp/classes/node3D.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>
#endif

enum TRLevelFormat {
	TR_VERSION_UNKNOWN,
	TR1_PC,
	TR2_PC,
	TR3_PC,
	TR4_PC,
};

#include "tr_names.hpp"
#include "tr_misc.hpp"
#include "tr_types.h"

const float TR_FPS = 30.0f;

class TRLevel : public Node3D {
	GDCLASS(TRLevel, Node3D);
protected:
	String level_path;

	static void _bind_methods();
public:
	TRLevel();
	~TRLevel();

	void _notification(int p_what);

	String get_level_path() { return level_path; }
	void set_level_path(String p_level_path) { level_path = p_level_path; }

	void clear_level();
	void load_level(bool p_lara_only);
	TRLevelData load_level_type(String file_path);
};