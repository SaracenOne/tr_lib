#include "tr_level.hpp"

#include <core/io/stream_peer_gzip.h>
#include "core/math/math_funcs.h"
#include <editor/editor_file_system.h> 
#include "tr_godot_conversion.hpp"


#ifdef IS_MODULE
using namespace godot;
#endif

// TR to Godot directional mappings:
// Z+ = North
// Z- = South
// X+ = East
// X- = West

void TRLevel::_bind_methods() {
	ClassDB::bind_method("load_level", &TRLevel::load_level);

	ClassDB::bind_method("set_level_path", &TRLevel::set_level_path);
	ClassDB::bind_method("get_level_path", &TRLevel::get_level_path);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "level_path", PROPERTY_HINT_FILE, "*.phd,*.tr2,*.tr4"), "set_level_path", "get_level_path");
}

TRLevel::TRLevel() {
	level_path = "";
}

TRLevel::~TRLevel() {
}

TRVertex read_tr_vertex(Ref<TRFileAccess> p_file) {
	TRVertex vertex;

	vertex.x = p_file->get_s16();
	vertex.y = p_file->get_s16();
	vertex.z = p_file->get_s16();

	return vertex;
}

TRPos read_tr_pos(Ref<TRFileAccess> p_file) {
	TRPos pos;

	pos.x = p_file->get_s32();
	pos.y = p_file->get_s32();
	pos.z = p_file->get_s32();

	return pos;
}

TRRot read_tr_rot(Ref<TRFileAccess> p_file) {
	TRRot rot;

	rot.x = p_file->get_s16();
	rot.y = p_file->get_s16();
	rot.z = p_file->get_s16();

	return rot;
}

TRTransform read_tr_transform(Ref<TRFileAccess> p_file) {
	TRTransform transform;

	transform.pos = read_tr_pos(p_file);
	transform.rot = read_tr_rot(p_file);

	return transform;
}

Color tr_color_shade_to_godot_color(int16_t p_lighting) {
	TRColor3 color3;
	color3.r = ((p_lighting & 0x7C00) >> 10);
	color3.g = ((p_lighting & 0x03E0) >> 5);
	color3.b = ((p_lighting & 0x001F));

	Color godot_color;
	godot_color.r = (float)color3.r / (float)0xff;
	godot_color.g = (float)color3.g / (float)0xff;
	godot_color.b = (float)color3.b / (float)0xff;
	godot_color.a = 1.0f;

	return godot_color;
}

Color tr_monochrome_shade_to_godot_color(int16_t p_lighting) {
	Color godot_color;
	godot_color.r = 1.0f - ((float)p_lighting / (float)0x1fff);
	godot_color.g = 1.0f - ((float)p_lighting / (float)0x1fff);
	godot_color.b = 1.0f - ((float)p_lighting / (float)0x1fff);
	godot_color.a = 1.0f;

	return godot_color;
}

TRRoomVertex read_tr_room_vertex(Ref<TRFileAccess> p_file, TRLevelFormat p_format) {
	TRRoomVertex room_vertex;

	room_vertex.vertex = read_tr_vertex(p_file);
	uint16_t first_shade_value = p_file->get_s16();
	room_vertex.attributes = 0;
	uint16_t second_shade_value = first_shade_value;
	if (p_format == TR2_PC || p_format == TR3_PC || p_format == TR4_PC) {
		room_vertex.attributes  = p_file->get_u16();
		second_shade_value = p_file->get_s16();
	}

	if (p_format == TR3_PC || p_format == TR4_PC) {
		room_vertex.color = tr_color_shade_to_godot_color(second_shade_value) * 8.0;
		room_vertex.color2 = room_vertex.color;
	} else {
		room_vertex.color = tr_monochrome_shade_to_godot_color(first_shade_value);
		if (p_format == TR2_PC) {
			room_vertex.color2 = tr_monochrome_shade_to_godot_color(second_shade_value);
		} else {
			room_vertex.color2 = room_vertex.color;
		}
	}

	return room_vertex;
}

TRFaceTriangle read_tr_face_triangle(Ref<TRFileAccess> p_file) {
	TRFaceTriangle face_triangle;

	for (int32_t i = 0; i < 3; i++) {
		face_triangle.indices[i] = p_file->get_s16();
	}
	face_triangle.tex_info_id = p_file->get_s16();

	return face_triangle;
}

TRFaceQuad read_tr_face_quad(Ref<TRFileAccess> p_file) {
	TRFaceQuad face_quad;

	for (int32_t i = 0; i < 4; i++) {
		face_quad.indices[i] = p_file->get_s16();
	}
	face_quad.tex_info_id = p_file->get_s16();

	return face_quad;
}

TRFaceTriangle read_tr_mesh_face_triangle(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	TRFaceTriangle face_triangle;

	for (int32_t i = 0; i < 3; i++) {
		face_triangle.indices[i] = p_file->get_s16();
	}
	face_triangle.tex_info_id = p_file->get_s16();
	if (p_level_format == TR4_PC) {
		face_triangle.effect_info = p_file->get_s16();
	}

	return face_triangle;
}

TRFaceQuad read_tr_mesh_face_quad(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	TRFaceQuad face_quad;

	for (int32_t i = 0; i < 4; i++) {
		face_quad.indices[i] = p_file->get_s16();
	}
	face_quad.tex_info_id = p_file->get_s16();
	if (p_level_format == TR4_PC) {
		face_quad.effect_info = p_file->get_s16();
	}

	return face_quad;
}

TRRoomSprite read_tr_room_sprite(Ref<TRFileAccess> p_file) {
	TRRoomSprite room_sprite;

	room_sprite.vertex = p_file->get_s16();
	room_sprite.texture = p_file->get_s16();

	return room_sprite;
}

TRRoomData read_tr_room_data(Ref<TRFileAccess> p_file, TRLevelFormat p_format) {
	TRRoomData room_data;

	// Vertex buffer
	room_data.room_vertex_count = p_file->get_s16();
	room_data.room_vertices.resize(room_data.room_vertex_count);
	for (int32_t i = 0; i < room_data.room_vertex_count; i++) {
		room_data.room_vertices.set(i, read_tr_room_vertex(p_file, p_format));
	}

	// Quad buffer
	room_data.room_quad_count = p_file->get_s16();
	room_data.room_quads.resize(room_data.room_quad_count);
	for (int32_t i = 0; i < room_data.room_quad_count; i++) {
		room_data.room_quads.set(i, read_tr_face_quad(p_file));
	}

	// Triangle buffer
	room_data.room_triangle_count = p_file->get_s16();
	room_data.room_triangles.resize(room_data.room_triangle_count);
	for (int32_t i = 0; i < room_data.room_triangle_count; i++) {
		room_data.room_triangles.set(i, read_tr_face_triangle(p_file));
	}

	// Sprite buffer
	room_data.room_sprite_count = p_file->get_s16();
	room_data.room_sprites.resize(room_data.room_sprite_count);
	for (int32_t i = 0; i < room_data.room_sprite_count; i++) {
		room_data.room_sprites.set(i, read_tr_room_sprite(p_file));
	}

	return room_data;
}

