#pragma once
#include "tr_level.hpp"
#include "tr_names.hpp"


static String get_tr1_type_info_name(size_t p_type_info_id) {
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

static String get_tr1_bone_name(size_t p_type_info_id, size_t p_bone_id) {
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
	}
	return String("bone_") + itos(p_bone_id);
}

static String get_tr1_animation_name(size_t p_type_info_id, size_t p_animation_id) {
	switch (p_type_info_id) {
		case 0: {
			return get_lara_animation_name(p_animation_id, TR1_PC);
		} break;
	}
	return String("animation_") + itos(p_animation_id);
}