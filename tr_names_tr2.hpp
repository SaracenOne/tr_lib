#pragma once
#include "tr_level.hpp"
#include "tr_names.hpp"

static String get_tr2_type_info_name(uint32_t p_type_info_id) {
	switch (p_type_info_id) {
	case 0:
		return "Lara";
	case 1:
		return "Lara Pistols Animations";
	case 2:
		return "Lara's Hair";
	case 3:
		return "Lara Shotgun Animations";
	case 4:
		return "Lara Automatic Pistols Animations";
	case 5:
		return "Lara Uzis Animations";
	case 6:
		return "Lara M16 Animations";
	case 7:
		return "Lara Grenade Launcher Animations";
	case 8:
		return "Lara Harpoon Gun Animations";
	case 9:
		return "Lara Flare Animations";
	case 11:
		return "Lara Boat Animations";
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

static String get_tr2_bone_name(uint32_t p_type_info_id, uint32_t p_bone_id) {
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
	case 11:
		return get_lara_bone_name(p_bone_id);
	case 12:
		return get_lara_bone_name(p_bone_id);
	}
	return String("bone_") + itos(p_bone_id);
}

static String get_tr2_animation_name(size_t p_type_info_id, size_t p_animation_id) {
	switch (p_type_info_id) {
		case 0: {
			return get_lara_animation_name(p_animation_id, TR2_PC);
		} break;
	}
	return String("animation_") + itos(p_animation_id);
}