TRRoomSector read_tr_room_sector(Ref<TRFileAccess> p_file) {
	TRRoomSector room_sector;

	room_sector.floor_data_index = p_file->get_u16();
	room_sector.box_index = p_file->get_s16();
	room_sector.room_below = p_file->get_u8();
	room_sector.floor = p_file->get_s8();
	room_sector.room_above = p_file->get_u8();
	room_sector.ceiling = p_file->get_s8();

	return room_sector;
}

TRColor3 read_tr_color3(Ref<TRFileAccess> p_file) {
	TRColor3 color_3;

	color_3.r = p_file->get_u8();
	color_3.g = p_file->get_u8();
	color_3.b = p_file->get_u8();

	return color_3;
}

TRColor4 read_tr_color4_argb(Ref<TRFileAccess> p_file) {
	TRColor4 color_4;

	color_4.a = p_file->get_u8();
	color_4.r = p_file->get_u8();
	color_4.g = p_file->get_u8();
	color_4.b = p_file->get_u8();

	return color_4;
}

TRRoomLight read_tr_light(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	TRRoomLight room_light;

	// Position
	room_light.pos = read_tr_pos(p_file);

	if (p_level_format == TR3_PC || p_level_format == TR4_PC) {
		room_light.color = read_tr_color3(p_file);
	}

	if (p_level_format == TR3_PC || p_level_format == TR4_PC) {
		// Light Type
		room_light.light_type = p_file->get_u8();

		if (p_level_format == TR3_PC) {
			if (room_light.light_type == 1) {
				room_light.intensity_alt = room_light.intensity = p_file->get_u32();
				room_light.fade_alt = room_light.fade = p_file->get_u32();
			} else {
				uint16_t nx = p_file->get_u16();
				uint16_t ny = p_file->get_u16();
				uint16_t nz = p_file->get_u16();
				uint16_t unk = p_file->get_u16();
			}
		} else {
			uint8_t unk = p_file->get_u8();
			uint8_t intensity = p_file->get_u8();

			float in = p_file->get_float();
			float out = p_file->get_float();
			float length = p_file->get_float();
			float cutoff = p_file->get_float();

			float dx = p_file->get_float();
			float dy = p_file->get_float();
			float dz = p_file->get_float();
		}

	} else if (p_level_format == TR1_PC || p_level_format == TR2_PC) {
		// Intensity
		room_light.intensity = p_file->get_u16();

		if (p_level_format == TR2_PC) {
			room_light.intensity_alt = p_file->get_u16();
		} else {
			room_light.intensity_alt = room_light.intensity;
		}
		// Fade
		room_light.fade = p_file->get_u32();
		if (p_level_format == TR2_PC) {
			room_light.fade_alt = p_file->get_u32();
		} else {
			room_light.fade_alt = room_light.fade;
		}
	}

	return room_light;
}

TRRoomStaticMesh read_tr_room_static_mesh(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	TRRoomStaticMesh room_static_mesh;

	room_static_mesh.pos = read_tr_pos(p_file);
	room_static_mesh.rotation = static_cast<tr_angle>(p_file->get_s16());
	uint16_t intensity_or_color = p_file->get_u16();
	uint16_t intensity_or_color_2 = intensity_or_color;

	if (p_level_format == TR2_PC || p_level_format == TR3_PC || p_level_format == TR4_PC) {
		intensity_or_color_2 = p_file->get_u16();
	}

	if (p_level_format == TR3_PC || p_level_format == TR4_PC) {
		room_static_mesh.color_1 = tr_color_shade_to_godot_color(intensity_or_color);
		room_static_mesh.color_2 = tr_color_shade_to_godot_color(intensity_or_color_2);
	} else {
		room_static_mesh.color_1 = tr_monochrome_shade_to_godot_color(intensity_or_color);
		room_static_mesh.color_2 = tr_monochrome_shade_to_godot_color(intensity_or_color_2);
	}

	room_static_mesh.mesh_id = p_file->get_u16();

	return room_static_mesh;
}

TRRoomPortal read_tr_room_portal(Ref<TRFileAccess> p_file) {
	TRRoomPortal room_portal;

	room_portal.adjoining_room = p_file->get_u16();
	room_portal.normal = read_tr_vertex(p_file);
	for (int32_t i = 0; i < 4; i++) {
		room_portal.vertices[i] = read_tr_vertex(p_file);
	}

	return room_portal;
}

TRRoomInfo read_tr_room_info(Ref<TRFileAccess> p_file) {
	TRRoomInfo room_info;

	room_info.x = p_file->get_s32();
	room_info.z = p_file->get_s32();

	room_info.y_bottom = p_file->get_s32();
	room_info.y_top = p_file->get_s32();

	return room_info;
}

TRRoom read_tr_room(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	TRRoom room;

	uint32_t rt_position = p_file->get_position();

	room.info = read_tr_room_info(p_file);

	uint32_t rt_position2 = p_file->get_position();

	// Room's mesh data parsing
	uint32_t data_count = p_file->get_u32();
	uint32_t offset = data_count * sizeof(uint16_t);
	uint32_t data_end = p_file->get_position() + (data_count * sizeof(uint16_t));

	room.data = read_tr_room_data(p_file, p_level_format);

	p_file->seek(data_end);

	int16_t portal_count = p_file->get_s16();
	room.portal_count = portal_count;
	room.portals.resize(room.portal_count);
	for (int32_t i = 0; i < room.portal_count; i++) {
		room.portals.set(i, read_tr_room_portal(p_file));
	}

	room.sector_count_x = p_file->get_u16();
	room.sector_count_z = p_file->get_u16();

	uint32_t floor_sectors_total = room.sector_count_x * room.sector_count_z;
	room.sectors.resize(floor_sectors_total);
	for (uint32_t i = 0; i < floor_sectors_total; i++) {
		room.sectors.set(i, read_tr_room_sector(p_file));
	}

	if (p_level_format == TR4_PC) {
		TRColor4 room_color = read_tr_color4_argb(p_file);
		room.ambient_light.set_r8(room_color.r);
		room.ambient_light.set_g8(room_color.g);
		room.ambient_light.set_b8(room_color.b);
		room.ambient_light.set_a8(room_color.a);

		room.ambient_light_alt = room.ambient_light;
	} else {
		if (p_level_format == TR2_PC || p_level_format == TR3_PC) {
			room.ambient_light = tr_monochrome_shade_to_godot_color(p_file->get_u16());
			room.ambient_light_alt = tr_monochrome_shade_to_godot_color(p_file->get_u16());
		} else {
			room.ambient_light = tr_monochrome_shade_to_godot_color(p_file->get_u16());
			room.ambient_light_alt = room.ambient_light;
		}
	}

	if (p_level_format == TR2_PC) {
		room.light_mode = p_file->get_s16();
	} else {
		room.light_mode = 0;
	}

	room.light_count = p_file->get_u16();
	room.lights.resize(room.light_count);
	for (int32_t i = 0; i < room.light_count; i++) {
		room.lights.set(i, read_tr_light(p_file, p_level_format));
	}

	room.room_static_mesh_count = p_file->get_u16();
	room.room_static_meshes.resize(room.room_static_mesh_count);
	for (int32_t i = 0; i < room.room_static_mesh_count; i++) {
		room.room_static_meshes.set(i, read_tr_room_static_mesh(p_file, p_level_format));
	}

	room.alternative_room = p_file->get_s16();
	room.room_flags = p_file->get_u16();

	// If the room is underwater, change the color
	if (p_level_format == TR1_PC || p_level_format == TR2_PC) {
		if (room.room_flags & 1) {
			for (int32_t i = 0; i < room.data.room_vertex_count; i++) {
				TRRoomVertex room_vertex = room.data.room_vertices.get(i);
				room_vertex.color *= Color(0.0, 0.5, 1.0);
				room.data.room_vertices.set(i, room_vertex);
			}
		}
	}

	if (p_level_format == TR3_PC || p_level_format == TR4_PC) {
		room.water_scheme = p_file->get_u8();
		room.reverb_info = p_file->get_u8();

		if (p_level_format == TR4_PC) {
			room.alternate_group = p_file->get_u8();
		} else {
			uint8_t _filler = p_file->get_u8();
		}
	}

	return room;
}

