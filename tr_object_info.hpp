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

class TRObjectAnimationInfo {
	StringName animation_name = "";
};

class TRObjectStateInfo {
	StringName state_name = "";
};

class TRDummyBoneInfo {
	Vector3 offset;
	StringName name;
	Quaternion rotation;
};

class TRObjectBoneInfo {
	StringName bone_name = "";
	Quaternion custom_rotation = Quaternion();
	bool rotate_x = false;
	bool rotate_y = false;
	bool rotate_z = false;
	Vector<TRDummyBoneInfo> dummy_bones;
};

class TRObjectInfo {
	String object_name = "";
	bool is_lara = false;
	bool uses_skeleton = false;
	bool uses_motion_scale = false;
	int32_t ponytail_object_id = -1;
	int32_t skinning_object_id = -1;
	Vector<TRObjectAnimationInfo> animations;
	Vector<TRObjectStateInfo> states;
	Vector<TRObjectBoneInfo> bones;
};