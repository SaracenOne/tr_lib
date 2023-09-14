#pragma once
#include "tr_level.hpp"

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

static String get_tr2_type_info_name(uint32_t p_type_info_id) {
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
		return "Lara Automatic Pistol Animation";
	case 5:
		return "Lara Uzi Animation";
	case 6:
		return "Lara M16 Animation";
	case 7:
		return "Lara Grenade Launcher Animation";
	case 8:
		return "Lara Harpoon Gun Animation";
	case 9:
		return "Lara Flare Animation";
	case 11:
		return "Lara Boat Animation";
	case 12:
		return "Lara Misc Animations";
	case 14:
		return "Speedboat";
	case 15:
		return "Doberman";
	case 16:
		return "Masked Goon 1";
	case 17:
		return "Masked Goon 2";
	case 18:
		return "Masked Goon 3";
	case 19:
		return "Knife Thrower";
	case 20:
		return "Shotgun Boss";
	case 21:
		return "Mouse";
	case 22:
		return "Dragon (Front)";
	case 23:
		return "Dragon (Back)";
	case 24:
		return "Gondola";
	case 25:
		return "Shark";
	case 28:
		return "Barracuda";
	case 29:
		return "Scuba Diver";
	case 32:
		return "Baseball Bat Goon";
	case 34:
		return "Flamethrower Goon";
	case 36:
		return "Small Spider";
	case 37:
		return "Giant Spider";
	case 39:
		return "Tiger or Snow Leopard or White Tiger";
	case 40:
		return "Marco Bartoli";
	case 41:
		return "Xian Spear Guard";
	case 42:
		return "Xian Spear Guard (Statue)";
	case 43:
		return "Xian Sword Guard";
	case 44:
		return "Xian Sword Guard (Statue)";
	case 45:
		return "Yeti";
	case 46:
		return "Guardian of Talion";
	case 47:
		return "Eagle";
	case 48:
		return "Arctic Mercenary";
	case 49:
		return "Arctic Mercenary (Black Ski Mask)";
	case 55:
		return "Collapsing Floor";
	case 58:
		return "Swinging Obstacle";
	case 59:
		return "Spikes";
	case 61:
		return "Disk";
	case 62:
		return "Disk Gun";
	case 63:
		return "Drawbridge";
	case 65:
		return "Lift";
	case 67:
		return "Movable Block 1";
	case 68:
		return "Movable Block 2";
	case 69:
		return "Movable Block 3";
	case 70:
		return "Movable Block 4";
	case 71:
		return "Big Bowl";
	case 72:
		return "Shootable Window";
	case 73:
		return "Defenstration Window";
	case 76:
		return "Propeller";
	case 79:
		return "Sandbag or Falling Ceiling";
	case 81:
		return "Wall Blade";
	case 82:
		return "Statue with Sword";
	case 83:
		return "Multiple Boulders";
	case 84:
		return "Falling Icicles";
	case 85:
		return "Moving Spiky Wall";
	case 86:
		return "Bounce Pad";
	case 87:
		return "Moving Spiky Ceiling";
	case 88:
		return "Shootable Bell";
	case 93:
		return "Clickable Switch";
	case 95:
		return "Air Fan";
	case 96:
		return "Swinging Box or Spiky Ball";
	case 103:
		return "Small Switch";
	case 104:
		return "Above Water or Underwater Switch";
	case 105:
		return "Underwater Switch";
	case 106:
		return "Door 1";
	case 107:
		return "Door 2";
	case 108:
		return "Door 3";
	case 109:
		return "Door 4";
	case 110:
		return "Door 5";
	case 113:
		return "Openable Bridge";
	case 114:
		return "Trapdoor 1";
	case 115:
		return "Trapdoor 2";
	case 117:
		return "Bridge (Flat)";
	case 118:
		return "Bridge (Tilt 1)";
	case 119:
		return "Bridge (Tilt 2)";
	case 120:
		return "Secret 1";
	case 121:
		return "Secret 2";
	case 133:
		return "Secret 3";
	case 134:
		return "Minimap";
	case 152:
		return "Flare";
	case 153:
		return "Sunglasses";
	case 154:
		return "Portable CD Player";
	case 155:
		return "Direction Keys";
	case 157:
		return "Pistols";
	case 158:
		return "Shotgun";
	case 159:
		return "Automatic Pistols";
	case 160:
		return "Uzis";
	case 161:
		return "Harpoon Gun";
	case 162:
		return "M16";
	case 163:
		return "Grenade Launcher";
	case 164:
		return "Pistol Ammo";
	case 165:
		return "Shotgun Ammo";
	case 170:
		return "Grenade Ammo";
	case 171:
		return "Small Medipack";
	case 172:
		return "Large Medipack";
	case 173:
		return "Flare Box";
	case 178:
		return "Puzzle 1";
	case 179:
		return "Puzzle 2";
	case 180:
		return "Puzzle 3";
	case 181:
		return "Puzzle 4";
	case 182:
		return "Slot 1 Empty";
	case 183:
		return "Slot 2 Empty";
	case 184:
		return "Slot 3 Empty";
	case 185:
		return "Slot 4 Empty";
	case 186:
		return "Slot 1 Full";
	case 187:
		return "Slot 2 Full";
	case 188:
		return "Slot 3 Full";
	case 189:
		return "Slot 4 Full";
	case 193:
		return "Key 1 (Sprite)";
	case 194:
		return "Key 2 (Sprite)";
	case 195:
		return "Key 3 (Sprite)";
	case 196:
		return "Key 4 (Sprite)";
	case 197:
		return "Key 1";
	case 198:
		return "Key 2";
	case 199:
		return "Key 3";
	case 200:
		return "Key 4";
	case 201:
		return "Lock 1";
	case 202:
		return "Lock 2";
	case 203:
		return "Lock 3";
	case 204:
		return "Lock 4";
	case 209:
		return "Dragon Explosion 1";
	case 210:
		return "Dragon Explosion 2";
	case 211:
		return "Dragon Explosion 3";
	case 212:
		return "Alarm";
	case 217:
		return "Placeholder";
	case 218:
		return "Dragon Bones (Front)";
	case 219:
		return "Dragon Bones (Back)";
	case 223:
		return "Menu Background";
	case 229:
		return "Grenade Blast (Sprite)";
	case 230:
		return "Splash (Sprite)";
	case 231:
		return "Bubbles (Sprite)";
	case 233:
		return "Blood (Sprite)";
	case 235:
		return "Flare Burning";
	case 238:
		return "Bullet Ricochet (Sprite)";
	case 240:
		return "Gunflare";
	case 241:
		return "Gunflare (Rifle)";
	case 243:
		return "Camera Target";
	case 248:
		return "Grenade (Projectile)";
	case 249:
		return "Harpoon (Projectile)";
	case 253:
		return "Flame Emitter";
	case 254:
		return "Skybox";
	case 257:
		return "Door Bell";
	case 258:
		return "Alarm Bell";
	case 259:
		return "Helicopter";
	case 260:
		return "Winston";
	case 262:
		return "Lara Cutscene Placement";
	case 263:
		return "TR2 Ending Cutscene Animation";
	default:
		return String("MovableInfo_") + itos(p_type_info_id);
	}
}