Vector<PackedByteArray> read_tr_texture_pages_8(Ref<TRFileAccess> p_file, uint32_t p_count) {
	Vector<PackedByteArray> textures;

	for (uint32_t i = 0; i < p_count; i++) {
		uint64_t texture_buffer_size = TR_TEXTILE_SIZE * TR_TEXTILE_SIZE;
		PackedByteArray texture_buf = p_file->get_buffer(texture_buffer_size);

		textures.push_back(texture_buf);
		ERR_FAIL_COND_V(texture_buffer_size != texture_buf.size(), Vector<PackedByteArray>());
	}

	return textures;
}

Vector<PackedByteArray> read_tr_texture_pages_16(Ref<TRFileAccess> p_file, uint32_t p_count) {
	Vector<PackedByteArray> textures;

	for (uint32_t i = 0; i < p_count; i++) {
		uint64_t texture_buffer_size = TR_TEXTILE_SIZE * TR_TEXTILE_SIZE * sizeof(uint16_t);
		PackedByteArray texture_buf = p_file->get_buffer(texture_buffer_size);
		textures.push_back(texture_buf);
		ERR_FAIL_COND_V(texture_buffer_size != texture_buf.size(), Vector<PackedByteArray>());
	}

	return textures;
}

Vector<PackedByteArray> read_tr_texture_pages_32(Ref<TRFileAccess> p_file, uint32_t p_count) {
	Vector<PackedByteArray> textures;

	for (uint32_t i = 0; i < p_count; i++) {
		uint64_t texture_buffer_size = TR_TEXTILE_SIZE * TR_TEXTILE_SIZE * sizeof(uint32_t);
		PackedByteArray texture_buf = p_file->get_buffer(texture_buffer_size);
		textures.push_back(texture_buf);
		ERR_FAIL_COND_V(texture_buffer_size != texture_buf.size(), Vector<PackedByteArray>());
	}

	return textures;
}

Vector<PackedByteArray> read_tr_texture_pages(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	uint32_t texture_page_count = p_file->get_u32();

	Vector<PackedByteArray> textures_8 = read_tr_texture_pages_8(p_file, texture_page_count);
	if (p_level_format == TR2_PC || p_level_format == TR3_PC) {
		Vector<PackedByteArray> textures_16 = read_tr_texture_pages_16(p_file, texture_page_count);
	}

	return textures_8;
}

Vector<TRRoom> read_tr_rooms(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	uint16_t room_count = p_file->get_u16();
	Vector<TRRoom> rooms;
	rooms.resize(room_count);
	for (uint16_t i = 0; i < room_count; i++) {
		rooms.set(i, read_tr_room(p_file, p_level_format));
	}

	return rooms;
}

PackedByteArray read_tr_floor_data(Ref<TRFileAccess> p_file) {
	uint32_t floor_data_size = p_file->get_u32();
	return p_file->get_buffer(floor_data_size * sizeof(uint16_t));
}

TRAnimation read_tr_animation(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	TRAnimation animation;

	animation.frame_offset = p_file->get_s32();
	animation.frame_skip = p_file->get_u8();
	animation.frame_size = p_file->get_u8();
	animation.current_animation_state = p_file->get_s16();
	animation.velocity = p_file->get_s32();
	animation.acceleration = p_file->get_s32();

	if (p_level_format == TR4_PC) {
		animation.lateral_velocity = p_file->get_s32();
		animation.lateral_acceleration = p_file->get_s32();
	} else {
		animation.lateral_velocity = 0;
		animation.lateral_acceleration = 0;
	}

	animation.frame_base = p_file->get_s16();
	animation.frame_end = p_file->get_s16();
	animation.next_animation_number = p_file->get_s16();
	animation.next_frame_number = p_file->get_s16();
	animation.number_state_changes = p_file->get_s16();
	animation.state_change_index = p_file->get_s16();
	animation.number_commands = p_file->get_s16();
	animation.command_index = p_file->get_s16();

	return animation;
}

TRBoundingBox read_tr_bounding_box(Ref<TRFileAccess> p_file) {
	TRBoundingBox bounding_box;

	bounding_box.x_min = p_file->get_s16();
	bounding_box.x_max = p_file->get_s16();
	bounding_box.y_min = p_file->get_s16();
	bounding_box.y_max = p_file->get_s16();
	bounding_box.z_min = p_file->get_s16();
	bounding_box.z_max = p_file->get_s16();

	return bounding_box;
}

TRAnimationStateChange read_tr_animation_state_change(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	TRAnimationStateChange state_change;

	state_change.target_animation_state = p_file->get_s16();
	state_change.number_dispatches = p_file->get_s16();
	state_change.dispatch_index = p_file->get_s16();

	return state_change;
}

TRAnimationDispatch read_tr_animation_dispatch(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	TRAnimationDispatch dispatch;

	dispatch.start_frame = p_file->get_s16();
	dispatch.end_frame = p_file->get_s16();
	dispatch.target_animation_number = p_file->get_s16();
	dispatch.target_frame_number = p_file->get_s16();

	return dispatch;
}

TRAnimationCommand read_tr_animation_command(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	TRAnimationCommand command;

	command.command = p_file->get_s16();

	return command;
}


