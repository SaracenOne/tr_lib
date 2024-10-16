#pragma once
#include "tr_level.hpp"

struct DummyBoneInfo {
	Vector3 offset;
	StringName name;
	Quaternion rotation;
};

static bool is_lara_compatible_humanoid(uint32_t p_type_info_id, TRLevelFormat p_level_format) {
	switch (p_level_format) {
	case TR1_PC:
		if ((p_type_info_id >= 0 && p_type_info_id <= 6))
			return true;
		break;
	case TR2_PC:
		if (p_type_info_id == 0 || p_type_info_id == 1 || (p_type_info_id >= 3 && p_type_info_id <= 12)) {
			return true;
		}
		break;
	case TR3_PC:
		if (p_type_info_id == 0 || p_type_info_id == 1 || (p_type_info_id >= 3 && p_type_info_id <= 13) || p_type_info_id == 315) {
			return true;
		}
		break;
	case TR4_PC:
		if ((p_type_info_id >= 0 && p_type_info_id <= 29) || p_type_info_id == 33) {
			return true;
		}
		break;
	}

	return false;
}

static bool does_type_use_motion_scale(uint32_t p_type_info_id, TRLevelFormat p_level_format) {
	if (is_lara_compatible_humanoid(p_type_info_id, p_level_format)) {
		return true;
	}

	return false;
}

static bool does_have_dummy_bone(uint32_t p_type_info_id, uint32_t p_mesh_id, TRLevelFormat p_level_format) {
	if (is_lara_compatible_humanoid(p_type_info_id, p_level_format)) {
		if (p_mesh_id == 8) {
			return true;
		}
		else if (p_mesh_id == 11) {
			return true;
		}
	}

	return false;
}

static DummyBoneInfo get_dummy_bone_info(uint32_t p_type_info_id, uint32_t p_mesh_id, TRLevelFormat p_level_format) {
	DummyBoneInfo dummy_bone_info;

	if (is_lara_compatible_humanoid(p_type_info_id, p_level_format)) {
		if (p_mesh_id == 8) {
			dummy_bone_info.name = "LeftShoulder";
			dummy_bone_info.offset.x = -32;
			dummy_bone_info.rotation = Quaternion(0.5, -0.5, -0.5, -0.5);
		}
		else if (p_mesh_id == 11) {
			dummy_bone_info.name = "RightShoulder";
			dummy_bone_info.offset.x = 32;
			dummy_bone_info.rotation = Quaternion(0.5, 0.5, 0.5, -0.5);
		}
	}

	return dummy_bone_info;
}


static bool does_overwrite_mesh_rotation(uint32_t p_type_info_id, TRLevelFormat p_level_format) {
	if (is_lara_compatible_humanoid(p_type_info_id, p_level_format)) {
		return true;
	}

	return false;
}

static Basis get_overwritten_mesh_rotation(uint32_t p_type_info_id, uint32_t p_mesh_id, TRLevelFormat p_level_format) {
	if (is_lara_compatible_humanoid(p_type_info_id, p_level_format)) {
		switch (p_mesh_id) {
			case 0: // Hips
				return Basis(Quaternion());
			case 1: // LeftUpperLeg
				return Basis(Quaternion(0.0, 0.0, 1.0, 0.0));
			case 2: // LeftLowerLeg
				return Basis(Quaternion(1.0, 0.0, 0.0, 0.0));
			case 3: // LeftFoot
				return Basis(Quaternion(0.0, 0.707, 0.707, 0.0));
			case 4: // RightUpperLeg
				return Basis(Quaternion(0.0, 0.0, 1.0, 0.0));
			case 5: // RightLowerLeg
				return Basis(Quaternion(1.0, 0.0, 0.0, 0.0));
			case 6: // RightFoot
				return Basis(Quaternion(0.0, 0.707, 0.707, 0.0));
			case 7: // Spine
				return Basis(Quaternion(0.0, 0.0, 0.0, 1.0));
			case 8: // RightUpperArm
				return Basis(Quaternion(0.707, 0.0, 0.707, 0.0));
			case 9: // RightLowerArm
				return Basis(Quaternion(0.0, 0.0, 1.0, 0.0));
			case 10: // RightHand
				return Basis(Quaternion(0.707, 0.0, 0.707, 0.0));
			case 11: // LeftUpperArm
				return Basis(Quaternion(0.707, 0.0, -0.707, 0.0));
			case 12: // LeftLowerArm
				return Basis(Quaternion(0.0, 0.0, 1.0, 0.0));
			case 13: // LeftHand
				return Basis(Quaternion(0.707, 0.0, -0.707, 0.0));
			case 14: // Head
				return Basis(Quaternion(0.0, 0.0, 0.0, 1.0));
		}
		return Basis();
	}

	return Basis();
}

