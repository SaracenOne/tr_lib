#include "tr_level.hpp"

const AnimationNodeMetaInfo* get_animation_node_meta_info_for_animation(uint32_t p_type_info_id, StringName p_animation_name, TRLevelFormat p_level_format) {
	if (p_type_info_id == 0) {
		FIND_ANIMATION_META_INFO(lara_animation_node_meta_info, p_animation_name)
	}

	return nullptr;
}