TRMesh read_tr_mesh(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	TRMesh tr_mesh;
	tr_mesh.center = read_tr_vertex(p_file);
	tr_mesh.collision_radius = p_file->get_s32();

	tr_mesh.vertex_count = p_file->get_s16();
	tr_mesh.vertices.resize(tr_mesh.vertex_count);
	for (int32_t i = 0; i < tr_mesh.vertex_count; i++) {
		TRVertex vertex = read_tr_vertex(p_file);
		tr_mesh.vertices.set(i, vertex);
	}

	tr_mesh.normal_count = p_file->get_s16();
	int16_t abs_num_normals = abs(tr_mesh.normal_count);

	ERR_FAIL_COND_V(abs_num_normals != tr_mesh.vertex_count, tr_mesh);
	
	if (tr_mesh.normal_count > 0) {
		tr_mesh.normals.resize(abs_num_normals);
		for (int32_t i = 0; i < abs_num_normals; i++) {
			TRVertex normal = read_tr_vertex(p_file);
			tr_mesh.normals.set(i, normal);
		}
	} else {
		tr_mesh.colors.resize(abs_num_normals);
		for (int32_t i = 0; i < abs_num_normals; i++) {
			int16_t color = p_file->get_s16();
			tr_mesh.colors.set(i, color);
		}
	}

	// Texture Quad buffer
	tr_mesh.texture_quads_count = p_file->get_s16();
	tr_mesh.texture_quads.resize(tr_mesh.texture_quads_count);
	for (int32_t i = 0; i < tr_mesh.texture_quads_count; i++) {
		tr_mesh.texture_quads.set(i, read_tr_mesh_face_quad(p_file, p_level_format));
	}

	// Texture Triangle buffer
	tr_mesh.texture_triangles_count = p_file->get_s16();
	tr_mesh.texture_triangles.resize(tr_mesh.texture_triangles_count);
	for (int32_t i = 0; i < tr_mesh.texture_triangles_count; i++) {
		tr_mesh.texture_triangles.set(i, read_tr_mesh_face_triangle(p_file, p_level_format));
	}

	if (p_level_format == TR1_PC || p_level_format == TR2_PC || p_level_format == TR3_PC) {
		// Color Quad buffer
		tr_mesh.color_quads_count = p_file->get_s16();
		tr_mesh.color_quads.resize(tr_mesh.color_quads_count);
		for (int32_t i = 0; i < tr_mesh.color_quads_count; i++) {
			tr_mesh.color_quads.set(i, read_tr_mesh_face_quad(p_file, p_level_format));
		}

		// Color Triangle buffer
		tr_mesh.color_triangles_count = p_file->get_s16();
		tr_mesh.color_triangles.resize(tr_mesh.color_triangles_count);
		for (int32_t i = 0; i < tr_mesh.color_triangles_count; i++) {
			tr_mesh.color_triangles.set(i, read_tr_mesh_face_triangle(p_file, p_level_format));
		}
	} else {
		tr_mesh.color_quads_count = 0;
		tr_mesh.color_triangles_count = 0;
	}

	return tr_mesh;
}

Vector<TRTextureInfo> read_tr_texture_infos(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	int32_t texture_count = p_file->get_s32();
	Vector<TRTextureInfo> texture_infos;
	ERR_FAIL_COND_V(texture_count > 8192, Vector<TRTextureInfo>());

	for (int32_t i = 0; i < texture_count; i++) {
		TRTextureInfo texture_info;

		texture_info.draw_type = p_file->get_s16();

		uint16_t texture_page_and_flag = p_file->get_s16();
		texture_info.texture_page = texture_page_and_flag & 0x7fff;

		if (p_level_format == TR4_PC) {
			texture_info.extra_flags = p_file->get_s16();
		} else {
			texture_info.extra_flags = 0;
		}

		for (int32_t j = 0; j < 4; j++) {
			texture_info.uv[j].u = p_file->get_s16();
			texture_info.uv[j].v = p_file->get_s16();
		}

		if (p_level_format == TR4_PC) {
			texture_info.original_u = p_file->get_u32();
			texture_info.original_v = p_file->get_u32();
			texture_info.width = p_file->get_u32();
			texture_info.height = p_file->get_u32();
		} else {
			texture_info.original_u = 0;
			texture_info.original_v = 0;
			texture_info.width = 0;
			texture_info.height = 0;
		}

		texture_infos.push_back(texture_info);
	}

	return texture_infos;
}