static String get_type_info_name(uint32_t p_type_info_id, TRLevelFormat p_level_format) {
	switch(p_level_format) {
	case TRLevelFormat::TR1_PC:
		return get_tr1_type_info_name(p_type_info_id);
	case TRLevelFormat::TR2_PC:
		return get_tr2_type_info_name(p_type_info_id);
	default:
		return String("MovableInfo_") + itos(p_type_info_id);
	}
}

static String get_lara_bone_name(uint32_t p_bone_id) {
	switch (p_bone_id) {
		case 0:
			return "hips";
		case 1:
			return "left_upper_leg";
		case 2:
			return "left_lower_leg";
		case 3:
			return "left_Foot";
		case 4:
			return "right_upper_leg";
		case 5:
			return "right_lower_leg";
		case 6:
			return "right_foot";
		case 7:
			return "chest";
		case 8:
			return "right_upper_arm";
		case 9:
			return "right_lower_arm";
		case 10:
			return "right_hand";
		case 11:
			return "left_upper_arm";
		case 12:
			return "left_lower_arm";
		case 13:
			return "left_hand";
		case 14:
			return "head";
	}
	return String("bone_") + itos(p_bone_id);
}

static String get_tr1_bone_name(uint32_t p_type_info_id, uint32_t p_bone_id) {
	switch (p_type_info_id) {
	case 0:
		get_lara_bone_name(p_bone_id);
	case 1:
		get_lara_bone_name(p_bone_id);
	case 2:
		get_lara_bone_name(p_bone_id);
	case 3:
		get_lara_bone_name(p_bone_id);
	case 4:
		get_lara_bone_name(p_bone_id);
	case 5:
		get_lara_bone_name(p_bone_id);
	}
	return String("bone_") + itos(p_bone_id);
}

