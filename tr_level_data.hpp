#pragma once

#include "tr_module_extension_abstraction_layer.hpp"

#include "tr_types.h"
#include "tr_file_parser.hpp"

#ifdef IS_MODULE
#include "scene/3d/node_3d.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#else 
using namespace godot;
#include <godot_cpp/classes/node3D.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>
#endif

class TRLevelData : public Resource {
	GDCLASS(TRLevelData, Resource);
public:
	TRLevelFormat format;
	TRTextureType texture_type;
	Vector<PackedByteArray> level_textures;
	Vector<PackedByteArray> entity_textures;
	Vector<TRColor3> palette;
	Vector<TRRoom> rooms;
	PackedByteArray floor_data;
	TRTypes types;
	Vector<TREntity> entities;
	Vector<uint16_t> sound_map;
	Vector<TRSoundInfo> sound_infos;
	PackedByteArray sound_buffer;
	PackedInt32Array sound_indices;
};