TRTypes read_tr_types(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	TRTypes tr_types;

	// Mesh Buffer
	int32_t mesh_data_buffer_size = p_file->get_s32();
	// Save position of the start of mesh buffer
	int32_t mesh_buffer_pos = p_file->get_position();
	// Skip the buffer
	p_file->seek(p_file->get_position() + mesh_data_buffer_size * sizeof(uint16_t));

	// Mesh Pointers
	int32_t mesh_ptr_count = p_file->get_s32();
	PackedInt32Array mesh_ptr_buffer = p_file->get_buffer_int32(mesh_ptr_count);

	// Save position of the end of the mesh pointer buffer
	int32_t mesh_ptr_buffer_end = p_file->get_position();

	for (int32_t i = 0; i < mesh_ptr_count; i++) {
		int32_t ptr = mesh_ptr_buffer[i];
		p_file->seek(mesh_buffer_pos + ptr);

		TRMesh mesh = read_tr_mesh(p_file, p_level_format);
		tr_types.meshes.push_back(mesh);
	}

	p_file->seek(mesh_ptr_buffer_end);

	// Animations
	int32_t anim_count = p_file->get_s32();
	for (int32_t i = 0; i < anim_count; i++) {
		TRAnimation animation = read_tr_animation(p_file, p_level_format);
		tr_types.animations.push_back(animation);
	}

	// Animation State Changes
	int32_t animation_change_count = p_file->get_s32();
	for (int32_t i = 0; i < animation_change_count; i++) {
		TRAnimationStateChange state_change = read_tr_animation_state_change(p_file, p_level_format);
		tr_types.animation_state_changes.push_back(state_change);
	}

	// Animation Dispatches
	int32_t animation_dispatch_count = p_file->get_s32();
	for (int32_t i = 0; i < animation_dispatch_count; i++) {
		TRAnimationDispatch dispatch = read_tr_animation_dispatch(p_file, p_level_format);
		tr_types.animation_dispatches.push_back(dispatch);
	}

	// Animation Commands
	int32_t anim_command_count = p_file->get_s32();
	for (int32_t i = 0; i < anim_command_count; i++) {
		TRAnimationCommand command = read_tr_animation_command(p_file, p_level_format);
		tr_types.animation_commands.push_back(command);
	}

	// Mesh Tree Count
	int32_t mesh_tree_count = p_file->get_s32();
	tr_types.mesh_tree_buffer = p_file->get_buffer_int32(mesh_tree_count);

	// Animation Frame Count
	int32_t anim_frame_count = p_file->get_s32();
	PackedByteArray anim_frame_buffer = p_file->get_buffer(anim_frame_count * sizeof(uint16_t));

	// Moveable Info Count
	Vector<int32_t> id_list;
	Vector<TRMoveableInfo> moveable_infos;
	int32_t moveable_info_count = p_file->get_s32();
	for (int32_t i = 0; i < moveable_info_count; i++) {
		int32_t type_info_id = p_file->get_u32();

		TRMoveableInfo moveable_info;
		moveable_info.mesh_count = p_file->get_s16();
		moveable_info.mesh_index = p_file->get_s16();
		moveable_info.bone_index = p_file->get_s32();

		uint32_t tmp_anim_frames = p_file->get_s32();
		// TODO: set frame base
		moveable_info.animation_index = p_file->get_s16();

		if (!id_list.has(type_info_id)) {
			id_list.push_back(type_info_id);
			moveable_infos.push_back(moveable_info);
		}
	}

	// Calculate animation count (should work fine in retail games, but may not work for custom levels)
	for (int32_t i = 0; i < moveable_infos.size() - 1; i++) {
		TRMoveableInfo moveable_info = moveable_infos[i];

		if (moveable_infos[i].animation_index >= 0) {
			if (moveable_infos[i + 1].animation_index > moveable_infos[i].animation_index) {
				moveable_info.animation_count = moveable_infos[i + 1].animation_index - moveable_infos[i].animation_index;
			}
		}
		moveable_infos.set(i, moveable_info);
	}

	// Animation setup (we're skipping the last model because we don't know the amount of animations it has)
	for (int32_t i = 0; i < moveable_infos.size() - 1; i++) {
		TRMoveableInfo moveable_info = moveable_infos[i];

		for (int64_t animation_idx = 0; animation_idx < moveable_info.animation_count; animation_idx++) {
			if (moveable_info.animation_index + animation_idx >= tr_types.animations.size()) {
				continue;
			}

			TRAnimation animation = tr_types.animations[moveable_info.animation_index + animation_idx];
			ERR_FAIL_COND_V(animation.frame_skip <= 0, TRTypes());

			int32_t frame_count = (animation.frame_end - animation.frame_base) / animation.frame_skip;
			frame_count++;
			
			int32_t frame_ptr = animation.frame_offset;

			for (int64_t frame_idx = 0; frame_idx < frame_count; frame_idx++) {
				// This may be wrong

				if (animation.frame_size > 0) {
					frame_ptr = animation.frame_offset + ((animation.frame_size * sizeof(uint16_t) * frame_idx));
				}

				TRBoundingBox bounding_box;
				
				int16_t* bounds[] = { &bounding_box.x_min, &bounding_box.x_max, &bounding_box.y_min, &bounding_box.y_max, &bounding_box.z_min, &bounding_box.z_max };
				for (int32_t j = 0; j < sizeof(bounds) / sizeof(bounds[0]); j++) {
					if (frame_ptr + 2 >= anim_frame_buffer.size()) {
						continue;
					}
					uint8_t first = anim_frame_buffer[frame_ptr++];
					uint8_t second = anim_frame_buffer[frame_ptr++];

					*bounds[j] = (first | ((int16_t)second << 8));
				}

				TRPos pos;

				int32_t* coords[] = { &pos.x, &pos.y, &pos.z };
				for (int32_t j = 0; j < sizeof(coords) / sizeof(coords[0]); j++) {
					if (frame_ptr + 2 >= anim_frame_buffer.size()) {
						continue;
					}
					uint8_t first = anim_frame_buffer[frame_ptr++];
					uint8_t second = anim_frame_buffer[frame_ptr++];

					*coords[j] = (int16_t)(first | ((int16_t)second << 8));
				}

				int16_t num_rotations = moveable_info.mesh_count;
				if (p_level_format == TR1_PC) {
					if (frame_ptr + 2 >= anim_frame_buffer.size()) {
						continue;
					}
					uint8_t first = anim_frame_buffer[frame_ptr++];
					uint8_t second = anim_frame_buffer[frame_ptr++];

					num_rotations = (first) | ((int16_t)(second) << 8);
				}

				TRAnimFrame anim_frame;

				for (int32_t j = 0; j < num_rotations; j++) {
					TRTransform bone_transform;

					if (frame_ptr + 2 >= anim_frame_buffer.size()) {
						continue;
					}

					uint8_t rot_1 = anim_frame_buffer[frame_ptr++];
					uint8_t rot_2 = anim_frame_buffer[frame_ptr++];
					uint16_t rot_16_a = (rot_1) | ((uint16_t)(rot_2) << 8);

					uint16_t flags = (rot_16_a & 0xc000) >> 14;

					if (p_level_format == TR1_PC || flags == 0) {
						if (frame_ptr + 2 >= anim_frame_buffer.size()) {
							continue;
						}

						uint8_t rot_3 = anim_frame_buffer[frame_ptr++];
						uint8_t rot_4 = anim_frame_buffer[frame_ptr++];

						uint16_t rot_16_b = (rot_3) | ((uint16_t)(rot_4) << 8);

						uint32_t rot_32;
						if (p_level_format == TR2_PC || p_level_format == TR3_PC || p_level_format == TR4_PC) {
							rot_32 = rot_16_b | ((uint32_t)(rot_16_a) << 16);
						} else {
							rot_32 = rot_16_a | ((uint32_t)(rot_16_b) << 16);
						}

						TRRot rot;

						rot.y = (tr_angle)((((rot_32 >> 10) & 0x3ff) << 6));
						rot.x = (tr_angle)((((rot_32 >> 20) & 0x3ff) << 6));
						rot.z = (tr_angle)((((rot_32) & 0x3ff) << 6));

						bone_transform.rot = rot;
					} else {
						TRRot rot;

						rot.y = 0;
						rot.x = 0;
						rot.z = 0;

						if (p_level_format == TR4_PC) {
							switch (flags) {
								case 1:
									rot.x = (rot_16_a & 0xfff) << 4;
									break;
								case 2:
									rot.y = (rot_16_a & 0xfff) << 4;
									break;
								case 3:
									rot.z = (rot_16_a & 0xfff) << 4;
									break;
							}
						} else {
							switch (flags) {
								case 1:
									rot.x = (rot_16_a & 0x3ff) << 6;
									break;
								case 2:
									rot.y = (rot_16_a & 0x3ff) << 6;
									break;
								case 3:
									rot.z = (rot_16_a & 0x3ff) << 6;
									break;
							}
						}

						bone_transform.rot = rot;
					}

					if (j == 0) {
						bone_transform.pos = pos;
					}
					anim_frame.transforms.push_back(bone_transform);
				}
				anim_frame.bounding_box = bounding_box;

				animation.frames.push_back(anim_frame);
			}
			tr_types.animations.set(moveable_info.animation_index + animation_idx, animation);

			moveable_info.animations.push_back(animation);
		}
		moveable_infos.set(i, moveable_info);
	}

	for (int64_t i = 0; i < id_list.size(); i++) {
		tr_types.moveable_info_map[id_list[i]] = moveable_infos[i];
	}

	// TODO: Setup types

	// Statics Count
	HashMap<int32_t, TRStaticInfo > static_map;
	int32_t static_count = p_file->get_s32();
	for (int64_t i = 0; i < static_count; i++) {
		int32_t static_id = p_file->get_u32();
		
		TRStaticInfo static_info;

		static_info.mesh_number = p_file->get_u16();

		static_info.visibility_box = read_tr_bounding_box(p_file);
		static_info.collision_box = read_tr_bounding_box(p_file);

		static_info.flags = p_file->get_u16();

		tr_types.static_info_map[static_id] = static_info;
	}

	if (p_level_format != TR3_PC && p_level_format != TR4_PC) {
		tr_types.texture_infos = read_tr_texture_infos(p_file, p_level_format);
	}

	return tr_types;
}

TRSpriteInfo read_tr_sprite_info(Ref<TRFileAccess> p_file) {
	TRSpriteInfo sprite_info;

	sprite_info.texture_page = p_file->get_u16();
	sprite_info.offset = p_file->get_u16();
	sprite_info.width = p_file->get_u16();
	sprite_info.height = p_file->get_u16();

	sprite_info.x1 = p_file->get_s16();
	sprite_info.y1 = p_file->get_s16();
	sprite_info.x2 = p_file->get_s16();
	sprite_info.y2 = p_file->get_s16();

	return sprite_info;
}

