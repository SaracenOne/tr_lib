#pragma once
#include "tr_level.hpp"
#include "tr_names.hpp"
#include "tr_level_data.hpp"

static String get_tr4_type_info_name(uint32_t p_type_info_id) {
	switch (p_type_info_id) {
	case 0:
		return "Lara";
	case 1:
		return "Lara Pistols Animations";
	case 2:
		return "Lara Uzis Animations";
	case 3:
		return "Lara Shotgun Animations";
	case 4:
		return "Lara Crossbow Animations";
	case 5:
		return "Lara Grenade Gun Animations";
	case 6:
		return "Lara Sixshooter Animations";
	case 7:
		return "Lara Flare Animations";
	case 8:
		return "Lara Skin";
	case 9:
		return "Lara Skin Joints";
	case 10:
		return "Lara Scream";
	case 11:
		return "Lara Crossbow Laser";
	case 12:
		return "Lara Sixshooter Laser";
	case 13:
		return "Lara Holsters";
	case 14:
		return "Lara Holsters Pistols";
	case 15:
		return "Lara Holsters Uzis";
	case 16:
		return "Lara Holsters Sixshooter";
	case 17:
		return "Lara Speech Head 1";
	case 18:
		return "Lara Speech Head 2";
	case 19:
		return "Lara Speech Head 3";
	case 20:
		return "Lara Speech Head 4";
	case 21:
		return "Actor1 Speech Head 1";
	case 22:
		return "Actor1 Speech Head 2";
	case 23:
		return "Actor2 Speech Head 1";
	case 24:
		return "Actor2 Speech Head 2";
	case 25:
		return "Lara Waterskin Mesh";
	case 26:
		return "Lara Petrol Mesh";
	case 27:
		return "Lara Dirt Mesh";
	case 28:
		return "Lara Crowbar Animation";
	case 29:
		return "Lara Torch Animation";
	case 30:
		return "Lara's Hair";
	case 31:
		return "Motorbike";
	case 32:
		return "Jeep";
	case 33:
		return "Vehicle Extra Animation";
	case 34:
		return "Enemy Jeep";
	case 35:
		return "Skeleton";
	case 36:
		return "Skeleton MIP";
	case 37:
		return "Guide";
	case 38:
		return "Guide MIP";
	case 39:
		return "Von Croy";
	case 40:
		return "Von Croy MIP";
	case 41:
		return "Baddy 1";
	case 42:
		return "Baddy 1 MIP";
	case 43:
		return "Baddy 2";
	case 44:
		return "Baddy 2 MIP";
	case 45:
		return "Seth";
	case 459:
		return "Skybox";
	default:
		return String("MovableInfo_") + itos(p_type_info_id);
	}
}

static String get_tr4_bone_name(uint32_t p_type_info_id, uint32_t p_bone_id) {
	switch (p_type_info_id) {
	case 0:
		return get_lara_bone_name(p_bone_id);
	case 1:
		return get_lara_bone_name(p_bone_id);
	case 2:
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
	case 11:
		return get_lara_bone_name(p_bone_id);
	case 12:
		return get_lara_bone_name(p_bone_id);
	case 13:
		return get_lara_bone_name(p_bone_id);
	case 14:
		return get_lara_bone_name(p_bone_id);
	case 15:
		return get_lara_bone_name(p_bone_id);
	case 16:
		return get_lara_bone_name(p_bone_id);
	case 17:
		return get_lara_bone_name(p_bone_id);
	case 18:
		return get_lara_bone_name(p_bone_id);
	case 19:
		return get_lara_bone_name(p_bone_id);
	case 20:
		return get_lara_bone_name(p_bone_id);
	case 21:
		return get_lara_bone_name(p_bone_id);
	case 22:
		return get_lara_bone_name(p_bone_id);
	case 23:
		return get_lara_bone_name(p_bone_id);
	case 24:
		return get_lara_bone_name(p_bone_id);
	case 25:
		return get_lara_bone_name(p_bone_id);
	case 26:
		return get_lara_bone_name(p_bone_id);
	case 27:
		return get_lara_bone_name(p_bone_id);
	case 28:
		return get_lara_bone_name(p_bone_id);
	case 29:
		return get_lara_bone_name(p_bone_id);
	case 33:
		return get_lara_bone_name(p_bone_id);
	}
	return String("bone_") + itos(p_bone_id);
}

static String get_tr4_animation_name(size_t p_type_info_id, size_t p_animation_id) {
	switch (p_type_info_id) {
		case 0: {
			return get_lara_animation_name(p_animation_id, TR4_PC);
		} break;
	}
	return String("animation_") + itos(p_animation_id);
}