static String get_tr2_bone_name(uint32_t p_type_info_id, uint32_t p_bone_id) {
	switch (p_type_info_id) {
	case 0:
		get_lara_bone_name(p_bone_id);
	case 1:
		get_lara_bone_name(p_bone_id);
	case 3:
		get_lara_bone_name(p_bone_id);
	case 4:
		get_lara_bone_name(p_bone_id);
	case 5:
		get_lara_bone_name(p_bone_id);
	case 6:
		get_lara_bone_name(p_bone_id);
	case 7:
		get_lara_bone_name(p_bone_id);
	case 8:
		get_lara_bone_name(p_bone_id);
	case 9:
		get_lara_bone_name(p_bone_id);
	case 11:
		get_lara_bone_name(p_bone_id);
	case 12:
		get_lara_bone_name(p_bone_id);
	}
	return String("bone_") + itos(p_bone_id);
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

static bool is_tr1_animation_looping(uint32_t p_type_info_id, uint32_t p_animation_id) {
	switch (p_type_info_id) {
	case 0:
		switch (p_animation_id) {
		case 0:
			return true;
		case 1:
			return true;
		case 11:
			return true;
		case 12:
			return true;
		case 13:
			return true;
		}
	}
	return false;
}

static bool is_tr_animation_looping(uint32_t p_type_info_id, uint32_t p_animation_id) {
	return is_tr1_animation_looping(p_type_info_id, p_animation_id);
}

static String get_tr1_animation_name(uint32_t p_type_info_id, uint32_t p_animation_id) {
	switch (p_type_info_id) {
	case 0:
		switch (p_animation_id) {
		case 0:
			return "Run";
		case 1:
			return "Walk Forward";
		case 2:
			return "Walk Stop Right";
		case 3:
			return "Walk Stop Left";
		case 4:
			return "Walk To Run Right";
		case 5:
			return "Walk To Run Left";
		case 6:
			return "Run Start";
		case 7:
			return "Run To Walk Right";
		case 8:
			return "Run To Stand Left";
		case 9:
			return "Run To Walk Left";
		case 10:
			return "Run To Stand Right";
		case 11:
			return "Stand Still";
		case 12:
			return "Turn Right Slow";
		case 13:
			return "Turn Left Slow";
		case 14:
			return "Jump Forward Land Start (Unused)";
		case 15:
			return "Jump Forward Land End (Unused)";
		case 16:
			return "Run Jump Right Start";
		case 17:
			return "Run Jump Right Continue";
		case 18:
			return "Run Jump Left Start";
		case 19:
			return "Run Jump Left Continue";
		case 20:
			return "Walk Forward Start";
		case 21:
			return "Walk Forward Start Continue";
		case 22:
			return "Jump Forward to Freefall";
		case 23:
			return "Freefall";
		case 24:
			return "Freefall Land";
		case 25:
			return "Freefall Land Death";
		case 26:
			return "Stand To Jump Up";
		case 27:
			return "Stand To Jump Up Continue";
		case 28:
			return "Jump Up";
		case 29:
			return "Jump Up To Hang (Unused)";
		}
	}
	return String("Animation_") + itos(p_animation_id);
}

static String get_animation_name(uint32_t p_type_info_id, uint32_t p_animation_id) {
	return get_tr1_animation_name(p_type_info_id, p_animation_id);
}