void read_tr_sprites(Ref<TRFileAccess> p_file) {
	int32_t sprite_info_count = p_file->get_s32();
	ERR_FAIL_COND(sprite_info_count > 512);

	for (int32_t i = 0; i < sprite_info_count; i++) {
		TRSpriteInfo sprite_info = read_tr_sprite_info(p_file);
	}

	int32_t sprite_count = p_file->get_s32();
	for (int32_t i = 0; i < sprite_count; i++) {
		int32_t type_num = p_file->get_s32();

		if (type_num < 191) {
			int16_t unk = p_file->get_s16();
			int16_t mesh_index = p_file->get_s16();
		} else {
			int32_t static_num = type_num - 191;
			p_file->seek(p_file->get_position() + 2);

			int16_t mesh_number = p_file->get_s16();
		}
	}
}

TRGameVector read_tr_game_vector(Ref<TRFileAccess> p_file) {
	TRGameVector tr_game_vector;

	tr_game_vector.pos = read_tr_pos(p_file);
	tr_game_vector.room_number = p_file->get_s16();
	tr_game_vector.box_number = p_file->get_s16();

	return tr_game_vector;
}

TRObjectVector read_tr_type_vector(Ref<TRFileAccess> p_file) {
	TRObjectVector tr_type_vector;

	tr_type_vector.pos = read_tr_pos(p_file);
	tr_type_vector.data = p_file->get_s16();
	tr_type_vector.flags = p_file->get_s16();

	return tr_type_vector;
}

void read_tr_cameras(Ref<TRFileAccess> p_file) {
	int32_t num_cameras = p_file->get_s32();

	TRCameraInfo camera_info;
	camera_info.fixed.resize(num_cameras);

	for (int32_t i = 0; i < camera_info.fixed.size(); i++) {
		camera_info.fixed.set(i, read_tr_game_vector(p_file));
	}
}

void read_tr_flyby_cameras(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	if (p_level_format == TR4_PC) {
		int32_t num_flyby_cameras = p_file->get_u32();
		for (int32_t i = 0; i < num_flyby_cameras; i++) {
			TRPos pos = read_tr_pos(p_file);
			TRPos angles = read_tr_pos(p_file);
			uint8_t sequence = p_file->get_u8();
			uint8_t index = p_file->get_u8();

			uint16_t fov = p_file->get_u16();
			int16_t roll = p_file->get_s16();
			uint16_t timer = p_file->get_u16();
			uint16_t speed = p_file->get_u16();
			uint16_t flags = p_file->get_u16();

			uint32_t room_id = p_file->get_u32();
		}
	}
}

Vector<TRObjectVector> read_tr_sound_effects(Ref<TRFileAccess> p_file) {
	int32_t num_sound_effects = p_file->get_s32();
	Vector<TRObjectVector> sound_effect_table;

	sound_effect_table.resize(num_sound_effects);

	for (int32_t i = 0; i < sound_effect_table.size(); i++) {
		sound_effect_table.set(i, read_tr_type_vector(p_file));
	}

	return sound_effect_table;
}

TRNavCell read_tr_nav_cell(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	TRNavCell nav_cell;

	if (p_level_format == TR1_PC) {
		nav_cell.left = p_file->get_s32();
		nav_cell.right = p_file->get_s32();
		nav_cell.top = p_file->get_s32();
		nav_cell.bottom = p_file->get_s32();
	} else {
		nav_cell.left = p_file->get_u8();
		nav_cell.right = p_file->get_u8();
		nav_cell.top = p_file->get_u8();
		nav_cell.bottom = p_file->get_u8();
	}

	nav_cell.height = p_file->get_s16();
	nav_cell.overlap_index = p_file->get_s16();

	return nav_cell;
}

void read_tr_nav_cells(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	int32_t num_nav_cells = p_file->get_s32();

	Vector<TRNavCell> nav_cells;
	nav_cells.resize(num_nav_cells);
	for (int32_t i = 0; i < num_nav_cells; i++) {
		nav_cells.set(i, read_tr_nav_cell(p_file, p_level_format));
	}

	int32_t num_overlaps = p_file->get_s32();
	PackedByteArray overlaps = p_file->get_buffer(num_overlaps * sizeof(int16_t));

	for (int32_t i = 0; i < 2; i++) {
		PackedByteArray ground_zone = p_file->get_buffer(num_nav_cells * sizeof(int16_t));
		PackedByteArray ground_zone2 = p_file->get_buffer(num_nav_cells * sizeof(int16_t));
		if (p_level_format != TR1_PC) {
			PackedByteArray ground_zone3 = p_file->get_buffer(num_nav_cells * sizeof(int16_t));
			PackedByteArray ground_zone4 = p_file->get_buffer(num_nav_cells * sizeof(int16_t));
		}
		PackedByteArray fly_zone = p_file->get_buffer(num_nav_cells * sizeof(int16_t));
	}
}

void read_tr_animated_textures(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	int32_t animated_texture_dispatch_count = p_file->get_s32();

	PackedByteArray animated_texture_dispatches = p_file->get_buffer(animated_texture_dispatch_count * sizeof(int16_t));

	if (p_level_format == TR4_PC) {
		uint8_t animated_textures_uv_count = p_file->get_u8();
	}
}

void read_tr_ai_objects(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	if (p_level_format == TR4_PC) {
		uint32_t ai_object_count = p_file->get_u32();
		for (int64_t i = 0; i < ai_object_count; i++) {
			uint16_t type_id = p_file->get_u16();
			uint16_t room = p_file->get_u16();
			TRPos pos = read_tr_pos(p_file);
			int16_t ocb = p_file->get_s16();
			uint16_t flags = p_file->get_u16();
			int32_t angle = p_file->get_s32();
		}
	}
}

Vector<TREntity> read_tr_entities(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	Vector<TREntity> entities;

	int32_t entity_count = p_file->get_s32();

	if (entity_count) {
		ERR_FAIL_COND_V(entity_count > 10240, Vector<TREntity>());
		
		for (int32_t i = 0; i < entity_count; i++) {
			TREntity entity;

			entity.type_id = p_file->get_s16();
			entity.room_number = p_file->get_s16();
			entity.transform.pos = read_tr_pos(p_file);
			entity.transform.rot.y = p_file->get_s16();
			entity.shade = p_file->get_s16();
			entity.ocb = 0;
			if (p_level_format == TR2_PC || p_level_format == TR3_PC) {
				entity.shade2 = p_file->get_s16();
			} else {
				entity.shade2 = entity.shade;
				if (p_level_format == TR4_PC) {
					entity.ocb = p_file->get_s16();
				}
			}
			entity.flags = p_file->get_u16();

			ERR_FAIL_COND_V(entity.type_id < 0, Vector<TREntity>());
			ERR_FAIL_COND_V(entity.type_id >= 4096, Vector<TREntity>());

			entities.push_back(entity);
		}
	}

	return entities;
}

