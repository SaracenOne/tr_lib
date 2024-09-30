#pragma once
#include "tr_level.hpp"
#include "tr_names_tr1.hpp"
#include "tr_names_tr2.hpp"

static String get_type_info_name(uint32_t p_type_info_id, TRLevelFormat p_level_format) {
	switch(p_level_format) {
	case TRLevelFormat::TR1_PC:
		return get_tr1_type_info_name(p_type_info_id);
	case TRLevelFormat::TR2_PC:
		return get_tr2_type_info_name(p_type_info_id);
	default:
		return String("MoveableInfo_") + itos(p_type_info_id);
	}
}

static String get_bone_name(uint32_t p_type_info_id, uint32_t p_bone_id, TRLevelFormat p_level_format) {
	switch (p_level_format) {
		case TRLevelFormat::TR1_PC:
			return get_tr1_bone_name(p_type_info_id, p_bone_id);
		case TRLevelFormat::TR2_PC:
			return get_tr2_bone_name(p_type_info_id, p_bone_id);
		default:
			return String("bone_") + itos(p_bone_id);
	}
}

static String get_animation_name(uint32_t p_type_info_id, uint32_t p_animation_id, TRLevelFormat p_level_format) {
	switch (p_level_format) {
		case TRLevelFormat::TR1_PC:
			return get_tr1_animation_name(p_type_info_id, p_animation_id);
		default:
			return String("animation_") + itos(p_animation_id);
	}
}