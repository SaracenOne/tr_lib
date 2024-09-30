#pragma once
#include "tr_level.hpp"
#include "tr_names.hpp"

static String get_tr1_lara_bone_name(uint32_t p_bone_id) {
	switch (p_bone_id) {
	case 0:
		return "Hips";
	case 1:
		return "LeftUpperLeg";
	case 2:
		return "LeftLowerLeg";
	case 3:
		return "LeftFoot";
	case 4:
		return "RightUpperLeg";
	case 5:
		return "RightLowerLeg";
	case 6:
		return "RightFoot";
	case 7:
		return "Spine";
	case 8:
		return "RightUpperArm";
	case 9:
		return "RightLowerArm";
	case 10:
		return "RightHand";
	case 11:
		return "LeftUpperArm";
	case 12:
		return "LeftLowerArm";
	case 13:
		return "LeftHand";
	case 14:
		return "Head";
	}
	return String("bone_") + itos(p_bone_id);
}

static String get_tr1_type_info_name(uint32_t p_type_info_id) {
	switch (p_type_info_id) {
	case 0:
		return "Lara";
	case 1:
		return "Lara Pistol Animation";
	case 2:
		return "Lara Shotgun Animation";
	case 3:
		return "Lara Magnum Animation";
	case 4:
		return "Lara Uzi Animation";
	case 5:
		return "Lara Misc Animations";
	case 6:
		return "Lara Doppel";
	case 7:
		return "Wolf";
	case 8:
		return "Bear";
	case 9:
		return "Bat";
	case 10:
		return "Crocodile (on land)";
	case 11:
		return "Crocodile (in water)";
	case 12:
		return "Lion";
	case 13:
		return "Lioness";
	case 14:
		return "Panther";
	case 15:
		return "Gorilla";
	case 16:
		return "Giant Rat (on land)";
	case 17:
		return "Giant Rat (in water)";
	case 18:
		return "T-Rex";
	case 19:
		return "Raptor";
	case 20:
		return "Winged Mutant";
	case 21:
		return "Mutant with Gun Spawn Point";
	case 22:
		return "Mutant Spawn Point";
	case 23:
		return "Atlantean Centaur";
	case 24:
		return "Atlantean Mummy";
	case 25:
		return "Atlantean Dinosaur";
	case 26:
		return "Atlantean Fish";
	case 27:
		return "Larson";
	case 28:
		return "Pierre";
	case 29:
		return "Skateboard";
	case 30:
		return "Skateboard Kid";
	case 31:
		return "Cowboy";
	case 32:
		return "Bald Shotgun Guy";
	case 33:
		return "Winged Natla";
	case 34:
		return "Adam";
	case 35:
		return "Collapsing Floor";
	case 36:
		return "Swinging Blade";
	case 37:
		return "Spikes";
	case 38:
		return "Rolling Ball";
	case 39:
		return "Dart";
	case 40:
		return "Wall-Mounted Dartgun";
	case 41:
		return "Openable Bridge";
	case 42:
		return "Slamming Door";
	case 43:
		return "Sword of Damocles 1";
	case 44:
		return "Thor's Hammer Handle";
	case 45:
		return "Thor's Hammer Head";
	case 46:
		return "Lightning Orb";
	case 47:
		return "Lightning Rod or Mine Drill";
	case 48:
		return "Movable Block 1";
	case 49:
		return "Movable Block 2";
	case 50:
		return "Movable Block 3";
	case 51:
		return "Movable Block 4";
	case 52:
		return "Moving Tall Block";
	case 53:
		return "Falling Ceiling";
	case 54:
		return "Sword of Damocles 2";
	case 55:
		return "Above-Water Switch";
	case 56:
		return "Underwater Switch";
	case 57:
		return "Door 1";
	case 58:
		return "Door 2";
	case 59:
		return "Door 3";
	case 60:
		return "Door 4";
	case 61:
		return "Door 5";
	case 62:
		return "Door 6";
	case 63:
		return "Door 7";
	case 64:
		return "Door 8";
	case 65:
		return "Trapdoor 1";
	case 66:
		return "Trapdoor 2";
	case 68:
		return "Bridge (Flat)";
	case 69:
		return "Bridge (Tilt 1)";
	case 70:
		return "Bridge (Tilt 2)";
	case 71:
		return "Passport (Opening)";
	case 72:
		return "Compass";
	case 73:
		return "Lara's Home Polaroid";
	case 74:
		return "Animated Cogs 1";
	case 75:
		return "Animated Cogs 2";
	case 76:
		return "Animated Cogs 3";
	case 77:
		return "Cutscene Object 1";
	case 78:
		return "Cutscene Object 2";
	case 79:
		return "Cutscene Object 3";
	case 80:
		return "Cutscene Object 4";
	case 81:
		return "Passport (Closed)";
	case 82:
		return "Minimap";
	case 83:
		return "Save Crystal";
	case 84:
		return "Pistols (Sprite)";
	case 85:
		return "Shotgun (Sprite)";
	case 86:
		return "Magnums (Sprite)";
	case 87:
		return "Uzis (Sprite)";
	case 88:
		return "Pistol Ammo (Sprite)";
	case 89:
		return "Shotgun Ammo (Sprite)";
	case 90:
		return "Magnum Ammo (Sprite)";
	case 91:
		return "Uzi Ammo (Sprite)";
	case 93:
		return "Small Medipack (Sprite)";
	case 94:
		return "Large Medipack (Sprite)";
	case 95:
		return "Sunglasses";
	case 96:
		return "Cassette Player and Headphones";
	case 97:
		return "Direction Keys";
	case 98:
		return "Flashlight";
	case 99:
		return "Pistols";
	case 100:
		return "Shotgun";
	case 101:
		return "Magnums";
	case 102:
		return "Uzis";
	case 103:
		return "Pistol Ammo";
	case 104:
		return "Shotgun Ammo";
	case 105:
		return "Magnum Ammo";
	case 106:
		return "Uzi Ammo";
	case 107:
		return "Grenade";
	case 108:
		return "Small Medipack";
	case 109:
		return "Large Medipack";
	case 114:
		return "Puzzle 1";
	case 118:
		return "Slot 1 (Empty)";
	case 122:
		return "Slot 1 (Full)";
	case 127:
		return "Pickup 1";
	case 128:
		return "Midas Trap";
	case 129:
		return "Key 1 (Sprite)";
	case 130:
		return "Key 2 (Sprite)";
	case 131:
		return "Key 3 (Sprite)";
	case 132:
		return "Key 4 (Sprite)";
	case 133:
		return "Key 1";
	case 134:
		return "Key 2";
	case 135:
		return "Key 3";
	case 136:
		return "Key 4";
	case 137:
		return "Lock 1";
	case 138:
		return "Lock 2";
	case 139:
		return "Lock 3";
	case 140:
		return "Lock 4";
	case 143:
		return "Scion Piece (Sprite)";
	case 146:
		return "Complete Scion";
	case 147:
		return "Scion Pedestal";
	case 150:
		return "Scion Piece";
	case 151:
		return "Explosion";
	case 153:
		return "Splash (Sprite)";
	case 155:
		return "Bubbles (Sprite)";
	case 158:
		return "Blood (Sprite)";
	case 160:
		return "Smoke (Sprite)";
	case 161:
		return "Centaur Statue";
	case 162:
		return "Hanging Shack";
	case 163:
		return "Atlantean Egg (Regular)";
	case 164:
		return "Bullet Ricochet (Sprite)";
	case 166:
		return "Gunflare";
	case 169:
		return "Camera Target";
	case 170:
		return "Waterfall Mist";
	case 172:
		return "Atlantean Dart";
	case 173:
		return "Atlantean Grenade";
	case 176:
		return "Lava Particle (Sprite)";
	case 177:
		return "Lava Particle Emitter";
	case 178:
		return "Fire (Sprite)";
	case 179:
		return "Flame Emitter";
	case 180:
		return "Flowing Lava";
	case 181:
		return "Atlantean Egg (Big)";
	case 182:
		return "Motorboat";
	case 189:
		return "Lara's Hair";
	case 190:
		return "Symbols (Sprite)";
	case 191:
		return "Plant 1 (Sprite)";
	case 192:
		return "Plant 2 (Sprite)";
	case 193:
		return "Plant 3 (Sprite)";
	case 194:
		return "Plant 4 (Sprite)";
	case 195:
		return "Plant 5 (Sprite)";
	case 200:
		return "Bag 1 (Sprite)";
	case 204:
		return "Bag 2 (Sprite)";
	case 207:
		return "Gunfire";
	case 212:
		return "Rock 1 (Sprite)";
	case 213:
		return "Rock 2 (Sprite)";
	case 214:
		return "Rock 3 (Sprite)";
	case 216:
		return "Pottery 1 (Sprite)";
	case 217:
		return "Pottery 2 (Sprite)";
	case 231:
		return "Painted Pot (Sprite)";
	case 233:
		return "Inca Mummy (Sprite)";
	case 236:
		return "Pottery 3 (Sprite)";
	case 237:
		return "Pottery 4 (Sprite)";
	case 238:
		return "Pottery 5 (Sprite)";
	case 239:
		return "Pottery 6 (Sprite)";
	default:
		return String("MovableInfo_") + itos(p_type_info_id);
	}
}

