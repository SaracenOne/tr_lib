#pragma once
#include "tr_level.hpp"
#include "tr_names.hpp"

static String get_tr3_type_info_name(uint32_t p_type_info_id) {
	switch (p_type_info_id) {
	case 0:
		return "Lara";
	case 1:
		return "Lara Pistol Animation";
	case 2:
		return "Lara's Hair";
	case 3:
		return "Lara Shotgun Animation";
	case 4:
		return "Lara Desert Eagle Animation";
	case 5:
		return "Lara Uzi Animation";
	case 6:
		return "Lara MP5 Animation";
	case 7:
		return "Lara Rocket Launcher Animation";
	case 8:
		return "Lara Grenade Launcher Animation";
	case 9:
		return "Lara Harpoon Gun Animation";
	case 10:
		return "Lara Flare Animation";
	case 22:
		return "Dog";
	case 24:
		return "Kill All Triggers";
	case 28:
		return "Tiger";
	case 71:
		return "Monkey";
	case 315:
		return "Lara Skin";
	case 355:
		return "Skybox";
	case 366:
		return "Pistol Shell Casing";
	case 367:
		return "Shotgun Shell Casing";
	default:
		return String("MovableInfo_") + itos(p_type_info_id);
	}
}

static String get_tr3_bone_name(uint32_t p_type_info_id, uint32_t p_bone_id) {
	switch (p_type_info_id) {
	case 0:
		return get_lara_bone_name(p_bone_id);
	case 1:
		return get_lara_bone_name(p_bone_id);
	case 3:
		return get_lara_bone_name(p_bone_id);
	case 4:
		return get_lara_bone_name(p_bone_id);
	case 5:
		return get_lara_bone_name(p_bone_id);
	case 6:
		return get_lara_bone_name(p_bone_id);
	case 7:
		return get_lara_bone_name(p_bone_id);
	case 8:
		return get_lara_bone_name(p_bone_id);
	case 9:
		return get_lara_bone_name(p_bone_id);
	case 10:
		return get_lara_bone_name(p_bone_id);
	}
	return String("bone_") + itos(p_bone_id);
}

static String get_tr3_animation_name(size_t p_type_info_id, size_t p_animation_id) {
	switch (p_type_info_id) {
		case 0: {
			return get_lara_animation_name(p_animation_id, TR3_PC);
		} break;
	}
	return String("animation_") + itos(p_animation_id);
}