void read_tr_lightmap(Ref<TRFileAccess> p_file) {
	int32_t pos = p_file->get_position();
	p_file->seek(p_file->get_position() + (sizeof(uint8_t) * 32 * TR_TEXTILE_SIZE));
}

TRCameraFrame read_tr_camera_frame(Ref<TRFileAccess> p_file) {
	TRCameraFrame camera_frame;
	
	camera_frame.target.x = p_file->get_s16();
	camera_frame.target.y = p_file->get_s16();
	camera_frame.target.z = p_file->get_s16();
	
	camera_frame.pos.x = p_file->get_s16();
	camera_frame.pos.y = p_file->get_s16();
	camera_frame.pos.z = p_file->get_s16();
	
	camera_frame.fov = p_file->get_s16();
	camera_frame.roll = p_file->get_s16();

	return camera_frame;
}

Vector<TRCameraFrame> read_tr_camera_frames(Ref<TRFileAccess> p_file) {
	int16_t camera_frame_count = p_file->get_s16();

	Vector<TRCameraFrame> camera_frames;
	for (int32_t i = 0; i < camera_frame_count; i++) {
		camera_frames.push_back(read_tr_camera_frame(p_file));
	}

	return camera_frames;
}

void read_tr_demo_frames(Ref<TRFileAccess> p_file) {
	int16_t demo_frame_count = p_file->get_s16();

	p_file->seek(p_file->get_position() + (demo_frame_count * sizeof(uint8_t)));

	return;
}

TRSoundInfo read_tr_sound_info(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	TRSoundInfo sound_info;

	sound_info.sample_index = p_file->get_u16();
	if (p_level_format == TR1_PC || p_level_format == TR2_PC) {
		sound_info.volume = p_file->get_u16();
		sound_info.range = 8;
		sound_info.chance = p_file->get_u16();
		sound_info.pitch = 0xff * 0.2;
	} else {
		sound_info.volume = (uint16_t)(p_file->get_u8()) << 8;
		sound_info.range = p_file->get_u8();
		sound_info.chance = (uint16_t)(p_file->get_u8()) << 8;
		sound_info.pitch = p_file->get_u8();
	}
	sound_info.characteristics = p_file->get_u16();

	return sound_info;
}

Vector<uint16_t> read_tr_sound_map(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	uint32_t sound_map_count = (p_level_format == TR1_PC) ? 256 : 370;
	Vector<uint16_t> sound_map;
	for (uint32_t i = 0; i < sound_map_count; i++) {
		sound_map.push_back(p_file->get_u16());
	}

	return sound_map;
}

Vector<TRSoundInfo> read_tr_sound_infos(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	uint32_t sound_info_count = p_file->get_u32();

	Vector<TRSoundInfo> sound_infos;
	for (uint32_t i = 0; i < sound_info_count; i++) {
		sound_infos.push_back(read_tr_sound_info(p_file, p_level_format));
	}

	return sound_infos;
}

PackedByteArray read_tr_sound_buffer(Ref<TRFileAccess> p_file) {
	uint32_t buffer_size = p_file->get_u32();

	return p_file->get_buffer(buffer_size);
}

PackedInt32Array read_tr_sound_indices(Ref<TRFileAccess> p_file) {
	uint32_t indices_count = p_file->get_u32();
	PackedInt32Array pi32array;
	pi32array.resize(indices_count);
	for (int64_t i = 0; i < indices_count; i++) {
		pi32array.set(i, p_file->get_u32());
	}

	return pi32array;
}

Vector<TRColor3> read_tr_palette(Ref<TRFileAccess> p_file) {
	Vector<TRColor3> palette;
	palette.resize(TR_TEXTILE_SIZE);

	int32_t pos = p_file->get_position();

	for (int32_t i = 0; i < TR_TEXTILE_SIZE; i++) {
		TRColor3 color;

		color.r = p_file->get_u8() * 4;
		color.g = p_file->get_u8() * 4;
		color.b = p_file->get_u8() * 4;

		palette.set(i, color);
	}

	return palette;
}

Vector<TRColor4> read_tr_palette_32(Ref<TRFileAccess> p_file) {
	Vector<TRColor4> palette;
	palette.resize(TR_TEXTILE_SIZE);

	int32_t pos = p_file->get_position();

	for (int32_t i = 0; i < TR_TEXTILE_SIZE; i++) {
		TRColor4 color;

		color.r = p_file->get_u8() * 4;
		color.g = p_file->get_u8() * 4;
		color.b = p_file->get_u8() * 4;
		color.a = p_file->get_u8() * 4;

		palette.set(i, color);
	}

	return palette;
}

void TRLevel::clear_level() {
	while (get_child_count() > 0) {
		get_child(0)->queue_free();
		remove_child(get_child(0));
	}
}

void TRLevel::load_level(bool p_lara_only) {
	TRLevelData level_data = load_level_type(level_path);

	Node3D *rooms_node = generate_godot_scene(
		this, 
		level_data,
		p_lara_only);
}