static String get_tr1_bone_name(uint32_t p_type_info_id, uint32_t p_bone_id) {
	switch (p_type_info_id) {
		case 0:
			return get_tr1_lara_bone_name(p_bone_id);
		case 1:
			return get_tr1_lara_bone_name(p_bone_id);
		case 2:
			return get_tr1_lara_bone_name(p_bone_id);
		case 3:
			return get_tr1_lara_bone_name(p_bone_id);
		case 4:
			return get_tr1_lara_bone_name(p_bone_id);
		case 5:
			return get_tr1_lara_bone_name(p_bone_id);
	}
	return String("bone_") + itos(p_bone_id);
}

static String get_tr1_animation_name(uint32_t p_type_info_id, uint32_t p_animation_id) {
	switch (p_type_info_id) {
		case 0: {
			switch (p_animation_id) {
			case 0:
				return "run";
			case 1:
				return "walk_forward";
			case 2:
				return "walk_stop_right";
			case 3:
				return "walk_stop_left";
			case 4:
				return "walk_to_run_right";
			case 5:
				return "walk_to_run_left";
			case 6:
				return "run_start";
			case 7:
				return "run_to_walk_right";
			case 8:
				return "run_to_stand_left";
			case 9:
				return "run_to_walk_left";
			case 10:
				return "run_to_stand_right";
			case 11:
				return "stand_still";
			case 12:
				return "turn_right_slow";
			case 13:
				return "turn_left_slow";
			case 14:
				return "jump_forward_land_start_unused";
			case 15:
				return "jump_forward_land_end_unused";
			case 16:
				return "run_jump_right_start";
			case 17:
				return "run_jump_right_continue";
			case 18:
				return "run_jump_left_start";
			case 19:
				return "run_jump_left_continue";
			case 20:
				return "walk_forward_start";
			case 21:
				return "walk_forward_start_continue";
			case 22:
				return "jump_forward_to_freefall";
			case 23:
				return "freefall";
			case 24:
				return "freefall_land";
			case 25:
				return "freefall_land_death";
			case 26:
				return "stand_to_jump_up";
			case 27:
				return "stand_to_jump_up_continue";
			case 28:
				return "jump_up";
			case 29:
				return "jump_up_to_hang_unused";
			case 30:
				return "jump_to_freefall";
			case 31:
				return "jump_to_land";
			case 32:
				return "smash_jump";
			case 33:
				return "smash_jump_continue";
			case 34:
				return "fall_start";
			case 35:
				return "fall";
			case 36:
				return "fall_to_freefall";
			case 37:
				return "hang_to_freefall_unused";
			case 38:
				return "walk_back_end_right";
			case 39:
				return "walk_back_end_left";
			case 40:
				return "walk_back";
			case 41:
				return "walk_back_start";
			case 42:
				return "climb_3click";
			case 43:
				return "climb_3click_end_to_run_unused";
			case 44:
				return "turn_right";
			case 45:
				return "jump_forward_to_freefall";
			case 46:
				return "reach_to_freefall_unused";
			case 47:
				return "roll_alternate_unused";
			case 48:
				return "roll_end_alternate_unused";
			case 49:
				return "jump_forward_end_to_freefall_unused";
			case 50:
				return "climb_2click";
			case 51:
				return "climb_2click_end";
			case 52:
				return "climb_2click_end_to_run";
			case 53:
				return "wall_smash_left";
			case 54:
				return "wall_smash_right";
			case 55:
				return "run_up_step_right";
			case 56:
				return "run_up_step_left";
			case 57:
				return "walk_up_step_right";
			case 58:
				return "walk_up_step_left";
			case 59:
				return "walk_down_left";
			case 60:
				return "walk_down_right";
			case 61:
				return "walk_down_back_left";
			case 62:
				return "walk_down_back_right";
			case 63:
				return "wall_switch_down";
			case 64:
				return "wall_switch_up";
			case 65:
				return "sidestep_left";
			case 66:
				return "sidestep_left_end";
			case 67:
				return "sidestep_right";
			case 68:
				return "sidestep_right_end";
			case 69:
				return "rotate_left";
			case 70:
				return "slide_forward";
			case 71:
				return "slide_forward_end";
			case 72:
				return "slide_forward_stop";
			case 73:
				return "stand_to_jump";
			case 74:
				return "jump_back_start";
			case 75:
				return "jump_back";
			case 76:
				return "jump_forward_start";
			case 77:
				return "jump_forward";
			case 78:
				return "jump_left_start";
			case 79:
				return "jump_left";
			case 80:
				return "jump_right_start";
			case 81:
				return "jump_right";
			case 82:
				return "land";
			case 83:
				return "jump_back_to_freefall";
			case 84:
				return "jump_left_to_freefall";
			case 85:
				return "jump_right_to_freefall";
			case 86:
				return "underwater_swim_forward";
			case 87:
				return "underwater_swim_forward_drift";
			case 88:
				return "small_jump_back_start";
			case 89:
				return "small_jump_back";
			case 90:
				return "small_jump_back_end";
			case 91:
				return "jump_up_start";
			case 92:
				return "land_to_run";
			case 93:
				return "fall_back";
			case 94:
				return "jump_forward_to_reach";
			case 95:
				return "reach";
			case 96:
				return "reach_to_hang";
			case 97:
				return "climb_on";
			case 98:
				return "reach_to_freefall";
			case 99:
				return "fall_crouching_landing_unused";
			case 100:
				return "jump_forward_to_reach_late_unused";
			case 101:
				return "jump_forward_start_to_reach_alternate_unused";
			case 102:
				return "climb_on_end";
			case 103:
				return "stand_idle";
			case 104:
				return "slide_backward_start";
			case 105:
				return "slide_backward";
			case 106:
				return "slide_backward_end";
			case 107:
				return "underwater_swim_to_idle";
			case 108:
				return "underwater_idle";
			case 109:
				return "underwater_idle_to_swim";
			case 110:
				return "on_water_idle";
			case 111:
				return "on_water_to_stand_high";
			case 112:
				return "freefall_to_underwater";
			case 113:
				return "on_water_dive_alternate_unused";
			case 114:
				return "underwater_to_on_water";
			case 115:
				return "on_water_swim_forward_dive_unused";
			case 116:
				return "on_water_swim_forward";
			case 117:
				return "on_water_swim_forward_to_idle";
			case 118:
				return "on_water_idle_to_swim_forward";
			case 119:
				return "on_water_dive";
			case 120:
				return "pushable_grab";
			case 121:
				return "pushable_release";
			case 122:
				return "pushable_pull";
			case 123:
				return "pushable_push";
			case 124:
				return "underwater_death";
			case 125:
				return "hit_front";
			case 126:
				return "hit_back";
			case 127:
				return "hit_left";
			case 128:
				return "hit_right";
			case 129:
				return "underwater_switch";
			case 130:
				return "underwater_pickup";
			case 131:
				return "use_key";
			case 132:
				return "on_water_death";
			case 133:
				return "run_death";
			case 134:
				return "use_puzzle";
			case 135:
				return "pickup";
			case 136:
				return "shimmy_left";
			case 137:
				return "shimmy_right";
			case 138:
				return "stand_death";
			case 139:
				return "boulder_death";
			case 140:
				return "on_water_idle_to_swim_back";
			case 141:
				return "on_water_swim_back";
			case 142:
				return "on_water_swim_back_to_idle";
			case 143:
				return "on_water_swim_left";
			case 144:
				return "on_water_swim_right";
			case 145:
				return "death_jump";
			case 146:
				return "roll_start";
			case 147:
				return "roll_continue";
			case 148:
				return "roll_end";
			case 149:
				return "spike_death";
			case 150:
				return "reach_to_thin_ledge";
			case 151:
				return "swandive_roll";
			case 152:
				return "swandive_to_underwater";
			case 153:
				return "freefall_swandive";
			case 154:
				return "freefall_swandive_to_underwater";
			case 155:
				return "swandive_death";
			case 156:
				return "swandive_left";
			case 157:
				return "swandive_right";
			case 158:
				return "swandive_start";
			case 159:
				return "climb_on_handstand";
			}
		} break;
		case 1: {
			switch (p_animation_id) {
			case 0:
				return "pistols_aim";
			case 1:
				return "pistols_holster";
			case 2:
				return "pistols_unholster";
			case 3:
				return "pistols_shoot";
			}
		} break;
		case 2: {
			switch (p_animation_id) {
			case 0:
				return "shotgun_aim";
			case 1:
				return "shotgun_holster";
			case 2:
				return "shotgun_unholster";
			case 3:
				return "shotgun_shoot";
			}
		} break;
		case 3: {
			switch (p_animation_id) {
			case 0:
				return "magnums_aim";
			case 1:
				return "magnums_holster";
			case 2:
				return "magnums_unholster";
			case 3:
				return "magnums_shoot";
			}
		} break;
		case 4: {
			switch (p_animation_id) {
			case 0:
				return "uzi_aim";
			case 1:
				return "uzi_holster";
			case 2:
				return "uzi_unholster";
			case 3:
				return "uzi_shoot";
			}
		} break;
	}
	return String("animation_") + itos(p_animation_id);
}

static String get_animation_name(uint32_t p_type_info_id, uint32_t p_animation_id) {
	return get_tr1_animation_name(p_type_info_id, p_animation_id);
}