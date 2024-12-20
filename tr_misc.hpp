#pragma once
#include "tr_level.hpp"
#include "tr_level_data.hpp"

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

struct AnimationNodeMetaInfo {
	String animation_name;
	String animation_path;
	Vector2 position;
};

const AnimationNodeMetaInfo lara_animation_node_meta_info[] = {
	{"boulder_death", "boulder_death", Vector2(3328.0, 2304.0)},
	{"button_push", "button_push", Vector2(256.0, 3584.0)},
	{"climb_2click", "climb_2click", Vector2(2048.0, 768.0)},
	{"climb_2click_end", "climb_2click_end", Vector2(2304.0, 768.0)},
	{"climb_2click_end_to_run", "climb_2click_end_to_run", Vector2(2560.0, 768.0)},
	{"climb_3click", "climb_3click", Vector2(0.0, 768.0)},
	{"climb_3click_end_to_run_unused", "climb_3click_end_to_run_unused", Vector2(256.0, 768.0)},
	{"climb_on", "climb_on", Vector2(3328.0, 1536.0)},
	{"climb_on_end", "climb_on_end", Vector2(1024.0, 1792.0)},
	{"climb_on_handstand", "climb_on_handstand", Vector2(1280.0, 2816.0)},
	{"death_jump", "death_jump", Vector2(1280.0, 2560.0)},
	{"fall", "fall", Vector2(1784.0, -1611.0)},
	{"fall_back", "fall_back", Vector2(2002.0, -1529.0)},
	{"fall_crouching_landing_unused", "fall_crouching_landing_unused", Vector2(2092.0, -2111.0)},
	{"fall_start", "fall_start", Vector2(1584.0, -1611.0)},
	{"fall_to_freefall", "fall_to_freefall", Vector2(2002.0, -1611.0)},
	{"flare_pickup", "flare_pickup", Vector2(2048.0, 3584.0)},
	{"flare_throw", "flare_throw", Vector2(1792.0, 3328.0)},
	{"freefall", "freefall", Vector2(2272.0, -1611.0)},
	{"freefall_land", "freefall_land", Vector2(2542.0, -1611.0)},
	{"freefall_land_death", "freefall_land_death", Vector2(2839.0, -1611.0)},
	{"freefall_swandive", "freefall_swandive", Vector2(3328.0, 2560.0)},
	{"freefall_swandive_to_underwater", "freefall_swandive_to_underwater", Vector2(0.0, 2816.0)},
	{"freefall_to_underwater", "freefall_to_underwater", Vector2(0.0, 2048.0)},
	{"hang_to_freefall_unused", "hang_to_freefall_unused", Vector2(2304.0, 512.0)},
	{"hit_back", "hit_back", Vector2(0.0, 2304.0)},
	{"hit_front", "hit_front", Vector2(3328.0, 2048.0)},
	{"hit_left", "hit_left", Vector2(256.0, 2304.0)},
	{"hit_right", "hit_right", Vector2(512.0, 2304.0)},
	{"jump_back", "jump_back", Vector2(2083.0, -558.0)},
	{"jump_back_roll_end", "jump_back_roll_end", Vector2(768.0, 3840.0)},
	{"jump_back_roll_start", "jump_back_roll_start", Vector2(512.0, 3840.0)},
	{"jump_back_start", "jump_back_start", Vector2(2083.0, -623.0)},
	{"jump_back_to_freefall", "jump_back_to_freefall", Vector2(2083.0, -489.0)},
	{"jump_forward_end_to_freefall_unused", "jump_forward_end_to_freefall_unused", Vector2(1792.0, 768.0)},
	{"jump_forward_land_end_unused", "jump_forward_land_end_unused", Vector2(284.0, 378.0)},
	{"jump_forward_land_start_unused", "jump_forward_land_start_unused", Vector2(0.0, 256.0)},
	{"jump_forward_to_freefall", "jump_forward_to_freefall", Vector2(1374.0, -1323.0)},
	{"jump_forward_to_freefall_continue", "jump_forward_to_freefall_continue", Vector2(2083.0, -1228.0)},
	{"jump_forwards", "jump_forwards", Vector2(2083.0, -1160.0)},
	{"jump_forwards_roll_end", "jump_forwards_roll_end", Vector2(256.0, 3840.0)},
	{"jump_forwards_roll_start", "jump_forwards_roll_start", Vector2(0.0, 3840.0)},
	{"jump_forwards_start", "jump_forwards_start", Vector2(2083.0, -1101.0)},
	{"jump_forwards_start_to_reach_alternate_unused", "jump_forwards_start_to_reach_alternate_unused", Vector2(768.0, 1792.0)},
	{"jump_forwards_to_reach", "jump_forwards_to_reach", Vector2(2083.0, -1323.0)},
	{"jump_forwards_to_reach_late_unused", "jump_forwards_to_reach_late_unused", Vector2(512.0, 1792.0)},
	{"jump_left", "jump_left", Vector2(1584.0, -854.0)},
	{"jump_left_start", "jump_left_start", Vector2(1761.0, -854.0)},
	{"jump_left_to_freefall", "jump_left_to_freefall", Vector2(1398.0, -854.0)},
	{"jump_right", "jump_right", Vector2(2696.0, -854.0)},
	{"jump_right_start", "jump_right_start", Vector2(2542.0, -854.0)},
	{"jump_right_to_freefall", "jump_right_to_freefall", Vector2(2864.0, -854.0)},
	{"jump_to_freefall", "jump_to_freefall", Vector2(2272.0, -1529.0)},
	{"jump_to_land", "jump_to_land", Vector2(1584.0, -977.0)},
	{"jump_up", "jump_up", Vector2(2083.0, -977.0)},
	{"jump_up_start", "jump_up_start", Vector2(2083.0, -910.0)},
	{"jump_up_to_hang_unused", "jump_up_to_hang_unused", Vector2(256.0, 512.0)},
	{"kick", "kick", Vector2(1024.0, 3840.0)},
	{"ladder_backflip_continue", "ladder_backflip_continue", Vector2(256.0, 3328.0)},
	{"ladder_backflip_start", "ladder_backflip_start", Vector2(0.0, 3328.0)},
	{"ladder_climb_on", "ladder_climb_on", Vector2(1536.0, 3072.0)},
	{"ladder_down", "ladder_down", Vector2(0.0, 3072.0)},
	{"ladder_down_hanging", "ladder_down_hanging", Vector2(1536.0, 3328.0)},
	{"ladder_down_start", "ladder_down_start", Vector2(256.0, 3072.0)},
	{"ladder_down_stop_left", "ladder_down_stop_left", Vector2(3072.0, 2816.0)},
	{"ladder_down_stop_right", "ladder_down_stop_right", Vector2(3328.0, 2816.0)},
	{"ladder_hang", "ladder_hang", Vector2(1024.0, 3072.0)},
	{"ladder_hang_to_idle", "ladder_hang_to_idle", Vector2(1280.0, 3072.0)},
	{"ladder_idle", "ladder_idle", Vector2(1536.0, 2936.0)},
	{"ladder_left", "ladder_left", Vector2(768.0, 3072.0)},
	{"ladder_right", "ladder_right", Vector2(512.0, 3072.0)},
	{"ladder_to_hang_down", "ladder_to_hang_down", Vector2(3072.0, 3328.0)},
	{"ladder_to_hang_left", "ladder_to_hang_left", Vector2(1536.0, 3584.0)},
	{"ladder_to_hang_right", "ladder_to_hang_right", Vector2(1280.0, 3584.0)},
	{"ladder_up", "ladder_up", Vector2(2106.0, 2965.0)},
	{"ladder_up_hanging", "ladder_up_hanging", Vector2(1280.0, 3328.0)},
	{"ladder_up_start", "ladder_up_start", Vector2(2816.0, 2816.0)},
	{"ladder_up_stop_left", "ladder_up_stop_left", Vector2(2304.0, 2816.0)},
	{"ladder_up_stop_right", "ladder_up_stop_right", Vector2(1668.0, 2816.0)},
	{"land", "land", Vector2(3820.0, -1030.0)},
	{"land_to_run", "land_to_run", Vector2(1784.0, -1323.0)},
	{"on_water_death", "on_water_death", Vector2(1536.0, 2304.0)},
	{"on_water_dive", "on_water_dive", Vector2(1792.0, 2048.0)},
	{"on_water_dive_alternate_unused", "on_water_dive_alternate_unused", Vector2(256.0, 2048.0)},
	{"on_water_idle", "on_water_idle", Vector2(3072.0, 1792.0)},
	{"on_water_idle_to_swim_back", "on_water_idle_to_swim_back", Vector2(0.0, 2560.0)},
	{"on_water_idle_to_swim_forwards", "on_water_idle_to_swim_forwards", Vector2(1536.0, 2048.0)},
	{"on_water_swim_back", "on_water_swim_back", Vector2(256.0, 2560.0)},
	{"on_water_swim_back_to_idle", "on_water_swim_back_to_idle", Vector2(512.0, 2560.0)},
	{"on_water_swim_forwards", "on_water_swim_forwards", Vector2(1024.0, 2048.0)},
	{"on_water_swim_forwards_dive_unused", "on_water_swim_forwards_dive_unused", Vector2(768.0, 2048.0)},
	{"on_water_swim_forwards_to_idle", "on_water_swim_forwards_to_idle", Vector2(1280.0, 2048.0)},
	{"on_water_swim_left", "on_water_swim_left", Vector2(768.0, 2560.0)},
	{"on_water_swim_right", "on_water_swim_right", Vector2(1024.0, 2560.0)},
	{"on_water_to_stand_high", "on_water_to_stand_high", Vector2(3328.0, 1792.0)},
	{"on_water_to_stand_medium", "on_water_to_stand_medium", Vector2(2304.0, 3328.0)},
	{"on_water_to_wade", "on_water_to_wade", Vector2(2048.0, 3328.0)},
	{"on_water_to_wade_low", "on_water_to_wade_low", Vector2(2816.0, 3328.0)},
	{"on_water_to_wade_shallow_unused", "on_water_to_wade_shallow_unused", Vector2(2048.0, 3072.0)},
	{"pickup", "pickup", Vector2(2304.0, 2304.0)},
	{"pushable_grab", "pushable_grab", Vector2(2048.0, 2048.0)},
	{"pushable_pull", "pushable_pull", Vector2(2560.0, 2048.0)},
	{"pushable_push", "pushable_push", Vector2(2816.0, 2048.0)},
	{"pushable_release", "pushable_release", Vector2(2304.0, 2048.0)},
	{"reach", "reach", Vector2(2816.0, 1536.0)},
	{"reach_to_freefall", "reach_to_freefall", Vector2(2083.0, -1850.0)},
	{"reach_to_freefall_unused", "reach_to_freefall_unused", Vector2(1024.0, 768.0)},
	{"reach_to_hang", "reach_to_hang", Vector2(3072.0, 1536.0)},
	{"reach_to_thin_ledge", "reach_to_thin_ledge", Vector2(2560.0, 2560.0)},
	{"roll_alternate_unused", "roll_alternate_unused", Vector2(-115.0, -930.0)},
	{"roll_continue", "roll_continue", Vector2(622.0, -1365.0)},
	{"roll_end", "roll_end", Vector2(878.0, -1365.0)},
	{"roll_end_alternate_unused", "roll_end_alternate_unused", Vector2(141.0, -930.0)},
	{"roll_start", "roll_start", Vector2(366.0, -1365.0)},
	{"rotate_left", "rotate_left", Vector2(3328.0, 1024.0)},
	{"run", "run", Vector2(878.0, -378.0)},
	{"run_death", "run_death", Vector2(879.0, -508.0)},
	{"run_jump_left_continue", "run_jump_left_continue", Vector2(1339.0, -378.0)},
	{"run_jump_left_start", "run_jump_left_start", Vector2(1114.0, -378.0)},
	{"run_jump_right_continue", "run_jump_right_continue", Vector2(391.0, -378.0)},
	{"run_jump_right_start", "run_jump_right_start", Vector2(622.0, -378.0)},
	{"run_jump_roll_end", "run_jump_roll_end", Vector2(3328.0, 3584.0)},
	{"run_jump_roll_start", "run_jump_roll_start", Vector2(2816.0, 3584.0)},
	{"run_start", "run_start", Vector2(878.0, -181.0)},
	{"run_to_stand_left", "run_to_stand_left", Vector2(391.0, -686.0)},
	{"run_to_stand_right", "run_to_stand_right", Vector2(1339.0, -686.0)},
	{"run_to_wade_left", "run_to_wade_left", Vector2(2560.0, 3072.0)},
	{"run_to_wade_right", "run_to_wade_right", Vector2(2816.0, 3072.0)},
	{"run_to_walk_left", "run_to_walk_left", Vector2(627.0, -446.0)},
	{"run_to_walk_right", "run_to_walk_right", Vector2(1114.0, -446.0)},
	{"run_up_step_left", "run_up_step_left", Vector2(0.0, 1024.0)},
	{"run_up_step_right", "run_up_step_right", Vector2(3328.0, 768.0)},
	{"shimmy_left", "shimmy_left", Vector2(2560.0, 2304.0)},
	{"shimmy_right", "shimmy_right", Vector2(2816.0, 2304.0)},
	{"sidestep_left", "sidestep_left", Vector2(2304.0, 1024.0)},
	{"sidestep_left_end", "sidestep_left_end", Vector2(2560.0, 1024.0)},
	{"sidestep_right", "sidestep_right", Vector2(2816.0, 1024.0)},
	{"sidestep_right_end", "sidestep_right_end", Vector2(3072.0, 1024.0)},
	{"slide_backwards", "slide_backwards", Vector2(2760.0, -1030.0)},
	{"slide_backwards_end", "slide_backwards_end", Vector2(2949.0, -1030.0)},
	{"slide_backwards_start", "slide_backwards_start", Vector2(2560.0, -1030.0)},
	{"slide_forwards", "slide_forwards", Vector2(0.0, 1280.0)},
	{"slide_forwards_end", "slide_forwards_end", Vector2(256.0, 1280.0)},
	{"slide_forwards_stop", "slide_forwards_stop", Vector2(512.0, 1280.0)},
	{"small_jump_back", "small_jump_back", Vector2(1280.0, 1536.0)},
	{"small_jump_back_end", "small_jump_back_end", Vector2(1536.0, 1536.0)},
	{"small_jump_back_start", "small_jump_back_start", Vector2(1024.0, 1536.0)},
	{"smash_jump", "smash_jump", Vector2(613.0, -1752.0)},
	{"smash_jump_continue", "smash_jump_continue", Vector2(869.0, -1752.0)},
	{"somersault", "somersault", Vector2(3072.0, 3584.0)},
	{"spike_death", "spike_death", Vector2(2304.0, 2560.0)},
	{"stand_death", "stand_death", Vector2(3072.0, 2304.0)},
	{"stand_idle", "stand_idle", Vector2(879.0, -822.0)},
	{"stand_still", "stand_still", Vector2(879.0, -686.0)},
	{"stand_to_jump", "stand_to_jump", Vector2(1761.0, -623.0)},
	{"stand_to_jump_up", "stand_to_jump_up", Vector2(2083.0, -794.0)},
	{"stand_to_jump_up_continue", "stand_to_jump_up_continue", Vector2(2083.0, -854.0)},
	{"stand_to_ladder", "stand_to_ladder", Vector2(1531.0, 2698.0)},
	{"stand_to_wade", "stand_to_wade", Vector2(1024.0, 3328.0)},
	{"swandive_death", "swandive_death", Vector2(256.0, 2816.0)},
	{"swandive_left", "swandive_left", Vector2(512.0, 2816.0)},
	{"swandive_right", "swandive_right", Vector2(768.0, 2816.0)},
	{"swandive_roll", "swandive_roll", Vector2(2816.0, 2560.0)},
	{"swandive_start", "swandive_start", Vector2(1024.0, 2816.0)},
	{"swandive_to_underwater", "swandive_to_underwater", Vector2(3072.0, 2560.0)},
	{"switch_small_down", "switch_small_down", Vector2(3328.0, 3328.0)},
	{"switch_small_up", "switch_small_up", Vector2(0.0, 3584.0)},
	{"turn_left_slow", "turn_left_slow", Vector2(706.0, -930.0)},
	{"turn_right", "turn_right", Vector2(1114.0, -822.0)},
	{"turn_right_slow", "turn_right_slow", Vector2(1217.0, -930.0)},
	{"underwater_death", "underwater_death", Vector2(3072.0, 2048.0)},
	{"underwater_flare_pickup", "underwater_flare_pickup", Vector2(2560.0, 3584.0)},
	{"underwater_idle", "underwater_idle", Vector2(2560.0, 1792.0)},
	{"underwater_idle_to_swim", "underwater_idle_to_swim", Vector2(2816.0, 1792.0)},
	{"underwater_pickup", "underwater_pickup", Vector2(1024.0, 2304.0)},
	{"underwater_roll_end", "underwater_roll_end", Vector2(2304.0, 3584.0)},
	{"underwater_roll_start", "underwater_roll_start", Vector2(1792.0, 3584.0)},
	{"underwater_swim_forwards", "underwater_swim_forwards", Vector2(512.0, 1536.0)},
	{"underwater_swim_forwards_drift", "underwater_swim_forwards_drift", Vector2(768.0, 1536.0)},
	{"underwater_swim_to_idle", "underwater_swim_to_idle", Vector2(2304.0, 1792.0)},
	{"underwater_swim_to_still_huddle", "underwater_swim_to_still_huddle", Vector2(512.0, 3584.0)},
	{"underwater_swim_to_still_medium", "underwater_swim_to_still_medium", Vector2(1024.0, 3584.0)},
	{"underwater_swim_to_still_sprawl", "underwater_swim_to_still_sprawl", Vector2(768.0, 3584.0)},
	{"underwater_switch", "underwater_switch", Vector2(768.0, 2304.0)},
	{"underwater_to_on_water", "underwater_to_on_water", Vector2(512.0, 2048.0)},
	{"underwater_to_stand", "underwater_to_stand", Vector2(2560.0, 3328.0)},
	{"unknown_1", "unknown_1", Vector2(1792.0, 3072.0)},
	{"use_key", "use_key", Vector2(1280.0, 2304.0)},
	{"use_puzzle", "use_puzzle", Vector2(2048.0, 2304.0)},
	{"wade", "wade", Vector2(1933.0, 2490.0)},
	{"wade_to_run_left", "wade_to_run_left", Vector2(3072.0, 3072.0)},
	{"wade_to_run_right", "wade_to_run_right", Vector2(3328.0, 3072.0)},
	{"wade_to_stand_left", "wade_to_stand_left", Vector2(768.0, 3328.0)},
	{"wade_to_stand_right", "wade_to_stand_right", Vector2(512.0, 3328.0)},
	{"walk_back", "walk_back", Vector2(878.0, 278.0)},
	{"walk_back_end_left", "walk_back_end_left", Vector2(622.0, 378.0)},
	{"walk_back_end_right", "walk_back_end_right", Vector2(1130.0, 378.0)},
	{"walk_back_start", "walk_back_start", Vector2(876.0, 154.0)},
	{"walk_down_back_left", "walk_down_back_left", Vector2(1280.0, 1024.0)},
	{"walk_down_back_right", "walk_down_back_right", Vector2(1536.0, 1024.0)},
	{"walk_down_left", "walk_down_left", Vector2(768.0, 1024.0)},
	{"walk_down_right", "walk_down_right", Vector2(1024.0, 1024.0)},
	{"walk_forward_start", "walk_forward_start", Vector2(869.0, 32.0)},
	{"walk_forward_start_continue", "walk_forward_start_continue", Vector2(877.0, -35.0)},
	{"walk_forwards", "walk_forwards", Vector2(879.0, -102.0)},
	{"walk_stop_left", "walk_stop_left", Vector2(706.0, -623.0)},
	{"walk_stop_right", "walk_stop_right", Vector2(1114.0, -623.0)},
	{"walk_to_run_left", "walk_to_run_left", Vector2(627.0, -102.0)},
	{"walk_to_run_right", "walk_to_run_right", Vector2(1114.0, -102.0)},
	{"walk_up_step_left", "walk_up_step_left", Vector2(512.0, 1024.0)},
	{"walk_up_step_right", "walk_up_step_right", Vector2(256.0, 1024.0)},
	{"wall_smash_left", "wall_smash_left", Vector2(2816.0, 768.0)},
	{"wall_smash_right", "wall_smash_right", Vector2(3072.0, 768.0)},
	{"wall_switch_down", "wall_switch_down", Vector2(1792.0, 1024.0)},
	{"wall_switch_up", "wall_switch_up", Vector2(2048.0, 1024.0)},
	{"zipline_fall", "zipline_fall", Vector2(872.0, -1517.0)},
	{"zipline_grab", "zipline_grab", Vector2(360.0, -1517.0)},
	{"zipline_ride", "zipline_ride", Vector2(616.0, -1517.0)},
};

#define FIND_ANIMATION_META_INFO(current_array, p_animation_name) \
	for (const AnimationNodeMetaInfo &E : current_array) { \
		if (E.animation_name == p_animation_name) { return &E; } \
	}

#include <scene/animation/animation_tree.h>
#include <scene/animation/animation_blend_tree.h>
#include <scene/animation/animation_node_state_machine.h>

extern const AnimationNodeMetaInfo* get_animation_node_meta_info_for_animation(uint32_t p_type_info_id, StringName p_animation_name, TRLevelFormat p_level_format);
extern const Ref<AnimationNode> get_animation_tree_root_node_for_object(uint32_t p_type_info_id, TRLevelFormat p_level_format, Ref<AnimationNodeStateMachine> p_state_machine);