TRLevelData TRLevel::load_level_type(String file_path) {
	Vector<TRColor3> palette;
	Vector<TRColor4> palette32;
	TRTextureType texture_type = TR_TEXTURE_TYPE_8_PAL;
	Vector<PackedByteArray> entity_textures;
	Vector<PackedByteArray> level_textures;
	PackedByteArray other_decompressed_buffer;

	Error error;
	TRLevelData level_data;
	level_data.format = TR_VERSION_UNKNOWN;

	Ref<TRFileAccess> file = TRFileAccess::open(level_path, &error);
	if (error != Error::OK) {
		return level_data;
	}

	TRLevelFormat format = TR1_PC;
	int32_t version = file->get_s32();
	if (version == 0x00000020) {
		format = TR1_PC;
	}
	else if (version == 0x0000002D) {
		format = TR2_PC;
	}
	else if (version == 0xFF080038 || version == 0xFF180038) {
		format = TR3_PC;
	}
	else if (version == 0x00345254 && level_path.get_extension() == "tr4") {
		format = TR4_PC;
	}

	ERR_FAIL_COND_V(format == TR_VERSION_UNKNOWN, level_data);

	if (format == TR4_PC) {
		uint16_t num_room_textiles = file->get_u16();
		uint16_t num_obj_textiles = file->get_u16();
		uint16_t num_bump_textiles = file->get_u16();

		uint32_t total_textiles = num_room_textiles + num_obj_textiles + num_bump_textiles;

		// 32-Bit
		uint32_t textiles32_decompressed_size = file->get_u32();
		uint32_t textiles32_compressed_size = file->get_u32();

		PackedByteArray textiles32_compressed_buffer = file->get_buffer(textiles32_compressed_size);
		PackedByteArray textiles32_decompressed_buffer;
		textiles32_decompressed_buffer.resize(textiles32_decompressed_size);

		Compression compression_32;
		compression_32.decompress(textiles32_decompressed_buffer.ptrw(), textiles32_decompressed_size, textiles32_compressed_buffer.ptr(), textiles32_compressed_size, Compression::MODE_DEFLATE);

		// 16-Bit
		uint32_t textiles16_decompressed_size = file->get_u32();
		uint32_t textiles16_compressed_size = file->get_u32();

		PackedByteArray textiles16_compressed_buffer = file->get_buffer(textiles16_compressed_size);
		PackedByteArray textiles16_decompressed_buffer;
		textiles16_decompressed_buffer.resize(textiles16_decompressed_size);

		Compression compression_16;
		compression_16.decompress(textiles16_decompressed_buffer.ptrw(), textiles16_decompressed_size, textiles16_compressed_buffer.ptr(), textiles16_compressed_size, Compression::MODE_DEFLATE);


		Ref<TRFileAccess> uncompressed_file_access = TRFileAccess::create_from_buffer(textiles32_decompressed_buffer);
		level_textures = read_tr_texture_pages_32(uncompressed_file_access, num_room_textiles);
		entity_textures = read_tr_texture_pages_32(uncompressed_file_access, num_obj_textiles);
		Vector<PackedByteArray> bump_textures_32 = read_tr_texture_pages_32(uncompressed_file_access, num_bump_textiles);
		texture_type = TR_TEXTURE_TYPE_32;

		// Font and Sky
		uint32_t font_and_sky_decompressed_size = file->get_u32();
		uint32_t font_and_sky_compressed_size = file->get_u32();

		PackedByteArray font_and_sky_compressed_buffer = file->get_buffer(font_and_sky_compressed_size);
		PackedByteArray font_and_sky_decompressed_buffer;
		font_and_sky_decompressed_buffer.resize(font_and_sky_decompressed_size);

		Compression font_and_sky_compression;
		font_and_sky_compression.decompress(font_and_sky_decompressed_buffer.ptrw(), font_and_sky_decompressed_size, font_and_sky_compressed_buffer.ptr(), font_and_sky_compressed_size, Compression::MODE_DEFLATE);

		// Other
		uint32_t other_decompressed_size = file->get_u32();
		uint32_t other_compressed_size = file->get_u32();

		PackedByteArray other_compressed_buffer = file->get_buffer(other_compressed_size);
		other_decompressed_buffer.resize(other_decompressed_size);

		Compression other_compression;
		other_compression.decompress(other_decompressed_buffer.ptrw(), other_decompressed_size, other_compressed_buffer.ptr(), other_compressed_size, Compression::MODE_DEFLATE);

		file = TRFileAccess::create_from_buffer(other_decompressed_buffer);
	} else {
		if (format == TR2_PC || format == TR3_PC) {
			palette = read_tr_palette(file);
			palette32 = read_tr_palette_32(file);
		}

		Vector<PackedByteArray> textures = read_tr_texture_pages(file, format);
		texture_type = TR_TEXTURE_TYPE_8_PAL;
		level_textures = textures;
		entity_textures = textures;
	}

	int32_t file_level_num = file->get_s32();

	Vector<TRRoom> rooms = read_tr_rooms(file, format);

	PackedByteArray floor_data = read_tr_floor_data(file);

	TRTypes types = read_tr_types(file, format);

	if (format == TR4_PC) {
		PackedByteArray spr_ident = file->get_buffer(3);
	}
	read_tr_sprites(file);

	read_tr_cameras(file);

	read_tr_flyby_cameras(file, format);

	read_tr_sound_effects(file);

	read_tr_nav_cells(file, format);

	read_tr_animated_textures(file, format);

	if (format == TR3_PC || format == TR4_PC) {
		if (format == TR4_PC) {
			PackedByteArray tex_ident = file->get_buffer(3);
		}
		types.texture_infos = read_tr_texture_infos(file, format);
	}

	Vector<TREntity> entities = read_tr_entities(file, format);

	if (format == TR4_PC) {
		read_tr_ai_objects(file, format);
	}

	if (format == TR1_PC || format == TR2_PC || format == TR3_PC) {
		read_tr_lightmap(file);
	}

	if (format == TR1_PC) {
		palette = read_tr_palette(file);
	}

	if (format == TR1_PC || format == TR2_PC || format == TR3_PC) {
		read_tr_camera_frames(file);
	}

	read_tr_demo_frames(file);


	Vector<uint16_t> sound_map; 
	if (format != TR4_PC) {
		sound_map = read_tr_sound_map(file, format);
	}
	types.sound_map = sound_map;

	Vector<TRSoundInfo> sound_infos;
	if (format != TR4_PC) {
		sound_infos = read_tr_sound_infos(file, format);
	}

	PackedByteArray sound_buffer;

	if (format == TR1_PC) {
		sound_buffer = read_tr_sound_buffer(file);
	}

	PackedInt32Array sound_indices;
	if (format != TR4_PC) {
		sound_indices = read_tr_sound_indices(file);
	}

	if (format == TR2_PC || format == TR3_PC) {
		Error sfx_error;
		String directory_path = level_path.get_base_dir();
		Ref<TRFileAccess> sfx_file;
		sfx_file = TRFileAccess::open(directory_path + "/MAIN.SFX", &sfx_error);

		sound_buffer = sfx_file->get_buffer(sfx_file->get_size());
		sfx_file->seek(0);

		Vector<int32_t> wave_table;
		// TODO: handle Remaster
		for (int32_t i = 0; i < 370; i++) {
			if (sfx_file->get_fixed_string(4) == "RIFF") {
				wave_table.append(sfx_file->get_position() - 4);
				uint32_t file_size = sfx_file->get_u32();
				if (file_size > 0) {
					if (sfx_file->get_fixed_string(4) == "WAVE") {
						if (sfx_file->get_fixed_string(4) == "fmt ") {
							uint32_t length = sfx_file->get_u32();
							uint16_t format_type = sfx_file->get_u16();
							uint16_t channels = sfx_file->get_u16();
							uint32_t mix_rate = sfx_file->get_u32();
							uint32_t byte_per_second = sfx_file->get_u32();
							uint16_t byte_per_block = sfx_file->get_u16();
							uint16_t bits_per_sample = sfx_file->get_u16();

							if (sfx_file->get_fixed_string(4) == "data") {
								uint32_t buffer_size = sfx_file->get_u32();
								sfx_file->seek(sfx_file->get_position() + buffer_size);
							}
						}
					}
				}
			}
		}
		for (int32_t i = 0; i < sound_indices.size(); i++) {
			int32_t index = sound_indices.get(i);
			if (index < wave_table.size()) {
				sound_indices.set(i, wave_table.get(index));
			} else {
				sound_indices.set(i, -1);
			}
		}
	}

	if (format == TR4_PC) {
		PackedByteArray seperator = file->get_buffer(6);
	}

#if 0
	dump_8bit_textures(room_textures, palette);
	dump_8bit_textures(object_textures, palette);
#endif

	level_data.format = format;
	level_data.texture_type = texture_type;
	level_data.level_textures = level_textures;
	level_data.entity_textures = entity_textures;
	level_data.palette = palette;
	level_data.rooms = rooms;
	level_data.entities = entities;
	level_data.types = types;
	level_data.floor_data = floor_data;
	level_data.sound_map = sound_map;
	level_data.sound_infos = sound_infos;
	level_data.sound_buffer = sound_buffer;
	level_data.sound_indices = sound_indices;

	return level_data;
}

void TRLevel::_notification(int32_t p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			break;
	}
}