#pragma once
#include "tr_level.hpp"
#include "tr_level_data.hpp"
#include "tr_names.hpp"

#include <core/io/config_file.h>

class TRMetadataAnimationInfo : public RefCounted {
	StringName animation_name = "";
};

class TRMetadataStateInfo : public RefCounted {
	StringName state_info = "";
};

class TRMetaDummyBoneInfo {
	Vector3 offset;
	StringName name;
	Quaternion rotation;
};

class TRMetadataBoneInfo : public RefCounted {
	StringName bone_name = "";
	Quaternion custom_rotation = Quaternion();
	bool rotate_x = false;
	bool rotate_y = false;
	bool rotate_z = false;
	Vector<TRMetaDummyBoneInfo> dummy_bones;
};

class TRMetadataObject : public RefCounted {
	String object_name = "";
	bool is_lara = false;
	bool uses_skeleton = false;
	bool uses_motion_scale = false;
	int32_t ponytail_object_id = -1;
	int32_t skinning_object_id = -1;
	Vector<TRMetadataAnimationInfo> animations;
	Vector<TRMetadataStateInfo> states;
	Vector<TRMetadataBoneInfo> bones;
};

class TRMetadataRoom : public RefCounted {
	int32_t overlap_id = 0;
};

class TRMetadataLevel : public RefCounted {
	Vector<TRMetadataObject> objects;
	Vector<TRMetadataRoom> rooms;
};

void LoadLevelMetadata(ConfigFile p_metadata_file) {
}