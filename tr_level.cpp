#include "tr_level.hpp"

#include <core/io/stream_peer_gzip.h>
#include "core/math/math_funcs.h"
#include <editor/editor_file_system.h>
#include <scene/3d/audio_stream_player_3d.h>
#include <scene/3d/bone_attachment_3d.h>
#include <scene/3d/lightmap_gi.h>
#include <scene/3d/mesh_instance_3d.h>
#include <scene/3d/physics/collision_shape_3d.h>
#include <scene/3d/physics/physics_body_3d.h>
#include <scene/3d/physics/static_body_3d.h>
#include <scene/animation/animation_blend_tree.h>
#include <scene/animation/animation_node_state_machine.h>
#include <scene/animation/animation_player.h>
#include <scene/resources/audio_stream_wav.h>
#include <scene/resources/mesh_data_tool.h>
#include <scene/resources/3d/box_shape_3d.h>
#include <scene/resources/3d/concave_polygon_shape_3d.h>
#include <scene/resources/3d/primitive_meshes.h>
#include <scene/resources/surface_tool.h>
#include <scene/resources/image_texture.h>
#include <scene/3d/marker_3d.h>
#include <scene/3d/remote_transform_3d.h>


#ifdef IS_MODULE
using namespace godot;
#endif
#define TR_TO_GODOT_SCALE 0.001 * 2.0
#define TR_TEXTILE_SIZE 256

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

Transform3D tr_transform_to_godot_transform(TRTransform p_tr_transform) {
	Vector3 position = Vector3(
		p_tr_transform.pos.x * TR_TO_GODOT_SCALE,
		p_tr_transform.pos.y * -TR_TO_GODOT_SCALE,
		p_tr_transform.pos.z * -TR_TO_GODOT_SCALE);

	real_t rot_y_deg = (real_t)(p_tr_transform.rot.y) / 16384.0f * -90.0f;
	real_t rot_x_deg = (real_t)(p_tr_transform.rot.x) / 16384.0f * 90.0f;
	real_t rot_z_deg = (real_t)(p_tr_transform.rot.z) / 16384.0f * -90.0f;

	real_t rot_y_rad = Math::deg_to_rad(rot_y_deg);
	real_t rot_x_rad = Math::deg_to_rad(rot_x_deg);
	real_t rot_z_rad = Math::deg_to_rad(rot_z_deg);

	Basis rotation_basis;
	rotation_basis.rotate(Vector3(rot_x_rad, rot_y_rad, rot_z_rad), EulerOrder::YXZ);

	return Transform3D(rotation_basis, position);
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

struct TRInterpolatedFrame {
	TRAnimFrame first_frame;
	TRAnimFrame second_frame;
	real_t interpolation;
};

TRInterpolatedFrame get_final_frame_for_animation(int32_t p_anim_idx, Vector<TRAnimation> &p_animations) {
	TRInterpolatedFrame interpolated_frame;

	ERR_FAIL_INDEX_V(p_anim_idx, p_animations.size(), interpolated_frame);

	TRAnimation tr_animation_current = p_animations.get(p_anim_idx);
	int32_t next_animation_number = tr_animation_current.next_animation_number;

	ERR_FAIL_INDEX_V(next_animation_number, p_animations.size(), interpolated_frame);
	TRAnimation tr_next_animation = p_animations.get(next_animation_number);

	int32_t attempts_remaining = 128;
	while (tr_next_animation.frames.size() == 0) {
		ERR_FAIL_INDEX_V(next_animation_number, p_animations.size(), interpolated_frame);
		tr_animation_current = tr_next_animation;
		tr_next_animation = p_animations.get(next_animation_number);

		attempts_remaining--;
		ERR_FAIL_COND_V(attempts_remaining <= 0, interpolated_frame);
	}

	int32_t next_frame_idx = (tr_animation_current.next_frame_number - tr_next_animation.frame_base);
	int32_t next_keyframe_modulo = next_frame_idx % tr_next_animation.frame_skip;

	if (p_anim_idx == next_animation_number && tr_animation_current.next_frame_number == tr_animation_current.frame_end) {
		interpolated_frame.interpolation = 0.0;
		interpolated_frame.first_frame = tr_next_animation.frames[tr_next_animation.frames.size() - 1];
		interpolated_frame.second_frame = tr_next_animation.frames[tr_next_animation.frames.size() - 1];
	}
	else {
		if (tr_next_animation.frame_skip > 0) {
			int32_t keyframe_idx = next_frame_idx / tr_next_animation.frame_skip;
			ERR_FAIL_INDEX_V(keyframe_idx, tr_next_animation.frames.size(), interpolated_frame);
			interpolated_frame.first_frame = tr_next_animation.frames[keyframe_idx];
			interpolated_frame.second_frame = tr_next_animation.frames[keyframe_idx];
			if (next_keyframe_modulo > 0) {
				interpolated_frame.first_frame = tr_next_animation.frames[keyframe_idx];

				if (keyframe_idx + 1 >= tr_next_animation.frames.size()) {
					while (keyframe_idx + 1 >= tr_next_animation.frames.size()) {
						tr_next_animation = p_animations.get(next_animation_number);
						keyframe_idx = 0;
						interpolated_frame.second_frame = tr_next_animation.frames[keyframe_idx];
					}
				} else {
					interpolated_frame.second_frame = tr_next_animation.frames[keyframe_idx + 1];
				}

				interpolated_frame.interpolation = real_t(next_keyframe_modulo) / real_t(tr_next_animation.frame_skip);
			}
		}
		else {
			ERR_FAIL_INDEX_V(next_frame_idx, tr_next_animation.frames.size(), interpolated_frame);
			interpolated_frame.first_frame = interpolated_frame.second_frame = tr_next_animation.frames[next_frame_idx];
		}
	}

	return interpolated_frame;
}

Node3D *create_godot_moveable_model(
	uint32_t p_type_info_id,
	TRMoveableInfo p_moveable_info,
	TRTypes p_types,
	Vector<Ref<ArrayMesh>> p_meshes,
	Vector<Ref<AudioStream>> p_samples,
	TRLevelFormat p_level_format,
	bool p_use_unique_names) {
	Node3D *new_type_info = memnew(Node3D);
	new_type_info->set_name(get_type_info_name(p_type_info_id, p_level_format));

	Vector<int32_t> bone_stack;

	int32_t offset_bone_index = p_moveable_info.bone_index;

	AudioStreamPlayer3D *audio_stream_player = nullptr;

	Vector<TRPos> animation_position_offsets;

	HashMap<int32_t, int32_t> mesh_to_bone_mapping;
	HashMap<int32_t, int32_t> bone_to_mesh_mapping;

	Vector<MeshInstance3D *> mesh_instances;
	int32_t current_parent = -1;
	if (p_moveable_info.mesh_count > 0) {
		// TODO: Separate animation handling for single mesh types
		if (p_moveable_info.mesh_count > 0) {
			Node3D *armature = memnew(Node3D);
			armature->set_display_folded(true);
			armature->set_name("Armature");

			new_type_info->add_child(armature);

			Skeleton3D *skeleton = memnew(Skeleton3D);
			
			armature->add_child(skeleton);
			skeleton->set_display_folded(true);
			if (p_use_unique_names) {
				skeleton->set_name("GeneralSkeleton");
				skeleton->set_unique_name_in_owner(true);
			} else {
				skeleton->set_name("Skeleton");
			}

			Quaternion root_quat = Quaternion::from_euler(Vector3(0.0, 0.0, 0.0));

			int32_t root_idx = skeleton->add_bone("Root");
			skeleton->set_bone_parent(0, current_parent);
			current_parent = root_idx;
			skeleton->set_bone_rest(0, Transform3D());
			skeleton->set_bone_pose_position(0, Vector3());
			skeleton->set_bone_pose_rotation(0, root_quat);
			skeleton->set_bone_pose_scale(0, Vector3(1.0, 1.0, 1.0));

			Node3D *scaled_root = memnew(Node3D);
			scaled_root->set_name("ScaledRoot");
			new_type_info->add_child(scaled_root);

			StaticBody3D *avatar_bounds = memnew(StaticBody3D);
			avatar_bounds->set_name("AvatarBounds");
			avatar_bounds->set_collision_layer(0);
			avatar_bounds->set_collision_mask(0);
			scaled_root->add_child(avatar_bounds);

			CollisionShape3D *collision_shape = memnew(CollisionShape3D);
			collision_shape->set_display_folded(true);
			collision_shape->set_name("BoundingBox");
			
			Ref<BoxShape3D> box_shape = memnew(BoxShape3D);
			collision_shape->set_shape(box_shape);

			avatar_bounds->add_child(collision_shape);

			Marker3D* grip_center_position = memnew(Marker3D);
			grip_center_position->set_name("GripPointCenter");
			Marker3D *camera_target = memnew(Marker3D);
			camera_target->set_name("CameraTarget");

			scaled_root->add_child(grip_center_position);
			scaled_root->add_child(camera_target);

			if (p_use_unique_names) {
				grip_center_position->set_unique_name_in_owner(true);
				camera_target->set_unique_name_in_owner(true);
			}

			AnimationPlayer *animation_player = memnew(AnimationPlayer);
			new_type_info->add_child(animation_player);
			animation_player->set_name("AnimationPlayer");
			animation_player->set_audio_max_polyphony(1);

			AnimationNodeStateMachine *state_machine = memnew(AnimationNodeStateMachine);
			AnimationTree *animation_tree = memnew(AnimationTree);
			animation_tree->set_name("AnimationTree");
			animation_tree->set_root_animation_node(state_machine);
			new_type_info->add_child(animation_tree);
			animation_tree->set_animation_player(animation_tree->get_path_to(animation_player));
			animation_tree->set_active(false);

			Node *animation_root_node = animation_player->get_node_or_null(animation_player->get_root_node());
			ERR_FAIL_COND_V(!animation_root_node, nullptr);

			NodePath skeleton_path = animation_root_node->get_path_to(skeleton);
			NodePath grip_center_position_path = animation_root_node->get_path_to(grip_center_position);
			NodePath camera_target_path = animation_root_node->get_path_to(camera_target);
			if (p_use_unique_names) {
				skeleton_path = NodePath("%GeneralSkeleton");
				grip_center_position_path = NodePath("%GripPointCenter");
				camera_target_path = NodePath("%CameraTarget");
			}
			NodePath shape_path = animation_root_node->get_path_to(collision_shape);

			Ref<AnimationLibrary> animation_library = memnew(AnimationLibrary);
			Ref<Animation> reset_animation = memnew(Animation);
			reset_animation->set_name("RESET");

			reset_animation->add_track(Animation::TYPE_POSITION_3D);
			reset_animation->track_set_path(reset_animation->get_track_count() - 1, NodePath(String(skeleton_path) + ":Root"));
			reset_animation->position_track_insert_key(reset_animation->get_track_count() - 1, 0.0, Vector3());
			reset_animation->position_track_insert_key(reset_animation->get_track_count() - 1, 1.0, Vector3());
			reset_animation->add_track(Animation::TYPE_ROTATION_3D);
			reset_animation->track_set_path(reset_animation->get_track_count() - 1, NodePath(String(skeleton_path) + ":Root"));
			reset_animation->rotation_track_insert_key(reset_animation->get_track_count() - 1, 0.0, root_quat);
			reset_animation->rotation_track_insert_key(reset_animation->get_track_count() - 1, 1.0, root_quat);

			reset_animation->add_track(Animation::TYPE_POSITION_3D);
			reset_animation->track_set_path(reset_animation->get_track_count() - 1, NodePath(String(shape_path)));
			reset_animation->track_insert_key(reset_animation->get_track_count() - 1, 0.0, Vector3());
			reset_animation->track_insert_key(reset_animation->get_track_count() - 1, 1.0, Vector3());
			reset_animation->add_track(Animation::TYPE_VALUE);
			reset_animation->track_set_path(reset_animation->get_track_count() - 1, NodePath(String(shape_path) + ":shape:size"));
			reset_animation->track_insert_key(reset_animation->get_track_count() - 1, 0.0, Vector3(1.0, 1.0, 1.0));
			reset_animation->track_insert_key(reset_animation->get_track_count() - 1, 1.0, Vector3(1.0, 1.0, 1.0));

			animation_library->add_animation("RESET", reset_animation);

			Vector<Ref<Animation>> godot_animations;

#define ANIMATION_GRAPH_SPACING 256.0
			size_t grid_size = size_t(Math::floor(Math::sqrt(real_t(p_moveable_info.animations.size()))));
			for (int64_t anim_idx = 0; anim_idx < p_moveable_info.animations.size(); anim_idx++) {
				String animation_name = get_animation_name(p_type_info_id, anim_idx, p_level_format);
				Ref<AnimationNodeAnimation> animation_node = memnew(AnimationNodeAnimation);
				animation_node->set_animation(animation_name);

				size_t x_pos = anim_idx % grid_size;
				size_t y_pos = anim_idx / grid_size;

				state_machine->add_node(animation_name, animation_node, Vector2(real_t(x_pos) * ANIMATION_GRAPH_SPACING, real_t(y_pos) * ANIMATION_GRAPH_SPACING));
			}

			Vector<bool> apply_180_rotation_on_final_frame;
			Vector<bool> apply_180_rotation_on_first_frame;
			apply_180_rotation_on_first_frame.resize(p_moveable_info.animations.size());
			apply_180_rotation_on_final_frame.resize(p_moveable_info.animations.size());
			for (int64_t i = 0; i < p_moveable_info.animations.size(); i++) {
				apply_180_rotation_on_first_frame.set(i, false);
				apply_180_rotation_on_final_frame.set(i, false);
			}

			for (int64_t anim_idx = 0; anim_idx < p_moveable_info.animations.size(); anim_idx++) {
				Ref<Animation> godot_animation = memnew(Animation);
				TRAnimation tr_animation = p_moveable_info.animations.get(anim_idx);

				String animation_name = get_animation_name(p_type_info_id, anim_idx, p_level_format);
				Ref<AnimationNodeAnimation> animation_node = state_machine->get_node(animation_name);

				godot_animation->set_name(animation_name);

				godot_animations.push_back(godot_animation);

				real_t animation_length = (real_t)((tr_animation.frames.size()) / TR_FPS) * tr_animation.frame_skip;

				if (tr_animation.next_animation_number == p_moveable_info.animation_index + anim_idx && tr_animation.next_frame_number == tr_animation.frame_base) {
					godot_animation->set_loop_mode(Animation::LOOP_LINEAR);
				} else {
					godot_animation->set_loop_mode(Animation::LOOP_NONE);
				}

				godot_animation->set_length(((tr_animation.frame_end - tr_animation.frame_base) + 1) / TR_FPS);
				
				animation_library->add_animation(get_animation_name(p_type_info_id, anim_idx, p_level_format), godot_animation);

				Array animation_state_changes;

				int16_t state_change_index = tr_animation.state_change_index;
				for (int64_t state_change_offset_idx = 0; state_change_offset_idx < tr_animation.number_state_changes; state_change_offset_idx++) {
					ERR_FAIL_INDEX_V(state_change_index + state_change_offset_idx, p_types.animation_state_changes.size(), nullptr);

					TRAnimationStateChange state_change = p_types.animation_state_changes.get(state_change_index + state_change_offset_idx);

					Dictionary state_change_dict;
					int16_t target_animation_state = state_change.target_animation_state;
					int16_t dispatch_index = state_change.dispatch_index;
					state_change_dict.set("target_animation_state", target_animation_state);
					state_change_dict.set("target_animation_state_name", get_state_name(p_type_info_id, target_animation_state, p_level_format));

					Array dispatches;
					for (int64_t dispatch_offset_idx = 0; dispatch_offset_idx < state_change.number_dispatches; dispatch_offset_idx++) {
						ERR_FAIL_INDEX_V(dispatch_index + dispatch_offset_idx, p_types.animation_dispatches.size(), nullptr);
						TRAnimationDispatch dispatch = p_types.animation_dispatches.get(dispatch_index + dispatch_offset_idx);
						if (dispatch.target_animation_number >= p_moveable_info.animation_index
							&& dispatch.target_animation_number < p_moveable_info.animation_index + p_moveable_info.animation_count) {
							TRAnimation new_tr_animation = p_types.animations.get(dispatch.target_animation_number);

							Dictionary dispatch_dict;

							real_t start_time = real_t(dispatch.start_frame - tr_animation.frame_base) / real_t(TR_FPS);
							real_t end_time = real_t(dispatch.end_frame - tr_animation.frame_base) / real_t(TR_FPS);
							real_t target_frame_time = real_t(dispatch.target_frame_number - new_tr_animation.frame_base) / real_t(TR_FPS);

							dispatch_dict.set("start_frame", dispatch.start_frame - tr_animation.frame_base);
							dispatch_dict.set("end_frame", dispatch.end_frame - tr_animation.frame_base);

							dispatch_dict.set("start_time", start_time);
							dispatch_dict.set("end_time", end_time);

							String target_animation_name = get_animation_name(p_type_info_id, dispatch.target_animation_number - p_moveable_info.animation_index, p_level_format);

							dispatch_dict.set("target_animation_id", dispatch.target_animation_number - p_moveable_info.animation_index);
							dispatch_dict.set("target_animation_name", target_animation_name);
							dispatch_dict.set("target_frame_number", dispatch.target_frame_number - new_tr_animation.frame_base);
							dispatch_dict.set("target_frame_time", target_frame_time);

							Ref<AnimationNodeStateMachineTransition> transition = memnew(AnimationNodeStateMachineTransition);
							transition->set_switch_mode(AnimationNodeStateMachineTransition::SWITCH_MODE_IMMEDIATE);
							transition->set_xfade_time(0.0);
							if (animation_name != target_animation_name) {
								if (!state_machine->has_transition(animation_name, target_animation_name)) {
									state_machine->add_transition(animation_name, target_animation_name, transition);
								}
							}

							dispatches.append(dispatch_dict);

							godot_animation->add_marker(vformat("frame_%d", dispatch.start_frame - tr_animation.frame_base), start_time);
							godot_animation->add_marker(vformat("frame_%d", dispatch.end_frame - tr_animation.frame_base), end_time);
						}
						else {
							ERR_PRINT("Target animation out of range.");
						}
					}
					state_change_dict.set("dispatches", dispatches);
					animation_state_changes.append(state_change_dict);
				}

				int32_t audio_stream_track_idx = -1;

				Array command_list;
				int32_t command_offset = tr_animation.command_index;

				TRPos position_offset;

				for (int32_t command_idx = 0; command_idx < tr_animation.number_commands; command_idx++) {
					if (command_offset >= p_types.animation_commands.size()) {
						continue;
					}

					switch (p_types.animation_commands.get(command_offset++).command) {
						case 1: {
							if (command_offset + 2 >= p_types.animation_commands.size()) {
								continue;
							}

							// Set Position
							position_offset.x = p_types.animation_commands.get(command_offset++).command;
							position_offset.y = p_types.animation_commands.get(command_offset++).command;
							position_offset.z = p_types.animation_commands.get(command_offset++).command;

							Dictionary dict;
							dict.set("command_name", "set_position");
							dict.set("x", position_offset.x);
							dict.set("y", position_offset.y);
							dict.set("z", position_offset.z);

							command_list.push_back(dict);
							break;
						}
						case 2: {
							if (command_offset + 1 >= p_types.animation_commands.size()) {
								continue;
							}

							// Set Jump Velocity
							int16_t vertical_velocity = p_types.animation_commands.get(command_offset++).command;
							int16_t horizontal_velocity = p_types.animation_commands.get(command_offset++).command;

							Dictionary dict;
							dict.set("command_name", "set_jump_velocity");
							dict.set("vertical_velocity", -vertical_velocity);
							dict.set("horizontal_velocity", -horizontal_velocity);

							command_list.push_back(dict);
							break;
						}
						case 3: {
							// Free hands
							Dictionary dict;
							dict.set("command_name", "free_hands");

							command_list.push_back(dict);
							break;
						}
						case 4: {
							// Kill
							Dictionary dict;
							dict.set("command_name", "kill");

							command_list.push_back(dict);
							break;
						}
						case 5: {
							// Play SFX
							if (!audio_stream_player) {
								audio_stream_player = memnew(AudioStreamPlayer3D);
								audio_stream_player->set_name("EntitySounds");
								audio_stream_player->set_playback_type(AudioServer::PLAYBACK_TYPE_SAMPLE);
								audio_stream_player->set_volume_db(-20.0);
								new_type_info->add_child(audio_stream_player);
							}

							if (audio_stream_track_idx == -1) {
								godot_animation->add_track(Animation::TYPE_AUDIO);
								audio_stream_track_idx = godot_animation->get_track_count() - 1;
								godot_animation->track_set_path(audio_stream_track_idx, animation_root_node->get_path_to(audio_stream_player));
								godot_animation->audio_track_set_use_blend(audio_stream_track_idx, false);
							}

							if (command_offset + 1 >= p_types.animation_commands.size()) {
								continue;
							}

							uint16_t frame_number = p_types.animation_commands.get(command_offset++).command;
							uint16_t sound_id = p_types.animation_commands.get(command_offset++).command;

							if (p_level_format != TR1_PC) {
								// Only handle land sounds for now...
								if (sound_id & 0x4000) {
									sound_id = sound_id & 0x3fff;
								}

								if (sound_id & 0x8000) {
									continue;
								}
							}

							if (sound_id < p_types.sound_map.size()) {
								uint16_t remapped_sound_id = p_types.sound_map.get(sound_id);
								if (remapped_sound_id < p_samples.size()) {
									Ref<AudioStream> sample_stream = p_samples.get(remapped_sound_id);
									godot_animation->audio_track_insert_key(audio_stream_track_idx, real_t(frame_number - tr_animation.frame_base) / real_t(TR_FPS), sample_stream);
								}
							}


							Dictionary dict;
							dict.set("command_name", "play_sound");
							dict.set("frame_number", frame_number);
							dict.set("sound_id", sound_id);

							command_list.push_back(dict);

							break;
						}
						case 6: {
							// Flip effect
							int16_t frame_number = p_types.animation_commands.get(command_offset++).command;
							int16_t flipeffect_id = p_types.animation_commands.get(command_offset++).command;

							if (flipeffect_id == 0) {
								if (frame_number == tr_animation.frame_end) {
									apply_180_rotation_on_final_frame.set(anim_idx, true);
								} else if (frame_number == tr_animation.frame_base) {
									apply_180_rotation_on_first_frame.set(anim_idx, true);
								}
							}

							Dictionary dict;
							dict.set("command_name", "play_flipeffect");
							dict.set("frame_number", frame_number);
							dict.set("flipeffect_id", flipeffect_id);

							command_list.push_back(dict);
							break;
						}
					}
				}

				String next_animation_name = get_animation_name(p_type_info_id, tr_animation.next_animation_number - p_moveable_info.animation_index, p_level_format);

				godot_animation->set_meta("tr_animation_state_changes", animation_state_changes);
				godot_animation->set_meta("tr_animation_current_animation_state", tr_animation.current_animation_state);
				godot_animation->set_meta("tr_animation_current_animation_state_name", get_state_name(p_type_info_id, tr_animation.current_animation_state, p_level_format));

				godot_animation->set_meta("tr_animation_frame_skip", tr_animation.frame_skip);
				
				godot_animation->set_meta("tr_animation_next_animation_id", tr_animation.next_animation_number - p_moveable_info.animation_index);
				godot_animation->set_meta("tr_animation_next_animation_name", get_animation_name(p_type_info_id, tr_animation.next_animation_number - p_moveable_info.animation_index, p_level_format));
				godot_animation->set_meta("tr_animation_next_frame", tr_animation.next_frame_number - p_types.animations.get(tr_animation.next_animation_number).frame_base);
				godot_animation->set_meta("tr_animation_next_time", real_t(tr_animation.next_frame_number - p_types.animations.get(tr_animation.next_animation_number).frame_base) / real_t(TR_FPS));
				
				godot_animation->set_meta("tr_animation_velocity_tr_units", tr_animation.velocity);
				godot_animation->set_meta("tr_animation_acceleration_tr_units", tr_animation.acceleration);
				
				godot_animation->set_meta("tr_animation_lateral_velocity_tr_units", tr_animation.lateral_velocity);
				godot_animation->set_meta("tr_animation_lateral_acceleration_tr_units", tr_animation.lateral_acceleration);

				godot_animation->set_meta("tr_animation_velocity", -double(tr_animation.velocity >> 16) * TR_TO_GODOT_SCALE);
				godot_animation->set_meta("tr_animation_acceleration", -double(tr_animation.acceleration >> 16) * TR_TO_GODOT_SCALE);

				godot_animation->set_meta("tr_animation_lateral_velocity", -double(tr_animation.lateral_velocity >> 16) * TR_TO_GODOT_SCALE);
				godot_animation->set_meta("tr_animation_lateral_acceleration", -double(tr_animation.lateral_acceleration >> 16) * TR_TO_GODOT_SCALE);

				godot_animation->set_meta("tr_animation_command_list", command_list);

				if (animation_name != next_animation_name) {
					Ref<AnimationNodeStateMachineTransition> transition = memnew(AnimationNodeStateMachineTransition);
					transition->set_switch_mode(AnimationNodeStateMachineTransition::SWITCH_MODE_AT_END);
					transition->set_xfade_time(0.0);
					if (animation_name != next_animation_name) {
						if (!state_machine->has_transition(animation_name, next_animation_name)) {
							state_machine->add_transition(animation_name, next_animation_name, transition);
						}
					}
				}

				animation_position_offsets.append(position_offset);
			}

			animation_player->add_animation_library("", animation_library);
			animation_player->set_current_animation("RESET");

			String root_motion_path_string = String(skeleton_path) + ":Root";
			animation_player->set_root_motion_track(root_motion_path_string);
			animation_tree->set_root_motion_track(root_motion_path_string);

			real_t motion_scale = 1.0;

			Vector<Transform3D> bone_pre_transforms;
			Vector<Transform3D> bone_post_transforms;
			Vector<Transform3D> mesh_transforms;
			HashMap<int32_t, Basis> dummy_bone_global_rotations;

			mesh_transforms.resize(p_moveable_info.mesh_count);

			for (int64_t mesh_idx = 0; mesh_idx < p_moveable_info.mesh_count; mesh_idx++) {
				int32_t offset_mesh_index = p_moveable_info.mesh_index + mesh_idx;
				if (offset_mesh_index < p_meshes.size()) {
					Transform3D transform_offset;

					if (mesh_idx > 0) {
						ERR_FAIL_COND_V(p_types.mesh_tree_buffer.size() < (offset_bone_index + 4), nullptr);

						int32_t flags = p_types.mesh_tree_buffer.get(offset_bone_index++);
						int32_t x = p_types.mesh_tree_buffer.get(offset_bone_index++);
						int32_t y = p_types.mesh_tree_buffer.get(offset_bone_index++);
						int32_t z = p_types.mesh_tree_buffer.get(offset_bone_index++);

						transform_offset = Transform3D(Basis(), Vector3(
							x * TR_TO_GODOT_SCALE,
							y * -TR_TO_GODOT_SCALE,
							z * -TR_TO_GODOT_SCALE));

						if (flags & TR_BONE_POP) {
							ERR_FAIL_COND_V(bone_stack.size() <= 0, nullptr);
							current_parent = bone_stack.get(bone_stack.size() - 1);
							bone_stack.remove_at(bone_stack.size() - 1);
						}

						if (flags & TR_BONE_PUSH) {
							bone_stack.append(current_parent);
						}
					} else {
						bone_stack.push_back(0);
						if (does_overwrite_mesh_rotation(p_type_info_id, p_level_format)) {
							transform_offset = Transform3D().rotated(Vector3(0.0, 1.0, 0.0), Math_PI);
						}
					}

					String bone_name = get_bone_name(p_type_info_id, mesh_idx, p_level_format);

					int32_t valid_parent = current_parent;
					if (does_have_dummy_bone(p_type_info_id, mesh_idx, p_level_format)) {
						DummyBoneInfo dummy_bone = get_dummy_bone_info(p_type_info_id, mesh_idx, p_level_format);
						int32_t dummy_bone_idx = skeleton->add_bone(dummy_bone.name);
						skeleton->set_bone_parent(dummy_bone_idx, valid_parent);

						transform_offset.origin += dummy_bone.offset * TR_TO_GODOT_SCALE;

						skeleton->set_bone_rest(dummy_bone_idx, transform_offset);
						skeleton->set_bone_pose(dummy_bone_idx, transform_offset);

						transform_offset = Transform3D();
						transform_offset.origin -= dummy_bone.offset * TR_TO_GODOT_SCALE;
						
						valid_parent = dummy_bone_idx;

						dummy_bone_global_rotations[dummy_bone_idx] = dummy_bone.rotation;
					}

					int32_t bone_idx = skeleton->add_bone(bone_name);
					skeleton->set_bone_parent(bone_idx, valid_parent);
					skeleton->set_bone_rest(bone_idx, transform_offset);
					skeleton->set_bone_pose(bone_idx, transform_offset);

					mesh_to_bone_mapping[mesh_idx] = bone_idx;
					bone_to_mesh_mapping[bone_idx] = mesh_idx;

					mesh_transforms.set(mesh_idx, transform_offset);

					current_parent = bone_idx;
				}
			}

			bone_pre_transforms.resize(skeleton->get_bone_count());
			bone_post_transforms.resize(skeleton->get_bone_count());

			if (does_overwrite_mesh_rotation(p_type_info_id, p_level_format)) {
				for (int64_t bone_idx = 0; bone_idx < skeleton->get_bone_count(); bone_idx++) {
					if (bone_idx > 0) {
						Transform3D diff;
						if (bone_to_mesh_mapping.has(bone_idx)) {
							diff = skeleton->get_bone_global_rest(bone_idx).get_basis().inverse() * get_overwritten_mesh_rotation(p_type_info_id, bone_to_mesh_mapping[bone_idx], p_level_format);
						} else {
							if (dummy_bone_global_rotations.has(bone_idx)) {
								diff = skeleton->get_bone_global_rest(bone_idx).get_basis().inverse() * dummy_bone_global_rotations[bone_idx];
							}
						}

						Transform3D new_pose = skeleton->get_bone_pose(bone_idx) * diff;
						Transform3D new_rest = skeleton->get_bone_rest(bone_idx) * diff;

						skeleton->set_bone_pose(bone_idx, new_pose);
						skeleton->set_bone_rest(bone_idx, new_rest);

						bone_post_transforms.set(bone_idx, diff);

						PackedInt32Array children = skeleton->get_bone_children(bone_idx);
						for (int32_t c : children) {
							skeleton->set_bone_pose(c, diff.affine_inverse() * skeleton->get_bone_pose(c));
							skeleton->set_bone_rest(c, diff.affine_inverse() * skeleton->get_bone_rest(c));

							bone_pre_transforms.set(c, diff.affine_inverse());
						}
					}
				}
			}


			for (int64_t mesh_idx = 0; mesh_idx < p_moveable_info.mesh_count; mesh_idx++) {
				int32_t offset_mesh_index = p_moveable_info.mesh_index + mesh_idx;
				int32_t bone_idx = mesh_to_bone_mapping[mesh_idx];

				if (offset_mesh_index < p_meshes.size()) {
					Transform3D transform_offset = mesh_transforms.get(mesh_idx);

					String bone_name = get_bone_name(p_type_info_id, mesh_idx, p_level_format);

					if (p_moveable_info.animations.size()) {
						TRAnimation reference_tr_animation = p_moveable_info.animations.get(0);
						if (reference_tr_animation.frames.size() > 0) {
							TRAnimFrame reference_anim_frame = reference_tr_animation.frames[0];
							TRTransform reference_bone_transform = reference_anim_frame.transforms.get(mesh_idx);

							Vector3 frame_origin = Vector3(
								reference_bone_transform.pos.x * TR_TO_GODOT_SCALE,
								reference_bone_transform.pos.y * -TR_TO_GODOT_SCALE,
								reference_bone_transform.pos.z * -TR_TO_GODOT_SCALE);

							if (mesh_idx == 0) {
								bool uses_motion_scale = does_type_use_motion_scale(p_type_info_id, p_level_format);
								if (uses_motion_scale) {
									motion_scale = abs((transform_offset.origin + frame_origin).y);
									if (motion_scale > 0.0) {
										skeleton->set_motion_scale(motion_scale);
									}
								}

								scaled_root->set_scale(Vector3(motion_scale, motion_scale, motion_scale));
							}

							Transform3D reset_final_transform = Transform3D(transform_offset.basis, (transform_offset.origin + frame_origin) * Vector3(1.0, 1.0 / motion_scale, 1.0));
							reset_final_transform = bone_pre_transforms.get(bone_idx) * reset_final_transform * bone_post_transforms.get(bone_idx);

							if (mesh_idx == 0) {
								if (does_overwrite_mesh_rotation(p_type_info_id, p_level_format)) {
									reset_final_transform.origin.x = 0.0;
									reset_final_transform.origin.z = 0.0;
								}
								else {
									reset_final_transform = Transform3D().rotated(Vector3(0.0, 1.0, 0.0), Math_PI) * reset_final_transform;
								}
							}

							reset_animation->add_track(Animation::TYPE_POSITION_3D);
							reset_animation->track_set_path(reset_animation->get_track_count() - 1, NodePath(String(skeleton_path) + ":" + bone_name));
							reset_animation->position_track_insert_key(reset_animation->get_track_count() - 1, 0.0, reset_final_transform.origin);
							reset_animation->position_track_insert_key(reset_animation->get_track_count() - 1, 1.0, reset_final_transform.origin);
							reset_animation->add_track(Animation::TYPE_ROTATION_3D);
							reset_animation->track_set_path(reset_animation->get_track_count() - 1, NodePath(String(skeleton_path) + ":" + bone_name));
							reset_animation->rotation_track_insert_key(reset_animation->get_track_count() - 1, 0.0, reset_final_transform.basis.get_rotation_quaternion());
							reset_animation->rotation_track_insert_key(reset_animation->get_track_count() - 1, 1.0, reset_final_transform.basis.get_rotation_quaternion());
							reset_animation->add_track(Animation::TYPE_SCALE_3D);
							reset_animation->track_set_path(reset_animation->get_track_count() - 1, NodePath(String(skeleton_path) + ":" + bone_name));
							reset_animation->scale_track_insert_key(reset_animation->get_track_count() - 1, 0.0, Vector3(1.0, 1.0, 1.0));
							reset_animation->scale_track_insert_key(reset_animation->get_track_count() - 1, 1.0, Vector3(1.0, 1.0, 1.0));

						} else {
							if (mesh_idx == 0) {
								bool uses_motion_scale = does_type_use_motion_scale(p_type_info_id, p_level_format);
								if (uses_motion_scale) {
									motion_scale = abs(transform_offset.origin.y);
									if (motion_scale > 0.0) {
										skeleton->set_motion_scale(motion_scale);
									}
								}
							}

							Transform3D reset_final_transform = Transform3D(transform_offset.basis, (transform_offset.origin) * Vector3(1.0, 1.0 / motion_scale, 1.0));

							reset_final_transform = bone_pre_transforms.get(bone_idx) * reset_final_transform * bone_post_transforms.get(bone_idx);

							reset_animation->add_track(Animation::TYPE_POSITION_3D);
							reset_animation->track_set_path(reset_animation->get_track_count() - 1, NodePath(String(skeleton_path) + ":" + bone_name));
							reset_animation->position_track_insert_key(reset_animation->get_track_count() - 1, 0.0, reset_final_transform.origin);
							reset_animation->position_track_insert_key(reset_animation->get_track_count() - 1, 1.0, reset_final_transform.origin);
							reset_animation->add_track(Animation::TYPE_ROTATION_3D);
							reset_animation->track_set_path(reset_animation->get_track_count() - 1, NodePath(String(skeleton_path) + ":" + bone_name));
							reset_animation->rotation_track_insert_key(reset_animation->get_track_count() - 1, 0.0, reset_final_transform.basis.get_rotation_quaternion());
							reset_animation->rotation_track_insert_key(reset_animation->get_track_count() - 1, 1.0, reset_final_transform.basis.get_rotation_quaternion());
							reset_animation->add_track(Animation::TYPE_SCALE_3D);
							reset_animation->track_set_path(reset_animation->get_track_count() - 1, NodePath(String(skeleton_path) + ":" + bone_name));
							reset_animation->scale_track_insert_key(reset_animation->get_track_count() - 1, 0.0, Vector3(1.0, 1.0, 1.0));
							reset_animation->scale_track_insert_key(reset_animation->get_track_count() - 1, 1.0, Vector3(1.0, 1.0, 1.0));
						}
					}

					const int32_t EXTRA_FRAMES = 1;

					for (int64_t anim_idx = 0; anim_idx < p_moveable_info.animations.size(); anim_idx++) {
						TRAnimation tr_animation = p_moveable_info.animations.get(anim_idx);
						Ref<Animation> godot_animation = godot_animations[anim_idx];

						if (mesh_idx == 0) {
							real_t animation_length = godot_animation->get_length();

							int32_t start_pos_x = 0;
							int32_t end_pos_x = 0;
							int32_t start_pos_z = 0;
							int32_t end_pos_z = 0;

							godot_animation->add_track(Animation::TYPE_POSITION_3D);
							godot_animation->track_set_path(godot_animation->get_track_count() - 1, NodePath(String(skeleton_path) + ":Root"));

							int32_t root_position_track_idx = godot_animation->get_track_count() - 1;

							godot_animation->add_track(Animation::TYPE_POSITION_3D);
							godot_animation->track_set_path(godot_animation->get_track_count() - 1, NodePath(String(shape_path)));
							int32_t shape_position_track_idx = godot_animation->get_track_count() - 1;

							godot_animation->add_track(Animation::TYPE_VALUE);
							godot_animation->track_set_path(godot_animation->get_track_count() - 1, NodePath(String(shape_path) + ":shape:size"));
							int32_t shape_size_track_idx = godot_animation->get_track_count() - 1;

							// Bounds points
							godot_animation->add_track(Animation::TYPE_POSITION_3D);
							godot_animation->track_set_path(godot_animation->get_track_count() - 1, NodePath(String(grip_center_position_path)));
							int32_t grip_center_position_track_idx = godot_animation->get_track_count() - 1;

							godot_animation->add_track(Animation::TYPE_POSITION_3D);
							godot_animation->track_set_path(godot_animation->get_track_count() - 1, NodePath(String(camera_target_path)));
							int32_t camera_target_track_idx = godot_animation->get_track_count() - 1;

							int32_t frame_counter = 1;
							int32_t frame_skips = 1;

							end_pos_x -= (tr_animation.lateral_velocity + (tr_animation.lateral_acceleration * frame_counter)) >> 16;
							end_pos_z -= (tr_animation.velocity + (tr_animation.acceleration * frame_counter)) >> 16;

							for (int32_t frame_idx = 0; frame_idx < tr_animation.frames.size() + EXTRA_FRAMES; frame_idx++) {

								if (frame_idx < tr_animation.frames.size()) {
									if (frame_idx == 0 || tr_animation.acceleration != 0 || tr_animation.lateral_acceleration != 0) {
										godot_animation->position_track_insert_key(root_position_track_idx, (real_t)(frame_idx / TR_FPS) * tr_animation.frame_skip, Vector3((-double(end_pos_x) * TR_TO_GODOT_SCALE) / motion_scale, 0.0, (-double(end_pos_z) * TR_TO_GODOT_SCALE) / motion_scale));
									}

									for (; frame_skips < tr_animation.frame_skip; frame_skips++) {
										end_pos_x -= (tr_animation.lateral_velocity + (tr_animation.lateral_acceleration * frame_counter)) >> 16;
										end_pos_z -= (tr_animation.velocity + (tr_animation.acceleration * frame_counter)) >> 16;

										godot_animation->position_track_insert_key(root_position_track_idx, (1.0 / TR_FPS)* frame_counter, Vector3((-double(end_pos_x) * TR_TO_GODOT_SCALE) / motion_scale, 0.0, (-double(end_pos_z) * TR_TO_GODOT_SCALE) / motion_scale));

										frame_counter++;
									}
									frame_skips = 0;
								}

								Vector3 gd_bbox_min;
								Vector3 gd_bbox_max;

								if (frame_idx == tr_animation.frames.size()) {
									if (godot_animation->get_loop_mode() == Animation::LOOP_LINEAR) {
										ERR_FAIL_INDEX_V(0, tr_animation.frames.size(), nullptr);
										TRAnimFrame first_frame = tr_animation.frames[0];
										gd_bbox_min = Vector3(first_frame.bounding_box.x_min * TR_TO_GODOT_SCALE, first_frame.bounding_box.y_min * TR_TO_GODOT_SCALE, first_frame.bounding_box.z_min * TR_TO_GODOT_SCALE);
										gd_bbox_max = Vector3(first_frame.bounding_box.x_max * TR_TO_GODOT_SCALE, first_frame.bounding_box.y_max * TR_TO_GODOT_SCALE, first_frame.bounding_box.z_max * TR_TO_GODOT_SCALE);
									}
									else if (godot_animation->get_loop_mode() == Animation::LOOP_NONE) {
										TRInterpolatedFrame interpolated_frame = get_final_frame_for_animation(anim_idx, p_types.animations);

										Vector3 gd_bbox_min_a = Vector3(interpolated_frame.first_frame.bounding_box.x_min * TR_TO_GODOT_SCALE, interpolated_frame.first_frame.bounding_box.y_min * TR_TO_GODOT_SCALE, interpolated_frame.first_frame.bounding_box.z_min * TR_TO_GODOT_SCALE);
										Vector3 gd_bbox_max_a = Vector3(interpolated_frame.first_frame.bounding_box.x_max * TR_TO_GODOT_SCALE, interpolated_frame.first_frame.bounding_box.y_max * TR_TO_GODOT_SCALE, interpolated_frame.first_frame.bounding_box.z_max * TR_TO_GODOT_SCALE);

										Vector3 gd_bbox_min_b = Vector3(interpolated_frame.second_frame.bounding_box.x_min * TR_TO_GODOT_SCALE, interpolated_frame.second_frame.bounding_box.y_min * TR_TO_GODOT_SCALE, interpolated_frame.second_frame.bounding_box.z_min * TR_TO_GODOT_SCALE);
										Vector3 gd_bbox_max_b = Vector3(interpolated_frame.second_frame.bounding_box.x_max * TR_TO_GODOT_SCALE, interpolated_frame.second_frame.bounding_box.y_max * TR_TO_GODOT_SCALE, interpolated_frame.second_frame.bounding_box.z_max * TR_TO_GODOT_SCALE);

										gd_bbox_min = gd_bbox_min_a.lerp(gd_bbox_min_b, interpolated_frame.interpolation);
										gd_bbox_max = gd_bbox_max_a.lerp(gd_bbox_max_b, interpolated_frame.interpolation);
									}
								} else {
									TRAnimFrame frame = tr_animation.frames.get(frame_idx);
									gd_bbox_min = Vector3(frame.bounding_box.x_min * TR_TO_GODOT_SCALE, frame.bounding_box.y_min * TR_TO_GODOT_SCALE, frame.bounding_box.z_min * TR_TO_GODOT_SCALE);
									gd_bbox_max = Vector3(frame.bounding_box.x_max * TR_TO_GODOT_SCALE, frame.bounding_box.y_max * TR_TO_GODOT_SCALE, frame.bounding_box.z_max * TR_TO_GODOT_SCALE);
								}

								Vector3 gd_bbox_position = (gd_bbox_min + gd_bbox_max) / 2.0;
								Vector3 gd_bbox_scale = gd_bbox_max - gd_bbox_min;

								gd_bbox_position.x = -gd_bbox_position.x;
								gd_bbox_position.y = -gd_bbox_position.y;
								gd_bbox_position.z = gd_bbox_position.z;

								gd_bbox_position.x /= motion_scale;
								gd_bbox_position.y /= motion_scale;
								gd_bbox_position.z /= motion_scale;

								gd_bbox_scale.x /= motion_scale;
								gd_bbox_scale.y /= motion_scale;
								gd_bbox_scale.z /= motion_scale;

								godot_animation->position_track_insert_key(shape_position_track_idx, (real_t)(frame_idx / TR_FPS) * tr_animation.frame_skip, gd_bbox_position);
								godot_animation->track_insert_key(shape_size_track_idx, (real_t)(frame_idx / TR_FPS) * tr_animation.frame_skip, gd_bbox_scale);

								Vector3 center_grip_point = Vector3(0.0, -gd_bbox_min.y, gd_bbox_max.z) / motion_scale;
								Vector3 camera_target = Vector3(0.0, gd_bbox_position.y + (256 * TR_TO_GODOT_SCALE), 0.0);

								godot_animation->position_track_insert_key(grip_center_position_track_idx, (real_t)(frame_idx / TR_FPS) * tr_animation.frame_skip, center_grip_point);
								godot_animation->position_track_insert_key(camera_target_track_idx, (real_t)(frame_idx / TR_FPS) * tr_animation.frame_skip, camera_target);
							}

							TRAnimation tr_next_animation = p_types.animations.get(tr_animation.next_animation_number - p_moveable_info.animation_index);
							int32_t next_frame_number = tr_animation.next_frame_number - tr_next_animation.frame_base;

							// TODO: handle interpolation.

							end_pos_x -= ((tr_next_animation.lateral_velocity + (tr_next_animation.lateral_acceleration * next_frame_number)) >> 16);
							end_pos_z -= ((tr_next_animation.velocity + (tr_next_animation.acceleration * next_frame_number)) >> 16);

							godot_animation->position_track_insert_key(root_position_track_idx, animation_length, Vector3((-double(end_pos_x) * TR_TO_GODOT_SCALE) * motion_scale, 0.0, (-double(end_pos_z) * TR_TO_GODOT_SCALE) / motion_scale));

							godot_animation->add_track(Animation::TYPE_ROTATION_3D);
							int32_t root_rotation_track_idx = godot_animation->get_track_count() - 1;

							godot_animation->track_set_path(root_rotation_track_idx, NodePath(String(skeleton_path) + ":Root"));
							godot_animation->rotation_track_insert_key(root_rotation_track_idx, 0.0, Quaternion());
							godot_animation->rotation_track_insert_key(root_rotation_track_idx, animation_length, Quaternion());
						}

						int32_t position_track_idx = -1;
						if (mesh_idx == 0) {
							godot_animation->add_track(Animation::TYPE_POSITION_3D);
							position_track_idx = godot_animation->get_track_count() - 1;
							godot_animation->track_set_path(position_track_idx, NodePath(String(skeleton_path) + ":" + bone_name));
						}

						godot_animation->add_track(Animation::TYPE_ROTATION_3D);
						int32_t rotation_track_idx = godot_animation->get_track_count() - 1;
						godot_animation->track_set_path(rotation_track_idx, NodePath(String(skeleton_path) + ":" + bone_name));

						for (int32_t frame_idx = 0; frame_idx < tr_animation.frames.size() + EXTRA_FRAMES; frame_idx++) {
							real_t interpolation = 0.0;
							TRAnimFrame first_anim_frame;
							TRAnimFrame second_anim_frame;

							if (frame_idx >= tr_animation.frames.size()) {
								int32_t next_animation_number = tr_animation.next_animation_number - p_moveable_info.animation_index;

								if (godot_animation->get_loop_mode() == Animation::LOOP_LINEAR) {
									ERR_FAIL_INDEX_V(0, tr_animation.frames.size(), nullptr);
									first_anim_frame = second_anim_frame = tr_animation.frames[0];
								} else if (godot_animation->get_loop_mode() == Animation::LOOP_NONE) {
									TRInterpolatedFrame interpolated_frame = get_final_frame_for_animation(anim_idx, p_types.animations);
									interpolation = interpolated_frame.interpolation;
									first_anim_frame = interpolated_frame.first_frame;
									second_anim_frame = interpolated_frame.second_frame;
								}
								if (mesh_idx == 0) {
									TRTransform first_hips_transform;
									if (second_anim_frame.transforms.size()) {
										first_hips_transform = second_anim_frame.transforms.get(0);
									}
									TRTransform second_hips_transform;
									if (second_anim_frame.transforms.size()) {
										second_hips_transform = second_anim_frame.transforms.get(0);
									}

									first_hips_transform.pos.x += (animation_position_offsets.get(anim_idx).x);
									first_hips_transform.pos.y += (animation_position_offsets.get(anim_idx).y);
									first_hips_transform.pos.z += (animation_position_offsets.get(anim_idx).z);

									second_hips_transform.pos.x += (animation_position_offsets.get(anim_idx).x);
									second_hips_transform.pos.y += (animation_position_offsets.get(anim_idx).y);
									second_hips_transform.pos.z += (animation_position_offsets.get(anim_idx).z);

									if (apply_180_rotation_on_final_frame.get(anim_idx)) {
										first_hips_transform.rot.y -= 0x7fff;
										first_hips_transform.pos.z = -first_hips_transform.pos.z;

										second_hips_transform.rot.y -= 0x7fff;
										second_hips_transform.pos.z = -second_hips_transform.pos.z;
									}

									TRAnimation tr_animation_current = tr_animation;
									TRAnimation tr_next_animation = p_types.animations.get(next_animation_number);
									int32_t next_frame_idx = (tr_animation_current.next_frame_number - tr_next_animation.frame_base);

									if (apply_180_rotation_on_first_frame.size() > next_animation_number) {
										if (apply_180_rotation_on_first_frame[next_animation_number] && next_frame_idx == 0) {
											first_hips_transform.rot.y -= 0x7fff;
											first_hips_transform.pos.z = -first_hips_transform.pos.z;

											second_hips_transform.rot.y -= 0x7fff;
											second_hips_transform.pos.z = -second_hips_transform.pos.z;
										}
									}

									if (first_anim_frame.transforms.size() > mesh_idx) {
										first_anim_frame.transforms.set(mesh_idx, first_hips_transform);
									}
									if (second_anim_frame.transforms.size() > mesh_idx) {
										second_anim_frame.transforms.set(mesh_idx, second_hips_transform);
									}
								}
							} else {
								ERR_FAIL_INDEX_V(frame_idx, tr_animation.frames.size(), nullptr);
								first_anim_frame = second_anim_frame = tr_animation.frames[frame_idx];
							}

							Transform3D first_transform;
							Transform3D second_transform;
							if (mesh_idx < first_anim_frame.transforms.size()) {
								first_transform = tr_transform_to_godot_transform(first_anim_frame.transforms.get(mesh_idx));
							}
							if (mesh_idx < second_anim_frame.transforms.size()) {
								second_transform = tr_transform_to_godot_transform(second_anim_frame.transforms.get(mesh_idx));
							}

							Transform3D pre_fix;
							if (mesh_idx == 0) {
								pre_fix = pre_fix.rotated(Vector3(0.0, 1.0, 0.0), Math_PI);
							}

							Transform3D final_transform = pre_fix * first_transform.interpolate_with(second_transform, interpolation);
							final_transform.origin = ((transform_offset.origin + final_transform.origin) * Vector3(1.0, 1.0 / motion_scale, 1.0));

							final_transform = bone_pre_transforms.get(bone_idx) * final_transform * bone_post_transforms.get(bone_idx);

							if (position_track_idx >= 0) {
								godot_animation->position_track_insert_key(position_track_idx, (real_t)(frame_idx / TR_FPS) * tr_animation.frame_skip, final_transform.origin);
							}
							if (rotation_track_idx >= 0) {
								godot_animation->rotation_track_insert_key(rotation_track_idx, (real_t)(frame_idx / TR_FPS) * tr_animation.frame_skip, final_transform.basis.get_quaternion());
							}
						}
					}


					Ref<ArrayMesh> mesh = p_meshes.get(offset_mesh_index);
					MeshInstance3D *mi = memnew(MeshInstance3D);

					BoneAttachment3D *bone_attachment = memnew(BoneAttachment3D);
					bone_attachment->set_name(get_bone_name(p_type_info_id, mesh_idx, p_level_format) + "_attachment");
					skeleton->add_child(bone_attachment);
					bone_attachment->set_bone_idx(mesh_to_bone_mapping[mesh_idx]);
					bone_attachment->add_child(mi);

					mi->set_mesh(mesh);
					mi->set_name(String("MeshInstance_") + itos(offset_mesh_index));
					mi->set_layer_mask(1 << 1); // Dynamic
					mi->set_gi_mode(GeometryInstance3D::GI_MODE_DYNAMIC);

					if (does_overwrite_mesh_rotation(p_type_info_id, p_level_format)) {
						mi->set_transform(get_overwritten_mesh_rotation(p_type_info_id, mesh_idx, p_level_format).inverse() * Transform3D().rotated(Vector3(0.0, 1.0, 0.0), Math_PI).basis);
					}

					mesh_instances.append(mi);
				}
			}
		} else {
			int32_t offset_mesh_index = p_moveable_info.mesh_index;
			if (offset_mesh_index < p_meshes.size()) {
				Ref<ArrayMesh> mesh = p_meshes.get(offset_mesh_index);
				MeshInstance3D *mi = memnew(MeshInstance3D);

				new_type_info->add_child(mi);

				mi->set_mesh(mesh);
				mi->set_name(String("MeshInstance_") + itos(offset_mesh_index));
			}
		}
	}


	return new_type_info;
}

const int32_t TR_FRAME_BOUND_MIN_X = 0;
const int32_t TR_FRAME_BOUND_MAX_X = 1;
const int32_t TR_FRAME_BOUND_MIN_Y = 2;
const int32_t TR_FRAME_BOUND_MAX_Y = 3;
const int32_t TR_FRAME_BOUND_MIN_Z = 4;
const int32_t TR_FRAME_BOUND_MAX_Z = 5;
const int32_t TR_FRAME_POS_X = 6;
const int32_t TR_FRAME_POS_Y = 7;
const int32_t TR_FRAME_POS_Z = 8;
const int32_t TR_FRAME_ROT = 10;

Vector<Node3D *> create_nodes_for_moveables(
	HashMap<int32_t,
	TRMoveableInfo> p_type_info_map,
	TRTypes p_types,
	Vector<Ref<ArrayMesh>> p_meshes,
	Vector<Ref<AudioStream>> p_samples,
	TRLevelFormat p_level_format) {
	Vector<Node3D *> types;

	for (int32_t type_id = 0; type_id < 4096; type_id++) {
		if (p_type_info_map.has(type_id)) {
			Node3D *new_node = create_godot_moveable_model(type_id, p_type_info_map[type_id], p_types, p_meshes, p_samples, p_level_format, false);
			if (new_node) {
				types.push_back(new_node);
			}
		}
	}

	return types;
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

struct GeometryShift {
	int8_t z_floor_shift = 0;
	int8_t x_floor_shift = 0;
	int8_t z_ceiling_shift = 0;
	int8_t x_ceiling_shift = 0;
	uint8_t portal_room = 0xff;
};

static GeometryShift parse_floor_data_entry(PackedByteArray p_floor_data, uint16_t p_index) {
	GeometryShift geo_shift{};

	bool parsing = true;

	// ???
	if (p_index == 0) {
		return geo_shift;
	}

	while (parsing) {
		uint8_t first_byte = p_floor_data.get(p_index * 2);
		uint8_t second_byte = p_floor_data.get((p_index * 2) + 1);
		uint16_t data = (first_byte) | (uint16_t)(second_byte) << 8;

		uint8_t function = data & 0x001f;
		uint8_t sub_function = (data & 0x7f00) >> 8;
		uint8_t end_data = (data & 0x8000) >> 15;

		if (end_data) {
			parsing = false;
		}

		p_index++;

		switch (function) {
			case 0x01: {
				printf("portal\n");
				geo_shift.portal_room = p_floor_data.get(p_index * 2);
				p_index++;
				break;
			}
			case 0x02: {
				printf("floor_slant\n");
				geo_shift.x_floor_shift = p_floor_data.get(p_index * 2);
				geo_shift.z_floor_shift = p_floor_data.get((p_index * 2) + 1);
				p_index++;
				break;
			}
			case 0x03: {
				printf("ceiling_slant\n");
				geo_shift.x_ceiling_shift = p_floor_data.get(p_index * 2);
				geo_shift.z_ceiling_shift = p_floor_data.get((p_index * 2) + 1);
				p_index++;
				break;
			}
			case 0x04: {
				printf("trigger\n");

				uint8_t first_byte_2 = p_floor_data.get(p_index * 2);
				uint8_t second_byte_2 = p_floor_data.get((p_index * 2) + 1);
				uint16_t data_2 = (first_byte_2) | (uint16_t)(second_byte_2) << 8;

				// TR4+, this value should be signed
				uint8_t timer = data_2 & 0x00ff;

				// Shift this...
				uint8_t one_shot = data_2 & 0x0100;
				uint8_t mask = data_2 & 0x3e00;

				p_index++;
				break;
			}
			case 0x07: {
				printf("nw_se_floor_triangle\n");
				p_index++;
				break;
			}
			case 0x08: {
				printf("ne_sw_floor_triangle\n");
				p_index++;
				break;
			}
			case 0x09: {
				printf("nw_se_ceiling_triangle\n");
				p_index++;
				break;
			}
			case 0x0a: {
				printf("ne_sw_ceiling_triangle\n");
				p_index++;
				break;
			}
			case 0x0b: {
				printf("ne_floor_triangle\n");
				p_index++;
				break;
			}
		}
	}
	return geo_shift;
}

static real_t u_fixed_16_to_float(uint16_t p_fixed, bool no_fraction) {
	if (no_fraction) {
		return (real_t)((p_fixed & 0xff00) >> 8) + (real_t)((p_fixed & 0x00ff) / 255.0f);
	} else {
		return (real_t)((p_fixed & 0xff00) >> 8);
	}
}

#define INSERT_COLORED_VERTEX(p_current_idx, p_uv, p_vertex_id_list, p_color_index_map, p_verts_array) \
{ \
	ERR_FAIL_COND_V(p_current_idx < 0 || p_current_idx > p_verts_array.size(), Ref<ArrayMesh>()); \
	VertexAndUV vert_and_uv;\
	vert_and_uv.vertex_idx = p_current_idx;\
	vert_and_uv.uv = p_uv;\
	int32_t result = -1;\
	int32_t vertex_id_map_size = p_vertex_id_list.size();\
	for (int64_t idx = 0; idx < vertex_id_map_size; idx++) {\
		VertexAndUV new_vert_and_uv = p_vertex_id_list.get(idx);\
		if (new_vert_and_uv.vertex_idx == vert_and_uv.vertex_idx &&\
				new_vert_and_uv.uv.u == vert_and_uv.uv.u &&\
				new_vert_and_uv.uv.v == vert_and_uv.uv.v) {\
					result = idx;\
					break;\
		};\
	};\
	if (result < 0) { \
		result = vertex_id_map_size;\
		p_vertex_id_list.append(vert_and_uv);\
	};\
	p_color_index_map.append(result);\
}

#define INSERT_TEXTURED_VERTEX(p_current_idx, p_uv, p_texture_page, p_vertex_uv_map, p_material_index_map, p_verts_array) \
{ \
	ERR_FAIL_COND_V(p_current_idx < 0 || p_current_idx > p_verts_array.size(), Ref<ArrayMesh>()); \
	VertexAndUV vert_and_uv;\
	vert_and_uv.vertex_idx = p_current_idx;\
	vert_and_uv.uv = p_uv;\
	int32_t result = -1;\
	int32_t vertex_uv_map_size = p_vertex_uv_map.get(p_texture_page).size();\
	for (int64_t idx = 0; idx < vertex_uv_map_size; idx++) {\
		VertexAndUV new_vert_and_uv = p_vertex_uv_map.get(p_texture_page).get(idx);\
		if (new_vert_and_uv.vertex_idx == vert_and_uv.vertex_idx &&\
				new_vert_and_uv.uv.u == vert_and_uv.uv.u &&\
				new_vert_and_uv.uv.v == vert_and_uv.uv.v) {\
					result = idx;\
					break;\
		};\
	};\
	if (result < 0) { \
		result = vertex_uv_map_size; \
		p_vertex_uv_map.get(p_texture_page).append(vert_and_uv); \
	};\
	p_material_index_map.get(p_texture_page).append(result);\
}

Ref<ArrayMesh> tr_room_data_to_godot_mesh(const TRRoomData &p_room_data, const Vector<Ref<Material>> p_solid_level_materials, const Vector<Ref<Material>> p_level_trans_materials, TRTypes& p_types) {
	Ref<SurfaceTool> st = memnew(SurfaceTool);
	Ref<ArrayMesh> ar_mesh = memnew(ArrayMesh);

	Vector<TRRoomVertex> room_verts;
	room_verts.resize(p_room_data.room_vertex_count);

	for (int32_t i = 0; i < p_room_data.room_vertex_count; i++) {
		room_verts.set(i, p_room_data.room_vertices[i]);
	}

	struct VertexAndUV {
		int32_t vertex_idx;
		TRUV uv;
	};

	int32_t last_material_id = 0;

	HashMap<int32_t, Vector<VertexAndUV>> vertex_uv_map;
	HashMap<int32_t, Vector<int32_t> > material_index_map;

	//
	for (int32_t i = 0; i < p_room_data.room_quad_count; i++) {
		if (p_room_data.room_quads[i].tex_info_id < 0) {
			continue;
		}

		if (p_room_data.room_quads[i].tex_info_id >= p_types.texture_infos.size()) {
			continue;
		}

		TRTextureInfo texture_info = p_types.texture_infos.get(p_room_data.room_quads[i].tex_info_id);
		int32_t material_id = texture_info.texture_page;
		if (texture_info.draw_type == 1) {
			material_id += p_solid_level_materials.size();
		}
		last_material_id = material_id > last_material_id ? material_id : last_material_id;

		if (!vertex_uv_map.has(material_id)) {
			vertex_uv_map.insert(material_id, Vector<VertexAndUV>());
		}

		if (!material_index_map.has(material_id)) {
			material_index_map.insert(material_id, Vector<int32_t>());
		}

		INSERT_TEXTURED_VERTEX(p_room_data.room_quads[i].indices[0], texture_info.uv[0], material_id, vertex_uv_map, material_index_map, room_verts);
		INSERT_TEXTURED_VERTEX(p_room_data.room_quads[i].indices[1], texture_info.uv[1], material_id, vertex_uv_map, material_index_map, room_verts);
		INSERT_TEXTURED_VERTEX(p_room_data.room_quads[i].indices[2], texture_info.uv[2], material_id, vertex_uv_map, material_index_map, room_verts);

		INSERT_TEXTURED_VERTEX(p_room_data.room_quads[i].indices[2], texture_info.uv[2], material_id, vertex_uv_map, material_index_map, room_verts);
		INSERT_TEXTURED_VERTEX(p_room_data.room_quads[i].indices[3], texture_info.uv[3], material_id, vertex_uv_map, material_index_map, room_verts);
		INSERT_TEXTURED_VERTEX(p_room_data.room_quads[i].indices[0], texture_info.uv[0], material_id, vertex_uv_map, material_index_map, room_verts);
	}

	for (int32_t i = 0; i < p_room_data.room_triangle_count; i++) {
		if (p_room_data.room_triangles[i].tex_info_id < 0) {
			continue;
		}

		if (p_room_data.room_triangles[i].tex_info_id >= p_types.texture_infos.size()) {
			continue;
		}

		TRTextureInfo texture_info = p_types.texture_infos.get(p_room_data.room_triangles[i].tex_info_id);
		int32_t material_id = texture_info.texture_page;
		if (texture_info.draw_type == 1) {
			material_id += p_solid_level_materials.size();
		}

		last_material_id = material_id > last_material_id ? material_id : last_material_id;

		if (!vertex_uv_map.has(material_id)) {
			vertex_uv_map.insert(material_id, Vector<VertexAndUV>());
		}

		if (!material_index_map.has(material_id)) {
			material_index_map.insert(material_id, Vector<int32_t>());
		}

		INSERT_TEXTURED_VERTEX(p_room_data.room_triangles[i].indices[0], texture_info.uv[0], material_id, vertex_uv_map, material_index_map, room_verts);
		INSERT_TEXTURED_VERTEX(p_room_data.room_triangles[i].indices[1], texture_info.uv[1], material_id, vertex_uv_map, material_index_map, room_verts);
		INSERT_TEXTURED_VERTEX(p_room_data.room_triangles[i].indices[2], texture_info.uv[2], material_id, vertex_uv_map, material_index_map, room_verts);
	}

	Vector<Ref<Material>> all_materials = p_solid_level_materials;
	all_materials.append_array(p_level_trans_materials);

	for (int64_t current_tex_page = 0; current_tex_page < last_material_id + 1; current_tex_page++) {

		st->begin(Mesh::PRIMITIVE_TRIANGLES);

		if (!vertex_uv_map.has(current_tex_page) || !material_index_map.has(current_tex_page)) {
			continue;
		}

		for (int64_t i = 0; i < vertex_uv_map.get(current_tex_page).size(); i++) {
			VertexAndUV vertex_and_uv = vertex_uv_map.get(current_tex_page).get(i);
			TRRoomVertex room_vertex = room_verts[vertex_and_uv.vertex_idx];

			st->set_color(room_vertex.color);

			Vector2 uv = Vector2(u_fixed_16_to_float(vertex_and_uv.uv.u, false) / 255.0f, u_fixed_16_to_float(vertex_and_uv.uv.v, false) / 255.0f);
			st->set_uv(uv);

			Vector3 vec3 = Vector3(
				room_vertex.vertex.x * TR_TO_GODOT_SCALE,
				room_vertex.vertex.y * -TR_TO_GODOT_SCALE,
				room_vertex.vertex.z * -TR_TO_GODOT_SCALE);

			st->add_vertex(vec3);
		}

		for (int32_t i = 0; i < material_index_map.get(current_tex_page).size(); i++) {
			st->add_index(material_index_map.get(current_tex_page).get(i));
		}

		if (all_materials.size() > current_tex_page) {
			st->set_material(all_materials.get(current_tex_page));
		}
		st->generate_normals();
		ar_mesh = st->commit(ar_mesh);
		//ar_mesh->lightmap_unwrap();
	}

	return ar_mesh;
}

Ref<ArrayMesh> tr_mesh_to_godot_mesh(const TRMesh& p_mesh_data, const Vector<Ref<Material>> p_solid_level_materials, const Vector<Ref<Material>> p_level_trans_materials, const Ref<Material> p_level_palette_material, TRTypes& p_types) {
	Ref<SurfaceTool> st = memnew(SurfaceTool);
	Ref<ArrayMesh> ar_mesh = memnew(ArrayMesh);

	Vector<TRVertex> mesh_verts;

	for (int32_t i = 0; i < p_mesh_data.vertices.size(); i++) {
		mesh_verts.push_back(p_mesh_data.vertices[i]);
	}

	struct VertexAndUV {
		int32_t vertex_idx;
		TRUV uv;
	};

	struct VertexAndID {
		int32_t vertex_idx;
		uint8_t id;
	};

	int32_t last_material_id = 0;

	// Colors
	Vector<VertexAndUV> vertex_color_uv_map;
	Vector<int32_t> color_index_map;
	for (int32_t i = 0; i < p_mesh_data.color_quads_count; i++) {
		int16_t id = p_mesh_data.color_quads[i].tex_info_id;

		uint32_t y = (id / 16);
		uint32_t x = (id - (y * 16));
		y *= 16;
		x *= 16;

		TRUV uv[4];
		uv[0].u = (x + 4) << 8;
		uv[0].v = (y + 4) << 8;

		uv[1].u = (x + 16 - 4) << 8;
		uv[1].v = (y + 4) << 8;

		uv[2].u = (x + 4) << 8;
		uv[2].v = (y + 16 - 4) << 8;

		uv[3].u = (x + 16 - 4) << 8;
		uv[3].v = (y + 16 - 4) << 8;

		INSERT_COLORED_VERTEX(p_mesh_data.color_quads[i].indices[0], uv[0], vertex_color_uv_map, color_index_map, mesh_verts);
		INSERT_COLORED_VERTEX(p_mesh_data.color_quads[i].indices[1], uv[1], vertex_color_uv_map, color_index_map, mesh_verts);
		INSERT_COLORED_VERTEX(p_mesh_data.color_quads[i].indices[2], uv[2], vertex_color_uv_map, color_index_map, mesh_verts);

		INSERT_COLORED_VERTEX(p_mesh_data.color_quads[i].indices[2], uv[2], vertex_color_uv_map, color_index_map, mesh_verts);
		INSERT_COLORED_VERTEX(p_mesh_data.color_quads[i].indices[3], uv[3], vertex_color_uv_map, color_index_map, mesh_verts);
		INSERT_COLORED_VERTEX(p_mesh_data.color_quads[i].indices[0], uv[0], vertex_color_uv_map, color_index_map, mesh_verts);
	}

	for (int32_t i = 0; i < p_mesh_data.color_triangles_count; i++) {
		int16_t id = p_mesh_data.color_triangles[i].tex_info_id;

		uint32_t y = (id / 16);
		uint32_t x = (id - (y * 16));
		y *= 16;
		x *= 16;

		TRUV uv[3];
		uv[0].u = (x + 4) << 8;
		uv[0].v = (y + 4) << 8;

		uv[1].u = (x + 16 - 4) << 8;
		uv[1].v = (y + 4) << 8;

		uv[2].u = (x + 4) << 8;
		uv[2].v = (y + 16 - 4) << 8;

		INSERT_COLORED_VERTEX(p_mesh_data.color_triangles[i].indices[0], uv[0], vertex_color_uv_map, color_index_map, mesh_verts);
		INSERT_COLORED_VERTEX(p_mesh_data.color_triangles[i].indices[1], uv[1], vertex_color_uv_map, color_index_map, mesh_verts);
		INSERT_COLORED_VERTEX(p_mesh_data.color_triangles[i].indices[2], uv[2], vertex_color_uv_map, color_index_map, mesh_verts);
	}

	// Textures
	HashMap<int32_t, Vector<VertexAndUV>> vertex_uv_map;
	HashMap<int32_t, Vector<int32_t> > material_index_map;
	for (int32_t i = 0; i < p_mesh_data.texture_quads_count; i++) {
		TRTextureInfo texture_info = p_types.texture_infos.get(p_mesh_data.texture_quads[i].tex_info_id);
		int32_t material_id = texture_info.texture_page;
		if (texture_info.draw_type == 1) {
			material_id += p_solid_level_materials.size();
		}
		last_material_id = material_id > last_material_id ? material_id : last_material_id;

		if (!vertex_uv_map.has(material_id)) {
			vertex_uv_map.insert(material_id, Vector<VertexAndUV>());
		}

		if (!material_index_map.has(material_id)) {
			material_index_map.insert(material_id, Vector<int32_t>());
		}

		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_quads[i].indices[0], texture_info.uv[0], material_id, vertex_uv_map, material_index_map, mesh_verts);
		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_quads[i].indices[1], texture_info.uv[1], material_id, vertex_uv_map, material_index_map, mesh_verts);
		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_quads[i].indices[2], texture_info.uv[2], material_id, vertex_uv_map, material_index_map, mesh_verts);

		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_quads[i].indices[2], texture_info.uv[2], material_id, vertex_uv_map, material_index_map, mesh_verts);
		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_quads[i].indices[3], texture_info.uv[3], material_id, vertex_uv_map, material_index_map, mesh_verts);
		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_quads[i].indices[0], texture_info.uv[0], material_id, vertex_uv_map, material_index_map, mesh_verts);
	}

	for (int32_t i = 0; i < p_mesh_data.texture_triangles_count; i++) {
		TRTextureInfo texture_info = p_types.texture_infos.get(p_mesh_data.texture_triangles[i].tex_info_id);
		int32_t material_id = texture_info.texture_page;
		if (texture_info.draw_type == 1) {
			material_id += p_solid_level_materials.size();
		}

		last_material_id = material_id > last_material_id ? material_id : last_material_id;

		if (!vertex_uv_map.has(material_id)) {
			vertex_uv_map.insert(material_id, Vector<VertexAndUV>());
		}

		if (!material_index_map.has(material_id)) {
			material_index_map.insert(material_id, Vector<int32_t>());
		}

		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_triangles[i].indices[0], texture_info.uv[0], material_id, vertex_uv_map, material_index_map, mesh_verts);
		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_triangles[i].indices[1], texture_info.uv[1], material_id, vertex_uv_map, material_index_map, mesh_verts);
		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_triangles[i].indices[2], texture_info.uv[2], material_id, vertex_uv_map, material_index_map, mesh_verts);
	}

	if (vertex_color_uv_map.size() > 0) {
		st->begin(Mesh::PRIMITIVE_TRIANGLES);

		st->set_smooth_group(0);

		for (int32_t i = 0; i < vertex_color_uv_map.size(); i++) {
			VertexAndUV vertex_and_uv = vertex_color_uv_map.get(i);
			TRVertex vertex = mesh_verts[vertex_and_uv.vertex_idx];

			Vector2 uv = Vector2(u_fixed_16_to_float(vertex_and_uv.uv.u, false) / 255.0f, u_fixed_16_to_float(vertex_and_uv.uv.v, false) / 255.0f);
			st->set_uv(uv);

			Vector3 vec3 = Vector3(
				vertex.x * TR_TO_GODOT_SCALE,
				vertex.y * -TR_TO_GODOT_SCALE,
				vertex.z * -TR_TO_GODOT_SCALE);

			st->add_vertex(vec3);
		}

		for (int32_t i = 0; i < color_index_map.size(); i++) {
			st->add_index(color_index_map.get(i));
		}

		st->set_material(p_level_palette_material);
		ar_mesh = st->commit(ar_mesh);
	}

	Vector<Ref<Material>> all_materials = p_solid_level_materials;
	all_materials.append_array(p_level_trans_materials);

	for (int32_t current_tex_page = 0; current_tex_page < last_material_id + 1; current_tex_page++) {
		st->begin(Mesh::PRIMITIVE_TRIANGLES);

		if (!vertex_uv_map.has(current_tex_page) || !material_index_map.has(current_tex_page)) {
			continue;
		}

		for (int32_t i = 0; i < vertex_uv_map.get(current_tex_page).size(); i++) {
			VertexAndUV vertex_and_uv = vertex_uv_map.get(current_tex_page).get(i);
			TRVertex vertex = mesh_verts[vertex_and_uv.vertex_idx];

			Vector2 uv = Vector2(u_fixed_16_to_float(vertex_and_uv.uv.u, false) / 255.0f, u_fixed_16_to_float(vertex_and_uv.uv.v, false) / 255.0f);
			st->set_uv(uv);

			Vector3 vec3 = Vector3(
				vertex.x * TR_TO_GODOT_SCALE,
				vertex.y * -TR_TO_GODOT_SCALE,
				vertex.z * -TR_TO_GODOT_SCALE);

			st->add_vertex(vec3);
		}

		for (int32_t i = 0; i < material_index_map.get(current_tex_page).size(); i++) {
			st->add_index(material_index_map.get(current_tex_page).get(i));
		}

		if (current_tex_page < all_materials.size()) {
			st->set_material(all_materials.get(current_tex_page));
		}
		ar_mesh = st->commit(ar_mesh);
	}

	return ar_mesh;
}

struct GeometryCalculation {
	bool portal_floor;
	bool portal_ceiling;
	bool portal_wall;
	bool solid;

	real_t floor_north_west;
	real_t floor_north_east;
	real_t floor_south_west;
	real_t floor_south_east;

	real_t ceiling_north_west;
	real_t ceiling_north_east;
	real_t ceiling_south_west;
	real_t ceiling_south_east;
};

CollisionShape3D *tr_room_to_collision_shape(const TRRoom& p_current_room, PackedByteArray p_floor_data, Vector<TRRoom> p_rooms) {
	Ref<ConcavePolygonShape3D> collision_data = memnew(ConcavePolygonShape3D);

	const real_t SQUARE_SIZE = 1024.0 * TR_TO_GODOT_SCALE;
	const real_t CLICK_SIZE = SQUARE_SIZE / 4.0;

	PackedVector3Array buf;

	int32_t y_bottom = p_current_room.info.y_bottom;
	int32_t y_top = p_current_room.info.y_top;

	Vector<GeometryCalculation> calculated;
	calculated.resize(p_current_room.sector_count_z * p_current_room.sector_count_x);

	int32_t current_sector = 0;

	// Floors and ceilings
	for (int32_t x_sector = 0; x_sector < p_current_room.sector_count_z; x_sector++) {
		for (int32_t y_sector = 0; y_sector < p_current_room.sector_count_x; y_sector++) {
			GeometryCalculation calc = calculated.get(current_sector);
			calc.solid = true;
			calc.portal_floor = false;
			calc.portal_ceiling = false;
			calc.portal_wall = false;

			TRRoomSector room_sector = p_current_room.sectors.get(current_sector);

			uint8_t room_below = room_sector.room_below;
			uint8_t room_above = room_sector.room_above;

			if (room_below != 0xff) {
				calc.portal_floor = true;
			}

			if (room_above != 0xff) {
				calc.portal_ceiling = true;
			}

			int8_t floor_height = room_sector.floor;
			int8_t ceiling_height = room_sector.ceiling;
			uint16_t floor_data_index = room_sector.floor_data_index;

			GeometryShift geo_shift = parse_floor_data_entry(p_floor_data, floor_data_index);
			if (geo_shift.portal_room != 0xff) {
				calc.portal_wall = true;
				if (geo_shift.portal_room < p_rooms.size()) {
					// Fetch the sector data from the adjoining room.
					TRRoom new_room = p_rooms.get(geo_shift.portal_room);
					TRRoomSector original_room_sector = room_sector;

					uint32_t current_room_x = (p_current_room.info.x >> 10);
					uint32_t current_room_z = (p_current_room.info.z >> 10);

					// SWAP
					uint32_t global_x_sector = (current_room_x + x_sector);
					uint32_t global_y_sector = (current_room_z + y_sector);

					uint32_t new_room_x = (new_room.info.x >> 10);
					uint32_t new_room_z = (new_room.info.z >> 10);

					// SWAP
					uint32_t new_room_local_x = (global_x_sector - new_room_x);
					uint32_t new_room_local_y = (global_y_sector - new_room_z);

					int32_t new_sector_id = (new_room_local_x * new_room.sector_count_x) + new_room_local_y;
					room_sector = new_room.sectors.get(new_sector_id);

					room_below = room_sector.room_below;
					room_above = room_sector.room_above;

					if (room_below != 0xff) {
						calc.portal_floor = true;
					}

					if (room_above != 0xff) {
						calc.portal_ceiling = true;
					}

					int8_t floor_max = p_current_room.info.y_top >> 8;
					if (room_sector.floor < floor_max) {
						floor_height = floor_max;
					} else {
						floor_height = room_sector.floor;
					}

					int8_t ceiling_max = p_current_room.info.y_bottom >> 8;
					if (room_sector.ceiling > ceiling_max) {
						ceiling_height = ceiling_max;
					}
					else {
						ceiling_height = room_sector.ceiling;
					}

					ceiling_height = room_sector.ceiling;

					floor_data_index = room_sector.floor_data_index;

					geo_shift = parse_floor_data_entry(p_floor_data, floor_data_index);
				}
			}


			int16_t n_floor_offset = (geo_shift.z_floor_shift > 0 ? -geo_shift.z_floor_shift : 0);
			int16_t e_floor_offset = (geo_shift.x_floor_shift < 0 ? geo_shift.x_floor_shift : 0);
			int16_t s_floor_offset = (geo_shift.z_floor_shift < 0 ? geo_shift.z_floor_shift : 0);
			int16_t w_floor_offset = (geo_shift.x_floor_shift > 0 ? -geo_shift.x_floor_shift : 0);

			int16_t ne_floor_height = -floor_height + n_floor_offset + e_floor_offset;
			int16_t se_floor_height = -floor_height + s_floor_offset + e_floor_offset;
			int16_t sw_floor_height = -floor_height + s_floor_offset + w_floor_offset;
			int16_t nw_floor_height = -floor_height + n_floor_offset + w_floor_offset;

			int16_t n_ceiling_offset = (geo_shift.z_ceiling_shift > 0 ? geo_shift.z_ceiling_shift : 0);
			int16_t e_ceiling_offset = (geo_shift.x_ceiling_shift > 0 ? geo_shift.x_ceiling_shift : 0);
			int16_t s_ceiling_offset = (geo_shift.z_ceiling_shift < 0 ? -geo_shift.z_ceiling_shift : 0);
			int16_t w_ceiling_offset = (geo_shift.x_ceiling_shift < 0 ? -geo_shift.x_ceiling_shift : 0);

			int16_t ne_ceiling_height = -ceiling_height + n_ceiling_offset + e_ceiling_offset;
			int16_t se_ceiling_height = -ceiling_height + s_ceiling_offset + e_ceiling_offset;
			int16_t sw_ceiling_height = -ceiling_height + s_ceiling_offset + w_ceiling_offset;
			int16_t nw_ceiling_height = -ceiling_height + n_ceiling_offset + w_ceiling_offset;

			if (0) {
				calc.solid = false;

				if (!calc.portal_floor) {
					real_t ne_offset_f = (real_t)(ne_floor_height) * CLICK_SIZE;
					real_t se_offset_f = (real_t)(se_floor_height) * CLICK_SIZE;
					real_t sw_offset_f = (real_t)(sw_floor_height) * CLICK_SIZE;
					real_t nw_offset_f = (real_t)(nw_floor_height) * CLICK_SIZE;

					calc.floor_north_east = ne_offset_f;
					calc.floor_south_east = se_offset_f;
					calc.floor_north_west = nw_offset_f;
					calc.floor_south_west = sw_offset_f;
				}

				if (!calc.portal_ceiling) {
					real_t ne_offset_f = (real_t)(ne_ceiling_height) * CLICK_SIZE;
					real_t se_offset_f = (real_t)(se_ceiling_height) * CLICK_SIZE;
					real_t sw_offset_f = (real_t)(sw_ceiling_height) * CLICK_SIZE;
					real_t nw_offset_f = (real_t)(nw_ceiling_height) * CLICK_SIZE;

					calc.ceiling_north_east = ne_offset_f;
					calc.ceiling_south_east = se_offset_f;
					calc.ceiling_north_west = nw_offset_f;
					calc.ceiling_south_west = sw_offset_f;
				}
			} else {
				if (calc.portal_floor) {
					calc.floor_north_east = (real_t)(-floor_height) * CLICK_SIZE;
					calc.floor_north_west = (real_t)(-floor_height) * CLICK_SIZE;
					calc.floor_south_east = (real_t)(-floor_height) * CLICK_SIZE;
					calc.floor_south_west = (real_t)(-floor_height) * CLICK_SIZE;
					calc.solid = false;
				}

				if (calc.portal_ceiling) {
					calc.ceiling_north_east = (real_t)(-ceiling_height) * CLICK_SIZE;
					calc.ceiling_north_west = (real_t)(-ceiling_height) * CLICK_SIZE;
					calc.ceiling_south_east = (real_t)(-ceiling_height) * CLICK_SIZE;
					calc.ceiling_south_west = (real_t)(-ceiling_height) * CLICK_SIZE;
					calc.solid = false;
				}
			}

			// floor
			if ((ne_ceiling_height != ne_floor_height
				|| se_ceiling_height != se_floor_height
				|| sw_ceiling_height != sw_floor_height
				|| nw_ceiling_height != nw_floor_height) && floor_height != -127 && room_below == 0xff) {

				real_t ne_offset_f = (real_t)(ne_floor_height) * CLICK_SIZE;
				real_t se_offset_f = (real_t)(se_floor_height) * CLICK_SIZE;
				real_t sw_offset_f = (real_t)(sw_floor_height) * CLICK_SIZE;
				real_t nw_offset_f = (real_t)(nw_floor_height) * CLICK_SIZE;

				calc.floor_north_east = ne_offset_f;
				calc.floor_south_east = se_offset_f;
				calc.floor_north_west = nw_offset_f;
				calc.floor_south_west = sw_offset_f;

				if (!calc.portal_wall) {
					buf.append(Vector3(SQUARE_SIZE * x_sector, nw_offset_f, -(SQUARE_SIZE * y_sector)));
					buf.append(Vector3(SQUARE_SIZE * x_sector + SQUARE_SIZE, ne_offset_f, -(SQUARE_SIZE * y_sector)));
					buf.append(Vector3(SQUARE_SIZE * x_sector, sw_offset_f, -(SQUARE_SIZE * y_sector + SQUARE_SIZE)));

					buf.append(Vector3(SQUARE_SIZE * x_sector, sw_offset_f, -(SQUARE_SIZE * y_sector + SQUARE_SIZE)));
					buf.append(Vector3(SQUARE_SIZE * x_sector + SQUARE_SIZE, se_offset_f, -(SQUARE_SIZE * y_sector + SQUARE_SIZE)));
					buf.append(Vector3(SQUARE_SIZE * x_sector + SQUARE_SIZE, ne_offset_f, -(SQUARE_SIZE * y_sector)));
				}

				calc.solid = false;
			}

			// ceiling
			if ((ne_ceiling_height != ne_floor_height
				|| se_ceiling_height != se_floor_height
				|| sw_ceiling_height != sw_floor_height
				|| nw_ceiling_height != nw_floor_height) && ceiling_height != -127 && room_above == 0xff) {

				real_t ne_offset_f = (real_t)(ne_ceiling_height) * CLICK_SIZE;
				real_t se_offset_f = (real_t)(se_ceiling_height) * CLICK_SIZE;
				real_t sw_offset_f = (real_t)(sw_ceiling_height) * CLICK_SIZE;
				real_t nw_offset_f = (real_t)(nw_ceiling_height) * CLICK_SIZE;

				calc.ceiling_north_east = ne_offset_f;
				calc.ceiling_south_east = se_offset_f;
				calc.ceiling_north_west = nw_offset_f;
				calc.ceiling_south_west = sw_offset_f;

				if (!calc.portal_wall) {
					buf.append(Vector3(SQUARE_SIZE * x_sector, nw_offset_f, -(SQUARE_SIZE * y_sector)));
					buf.append(Vector3(SQUARE_SIZE * x_sector + SQUARE_SIZE, ne_offset_f, -(SQUARE_SIZE * y_sector)));
					buf.append(Vector3(SQUARE_SIZE * x_sector, sw_offset_f, -(SQUARE_SIZE * y_sector + SQUARE_SIZE)));

					buf.append(Vector3(SQUARE_SIZE * x_sector, sw_offset_f, -(SQUARE_SIZE * y_sector + SQUARE_SIZE)));
					buf.append(Vector3(SQUARE_SIZE * x_sector + SQUARE_SIZE, se_offset_f, -(SQUARE_SIZE * y_sector + SQUARE_SIZE)));
					buf.append(Vector3(SQUARE_SIZE * x_sector + SQUARE_SIZE, ne_offset_f, -(SQUARE_SIZE * y_sector)));
				}

				calc.solid = false;
			}

			calculated.set(current_sector, calc);
			current_sector++;
		}
	}

	current_sector = 0;

	// Walls
	for (int32_t x_sector = 0; x_sector < p_current_room.sector_count_z; x_sector++) {
		for (int32_t y_sector = 0; y_sector < p_current_room.sector_count_x; y_sector++) {
			GeometryCalculation calc = calculated.get(current_sector);

			if (x_sector >= 1 && x_sector < p_current_room.sector_count_z - 1 && y_sector >= 1 && y_sector < p_current_room.sector_count_x) {
				if (!calc.solid && !calc.portal_wall) {
					int32_t north_sector_id = current_sector - 1;
					int32_t south_sector_id = current_sector + 1;
					int32_t east_sector_id = current_sector + (p_current_room.sector_count_x);
					int32_t west_sector_id = current_sector - (p_current_room.sector_count_x);

					GeometryCalculation north_calc = calculated.get(north_sector_id);
					GeometryCalculation south_calc = calculated.get(south_sector_id);
					GeometryCalculation east_calc = calculated.get(east_sector_id);
					GeometryCalculation west_calc = calculated.get(west_sector_id);

					// North
					{
						Vector3 current_bottom_left =	Vector3((SQUARE_SIZE * x_sector),				calc.floor_north_west,		-(SQUARE_SIZE * y_sector));
						Vector3 current_bottom_right =	Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE,	calc.floor_north_east,		-(SQUARE_SIZE * y_sector));
						Vector3 current_top_left =		Vector3((SQUARE_SIZE * x_sector),				calc.ceiling_north_west,	-(SQUARE_SIZE * y_sector));
						Vector3 current_top_right =		Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE,	calc.ceiling_north_east,	-(SQUARE_SIZE * y_sector));

						if (north_calc.solid) {
							buf.append(current_bottom_left);
							buf.append(current_bottom_right);
							buf.append(current_top_left);

							buf.append(current_bottom_right);
							buf.append(current_top_right);
							buf.append(current_top_left);
						} else {
							Vector3 next_bottom_left =	Vector3((SQUARE_SIZE * x_sector),				north_calc.floor_south_west,	-(SQUARE_SIZE * y_sector));
							Vector3 next_bottom_right =	Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE,	north_calc.floor_south_east,	-(SQUARE_SIZE * y_sector));
							Vector3 next_top_left =		Vector3((SQUARE_SIZE * x_sector),				north_calc.ceiling_south_west,	-(SQUARE_SIZE * y_sector));
							Vector3 next_top_right =	Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE,	north_calc.ceiling_south_east,	-(SQUARE_SIZE * y_sector));

							// Bottom quad difference
							if (((current_bottom_left.y - next_bottom_left.y) < 0.0 || (current_bottom_right.y - next_bottom_right.y) < 0.0)) {
								if ((current_bottom_left.y - next_bottom_left.y) < CMP_EPSILON) {
									buf.append(current_bottom_left);
									buf.append(current_bottom_right);
									buf.append(next_bottom_left);
								}

								if ((current_bottom_right.y - next_bottom_right.y) < CMP_EPSILON) {
									buf.append(current_bottom_right);
									buf.append(next_bottom_right);
									buf.append(next_bottom_left);
								}
							}

							// Top quad difference
							if (((current_top_left.y - next_top_left.y) > 0.0 || (current_top_right.y - next_top_right.y) > 0.0)) {
								if ((current_top_left.y - next_top_left.y) > CMP_EPSILON) {
									buf.append(current_top_left);
									buf.append(current_top_right);
									buf.append(next_top_left);
								}

								if (((current_top_right.y - next_top_right.y) > CMP_EPSILON)) {
									buf.append(current_top_right);
									buf.append(next_top_right);
									buf.append(next_top_left);
								}
							}
						}
					}

					// South
					{
						Vector3 current_bottom_left =	Vector3((SQUARE_SIZE * x_sector),				calc.floor_south_west,		-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
						Vector3 current_bottom_right =	Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE,	calc.floor_south_east,		-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
						Vector3 current_top_left =		Vector3((SQUARE_SIZE * x_sector),				calc.ceiling_south_west,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
						Vector3 current_top_right =		Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE,	calc.ceiling_south_east,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);

						if (south_calc.solid) {
							buf.append(current_bottom_left);
							buf.append(current_bottom_right);
							buf.append(current_top_left);

							buf.append(current_bottom_right);
							buf.append(current_top_right);
							buf.append(current_top_left);
						} else {
							Vector3 next_bottom_left =	Vector3((SQUARE_SIZE * x_sector),				south_calc.floor_north_west,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
							Vector3 next_bottom_right =	Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE,	south_calc.floor_north_east,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
							Vector3 next_top_left =		Vector3((SQUARE_SIZE * x_sector),				south_calc.ceiling_north_west,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
							Vector3 next_top_right =	Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE,	south_calc.ceiling_north_east,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
						
							// Bottom quad difference
							if (((current_bottom_left.y - next_bottom_left.y) < 0.0 || (current_bottom_right.y - next_bottom_right.y) < 0.0)) {
								if ((current_bottom_left.y - next_bottom_left.y) < CMP_EPSILON) {
									buf.append(current_bottom_left);
									buf.append(current_bottom_right);
									buf.append(next_bottom_left);
								}

								if ((current_bottom_right.y - next_bottom_right.y) < CMP_EPSILON) {
									buf.append(current_bottom_right);
									buf.append(next_bottom_right);
									buf.append(next_bottom_left);
								}
							}

							// Top quad difference
							if (((current_top_left.y - next_top_left.y) > 0.0 || (current_top_right.y - next_top_right.y) > 0.0)) {
								if ((current_top_left.y - next_top_left.y) > CMP_EPSILON) {
									buf.append(current_top_left);
									buf.append(current_top_right);
									buf.append(next_top_left);
								}

								if (((current_top_right.y - next_top_right.y) > CMP_EPSILON)) {
									buf.append(current_top_right);
									buf.append(next_top_right);
									buf.append(next_top_left);
								}
							}
						}
					}

					// East
					{
						Vector3 current_bottom_left =	Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE, calc.floor_north_east,		-(SQUARE_SIZE * y_sector));
						Vector3 current_bottom_right =	Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE, calc.floor_south_east,		-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
						Vector3 current_top_left =		Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE, calc.ceiling_north_east,	-(SQUARE_SIZE * y_sector));
						Vector3 current_top_right =		Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE, calc.ceiling_south_east,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);

						if (east_calc.solid) {
							buf.append(current_bottom_left);
							buf.append(current_bottom_right);
							buf.append(current_top_left);

							buf.append(current_bottom_right);
							buf.append(current_top_right);
							buf.append(current_top_left);
						} else {
							Vector3 next_bottom_left =	Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE, east_calc.floor_north_west,		-(SQUARE_SIZE * y_sector));
							Vector3 next_bottom_right =	Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE, east_calc.floor_south_west,		-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
							Vector3 next_top_left =		Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE, east_calc.ceiling_north_west,	-(SQUARE_SIZE * y_sector));
							Vector3 next_top_right =	Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE, east_calc.ceiling_south_west,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);

							// Bottom quad difference
							if (((current_bottom_left.y - next_bottom_left.y) < 0.0 || (current_bottom_right.y - next_bottom_right.y) < 0.0)) {
								if ((current_bottom_left.y - next_bottom_left.y) < CMP_EPSILON) {
									buf.append(current_bottom_left);
									buf.append(current_bottom_right);
									buf.append(next_bottom_left);
								}

								if ((current_bottom_right.y - next_bottom_right.y) < CMP_EPSILON) {
									buf.append(current_bottom_right);
									buf.append(next_bottom_right);
									buf.append(next_bottom_left);
								}
							}

							// Top quad difference
							if (((current_top_left.y - next_top_left.y) > 0.0 || (current_top_right.y - next_top_right.y) > 0.0)) {
								if ((current_top_left.y - next_top_left.y) > CMP_EPSILON) {
									buf.append(current_top_left);
									buf.append(current_top_right);
									buf.append(next_top_left);
								}

								if (((current_top_right.y - next_top_right.y) > CMP_EPSILON)) {
									buf.append(current_top_right);
									buf.append(next_top_right);
									buf.append(next_top_left);
								}
							}
						}
					}

					// West
					{
						Vector3 current_bottom_left =	Vector3((SQUARE_SIZE * x_sector), calc.floor_north_west,	-(SQUARE_SIZE * y_sector));
						Vector3 current_bottom_right =	Vector3((SQUARE_SIZE * x_sector), calc.floor_south_west,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
						Vector3 current_top_left =		Vector3((SQUARE_SIZE * x_sector), calc.ceiling_north_west,	-(SQUARE_SIZE * y_sector));
						Vector3 current_top_right =		Vector3((SQUARE_SIZE * x_sector), calc.ceiling_south_west,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);

						if (west_calc.solid) {
							buf.append(current_bottom_left);
							buf.append(current_bottom_right);
							buf.append(current_top_left);

							buf.append(current_bottom_right);
							buf.append(current_top_right);
							buf.append(current_top_left);
						} else {
							Vector3 next_bottom_left =	Vector3((SQUARE_SIZE * x_sector), west_calc.floor_north_east,	-(SQUARE_SIZE * y_sector));
							Vector3 next_bottom_right =	Vector3((SQUARE_SIZE * x_sector), west_calc.floor_south_east,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
							Vector3 next_top_left =		Vector3((SQUARE_SIZE * x_sector), west_calc.ceiling_north_east,	-(SQUARE_SIZE * y_sector));
							Vector3 next_top_right =	Vector3((SQUARE_SIZE * x_sector), west_calc.ceiling_south_east,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
							
							// Bottom quad difference
							if (((current_bottom_left.y - next_bottom_left.y) < 0.0 || (current_bottom_right.y - next_bottom_right.y) < 0.0)) {
								if ((current_bottom_left.y - next_bottom_left.y) < CMP_EPSILON) {
									buf.append(current_bottom_left);
									buf.append(current_bottom_right);
									buf.append(next_bottom_left);
								}

								if ((current_bottom_right.y - next_bottom_right.y) < CMP_EPSILON) {
									buf.append(current_bottom_right);
									buf.append(next_bottom_right);
									buf.append(next_bottom_left);
								}
							}

							// Top quad difference
							if (((current_top_left.y - next_top_left.y) > 0.0 || (current_top_right.y - next_top_right.y) > 0.0)) {
								if ((current_top_left.y - next_top_left.y) > CMP_EPSILON) {
									buf.append(current_top_left);
									buf.append(current_top_right);
									buf.append(next_top_left);
								}

								if (((current_top_right.y - next_top_right.y) > CMP_EPSILON)) {
									buf.append(current_top_right);
									buf.append(next_top_right);
									buf.append(next_top_left);
								}
							}
						}
					}
				}
			}
			current_sector++;
		}
	}

	collision_data->set_faces(buf);
	collision_data->set_backface_collision_enabled(true);

	CollisionShape3D *sbar = memnew(CollisionShape3D);
	sbar->set_shape(collision_data);
	sbar->set_name("CollisionShape3D");

	return sbar;
}


Ref<Material> generate_tr_generic_material(Ref<ImageTexture> p_image_texture, bool p_is_transparent) {
	Ref<StandardMaterial3D> new_material = memnew(StandardMaterial3D);

	new_material->set_diffuse_mode(BaseMaterial3D::DIFFUSE_LAMBERT_WRAP);
	new_material->set_shading_mode(BaseMaterial3D::SHADING_MODE_PER_PIXEL);
	new_material->set_specular(0.0);
	new_material->set_roughness(1.0);
	new_material->set_metallic(0.0);
	new_material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	new_material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, p_image_texture);
	new_material->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_NEAREST);
	if (p_is_transparent) {
		new_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
	}

	return new_material;
}

Ref<Material> generate_tr_shader_material(Ref<ImageTexture> p_image_texture, Ref<Shader> p_shader) {
	Ref<ShaderMaterial> new_material = memnew(ShaderMaterial);

	new_material->set_shader(p_shader);
	new_material->set_shader_parameter("texture_albedo", p_image_texture);

	return new_material;
}


void set_owner_recursively(Node *p_node, Node *p_owner) {
	p_node->set_owner(p_owner);
	for (int32_t i = 0; i < p_node->get_child_count(); i++) {
		set_owner_recursively(p_node->get_child(i), p_owner);
	}
}

Node3D *generate_godot_scene(
	Node *p_root,
	TRLevelData level_data,
	bool p_lara_only) {

	Vector<Ref<Material>> level_materials;
	Vector<Ref<Material>> level_trans_materials;
	Vector<Ref<Material>> entity_materials;
	Vector<Ref<Material>> entity_trans_materials;

	Ref<Shader> level_shader = memnew(Shader);
	level_shader->set_code("shader_type spatial;\n\
		render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx, ambient_light_disabled;\n\
		\n\
		uniform sampler2D texture_albedo : source_color, filter_nearest_mipmap;\n\
		\n\
		void fragment() {\n\
			vec2 base_uv = UV;\n\
			vec4 albedo_tex = texture(texture_albedo, base_uv);\n\
			ALBEDO = vec3(0.0, 0.0, 0.0);\n\
			float inv_gamma =  1.0 / 2.2;\n\
			float gamma = 2.2;\n\
			vec3 color_gamma = pow(albedo_tex.rgb, vec3(inv_gamma, inv_gamma, inv_gamma));\n\
			color_gamma *= COLOR.rgb;\n\
			color_gamma *= 2.0;\n\
			EMISSION.rgb = pow(color_gamma, vec3(gamma, gamma, gamma));\n\
			METALLIC = 0.0;\n\
			SPECULAR = 1.0;\n\
			ROUGHNESS = 1.0;\n\
		}");

	Ref<Shader> level_trans_shader = memnew(Shader);
	level_trans_shader->set_code("shader_type spatial;\n\
		render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx, ambient_light_disabled;\n\
		\n\
		uniform sampler2D texture_albedo : source_color, filter_nearest_mipmap;\n\
		\n\
		void fragment() {\n\
			vec2 base_uv = UV;\n\
			vec4 albedo_tex = texture(texture_albedo, base_uv);\n\
			ALBEDO = vec3(0.0, 0.0, 0.0);\n\
			float inv_gamma =  1.0 / 2.2;\n\
			float gamma = 2.2;\n\
			vec3 color_gamma = pow(albedo_tex.rgb, vec3(inv_gamma, inv_gamma, inv_gamma));\n\
			color_gamma *= COLOR.rgb;\n\
			color_gamma *= 2.0;\n\
			EMISSION.rgb = pow(color_gamma, vec3(gamma, gamma, gamma));\n\
			METALLIC = 0.0;\n\
			SPECULAR = 1.0;\n\
			ROUGHNESS = 1.0;\n\
			ALPHA = albedo_tex.a;\n\
			ALPHA_SCISSOR_THRESHOLD = 1.0;\n\
		}");

	Ref<Material> palette_material;
	
	// Palette Texture
	if (!level_data.palette.is_empty()) {
		Ref<Image> palette_image = memnew(Image(TR_TEXTILE_SIZE, TR_TEXTILE_SIZE, false, Image::FORMAT_RGBA8));
		for (int32_t x = 0; x < TR_TEXTILE_SIZE; x++) {
			for (int32_t y = 0; y < TR_TEXTILE_SIZE; y++) {
				uint32_t index = ((y / 16) * 16) + (x / 16);

				TRColor3 color = level_data.palette.get(index);
				palette_image->set_pixel(x, y, Color(((float)color.r / 255.0f), ((float)color.g / 255.0f), ((float)color.b / 255.0f), 1.0f));
			}
		}
		Ref<ImageTexture> it = ImageTexture::create_from_image(palette_image);
		palette_material = generate_tr_generic_material(it, false);

		String palette_path = String("Pal") + String(".png");
		palette_image->save_png(palette_path);
	}

	Vector<Ref<ImageTexture>> image_textures;

	for (int32_t i = 0; i < level_data.level_textures.size(); i++) {
		PackedByteArray current_texture = level_data.level_textures.get(i);
		Ref<Image> image = memnew(Image(TR_TEXTILE_SIZE, TR_TEXTILE_SIZE, false, Image::FORMAT_RGBA8));
		if (level_data.texture_type == TR_TEXTURE_TYPE_8_PAL) {
			for (int32_t x = 0; x < TR_TEXTILE_SIZE; x++) {
				for (int32_t y = 0; y < TR_TEXTILE_SIZE; y++) {
					uint8_t index = current_texture.get((y * TR_TEXTILE_SIZE) + x);

					if (index == 0x00) {
						TRColor3 color = level_data.palette.get(index);
						image->set_pixel(x, y, Color(0.0f, 0.0f, 0.0f, 0.0f));
					}
					else {
						TRColor3 color = level_data.palette.get(index);
						image->set_pixel(x, y, Color(((float)color.r / 255.0f), ((float)color.g / 255.0f), ((float)color.b / 255.0f), 1.0f));
					}
				}
			}
		} else if (level_data.texture_type == TR_TEXTURE_TYPE_32) {
			for (int32_t x = 0; x < TR_TEXTILE_SIZE; x++) {
				for (int32_t y = 0; y < TR_TEXTILE_SIZE; y++) {
					uint32_t index = ((y * TR_TEXTILE_SIZE) + x) * sizeof(uint32_t);
					uint8_t b = current_texture.get(index + 0);
					uint8_t g = current_texture.get(index + 1);
					uint8_t r = current_texture.get(index + 2);
					uint8_t a = current_texture.get(index + 3);

					image->set_pixel(x, y, Color(((float)r / 255.0f), ((float)g / 255.0f), ((float)b / 255.0f), a / 255.0f));
				}
			}
		}

#if 0
		String image_path = String("LevelTexture_") + itos(i) + String(".png");
		image->save_png(image_path);
#endif

		image_textures.push_back(ImageTexture::create_from_image(image));
	}

	for (int32_t i = 0; i < level_data.entity_textures.size(); i++) {
		PackedByteArray current_texture = level_data.entity_textures.get(i);
		Ref<Image> image = memnew(Image(TR_TEXTILE_SIZE, TR_TEXTILE_SIZE, false, Image::FORMAT_RGBA8));
		if (level_data.texture_type == TR_TEXTURE_TYPE_8_PAL) {
			for (int32_t x = 0; x < TR_TEXTILE_SIZE; x++) {
				for (int32_t y = 0; y < TR_TEXTILE_SIZE; y++) {
					uint8_t index = current_texture.get((y * TR_TEXTILE_SIZE) + x);

					if (index == 0x00) {
						TRColor3 color = level_data.palette.get(index);
						image->set_pixel(x, y, Color(0.0f, 0.0f, 0.0f, 0.0f));
					}
					else {
						TRColor3 color = level_data.palette.get(index);
						image->set_pixel(x, y, Color(((float)color.r / 255.0f), ((float)color.g / 255.0f), ((float)color.b / 255.0f), 1.0f));
					}
				}
			}
		} else if (level_data.texture_type == TR_TEXTURE_TYPE_32) {
			for (int32_t x = 0; x < TR_TEXTILE_SIZE; x++) {
				for (int32_t y = 0; y < TR_TEXTILE_SIZE; y++) {
					uint32_t index = ((y * TR_TEXTILE_SIZE) + x) * sizeof(uint32_t);
					uint8_t b = current_texture.get(index + 0);
					uint8_t g = current_texture.get(index + 1);
					uint8_t r = current_texture.get(index + 2);
					uint8_t a = current_texture.get(index + 3);

					image->set_pixel(x, y, Color(((float)r / 255.0f), ((float)g / 255.0f), ((float)b / 255.0f), a / 255.0f));
				}
			}
		}

#if 0
		String image_path = String("EntityTexture_") + itos(i) + String(".png");
		image->save_png(image_path);
#endif

		image_textures.push_back(ImageTexture::create_from_image(image));
	}

	for (int32_t i = 0; i < image_textures.size(); i++) {
		level_materials.append(generate_tr_shader_material(image_textures[i], level_shader));
		level_trans_materials.append(generate_tr_shader_material(image_textures[i], level_trans_shader));
		entity_materials.append(generate_tr_generic_material(image_textures[i], false));
		entity_trans_materials.append(generate_tr_generic_material(image_textures[i], true));
	}


	Vector<Ref<ArrayMesh>> meshes;
	for (TRMesh& tr_mesh : level_data.types.meshes) {
		Ref<ArrayMesh> mesh = tr_mesh_to_godot_mesh(tr_mesh, entity_materials, entity_trans_materials, palette_material, level_data.types);
		meshes.push_back(mesh);
	}

	Vector<Ref<AudioStream>> samples;

	// Audio
	if (!level_data.sound_buffer.is_empty()) {
		for (TRSoundInfo tr_sound_info : level_data.sound_infos) {
			int32_t sample_id = tr_sound_info.sample_index;
			int32_t loop_mode = tr_sound_info.characteristics & 0x2;;
			int32_t sample_count = (tr_sound_info.characteristics >> 2) & 0xF;

			Ref<AudioStreamRandomizer> sample_collection = memnew(AudioStreamRandomizer);

			switch (loop_mode) {
			case 0:
				sample_collection->set_meta("tr_loop_mode", "looping_disabled");
				break;
			case 1:
				sample_collection->set_meta("tr_loop_mode", "one_shot_rewind");
				break;
			case 2:
				sample_collection->set_meta("tr_loop_mode", "looping_enabled");
				break;
			case 3:
				sample_collection->set_meta("tr_loop_mode", "one_shot_wait");
				break;
			}

			sample_collection->set_playback_mode(AudioStreamRandomizer::PLAYBACK_RANDOM);

			for (int32_t i = 0; i < sample_count; i++) {
				uint32_t sample_offset = level_data.sound_indices.get(sample_id + i);
				Ref<TRFileAccess> buffer_access = TRFileAccess::create_from_buffer(level_data.sound_buffer);

				Ref<AudioStreamWAV> sample = memnew(AudioStreamWAV);
				if (sample_offset == 0xffffffff) {
					continue;
				}

				buffer_access->seek(sample_offset);
				if (buffer_access->get_fixed_string(4) == "RIFF") {
					uint32_t file_size = buffer_access->get_u32();
					if (file_size > 0) {
						if (buffer_access->get_fixed_string(4) == "WAVE") {
							if (buffer_access->get_fixed_string(4) == "fmt ") {
								uint32_t length = buffer_access->get_u32();
								uint16_t format_type = buffer_access->get_u16();
								ERR_FAIL_COND_V(format_type != 1, nullptr);
								uint16_t channels = buffer_access->get_u16();
								ERR_FAIL_COND_V(channels <= 0 || channels > 2, nullptr);
								uint32_t mix_rate = buffer_access->get_u32();
								uint32_t byte_per_second = buffer_access->get_u32();
								uint16_t byte_per_block = buffer_access->get_u16();
								uint16_t bits_per_sample = buffer_access->get_u16();
								ERR_FAIL_COND_V(bits_per_sample != 16 && bits_per_sample != 8, nullptr);

								if (buffer_access->get_fixed_string(4) == "data") {
									uint32_t buffer_size = buffer_access->get_u32();
									PackedByteArray pba = buffer_access->get_buffer(buffer_size);
									if (bits_per_sample <= 8) {
										for (int32_t buf_idx = 0; buf_idx < pba.size(); buf_idx++) {
											pba.ptrw()[buf_idx] = pba.ptr()[buf_idx] - 128;
										}
									}
									sample->set_format(bits_per_sample == 16 ? AudioStreamWAV::FORMAT_16_BITS : AudioStreamWAV::FORMAT_8_BITS);
									sample->set_stereo(channels == 1 ? false : true);
									sample->set_mix_rate(mix_rate);
									sample->set_loop_mode(loop_mode == 2 ? AudioStreamWAV::LOOP_FORWARD : AudioStreamWAV::LOOP_DISABLED);
									sample->set_data(pba);
									sample->set_loop_begin(0);
									sample->set_loop_end(buffer_size);

									sample_collection->add_stream(i, sample);
								}
							}
						}
					}
				}
			}
			samples.append(sample_collection);
		}
	}

	Node *scene_owner = p_root->get_owner() != nullptr ? p_root->get_owner() : p_root;

	if (!p_lara_only) {
		Node3D *rooms_node = memnew(Node3D);
		p_root->add_child(rooms_node);
		rooms_node->set_name("TRRooms");
		rooms_node->set_owner(scene_owner);

		Vector<Node3D*> moveable_node = create_nodes_for_moveables(
			level_data.types.moveable_info_map,
			level_data.types,
			meshes,
			samples,
			level_data.format);

#if 0
		for (Node3D *moveable : moveable_node) {
			rooms_node->add_child(moveable);
			set_owner_recursively(moveable, scene_owner);
			moveable->set_display_folded(true);
		}
#endif

		Node3D *entities_node = memnew(Node3D);
		p_root->add_child(entities_node);
		entities_node->set_name("TREntities");
		entities_node->set_owner(scene_owner);

		uint32_t entity_idx = 0;
		for (const TREntity& entity : level_data.entities) {
			Node3D *entity_node = memnew(Node3D);

			real_t degrees_rotation = (real_t)((entity.transform.rot.y / 16384.0) * -90.0) + 180.0;
			entity_node->set_transform(Transform3D(Basis().rotated(
				Vector3(0.0, 1.0, 0.0), Math::deg_to_rad(degrees_rotation)), Vector3(
					entity.transform.pos.x * TR_TO_GODOT_SCALE,
					entity.transform.pos.y * -TR_TO_GODOT_SCALE,
					entity.transform.pos.z * -TR_TO_GODOT_SCALE)));

			entities_node->add_child(entity_node);
			entity_node->set_name("Entity_" + itos(entity_idx) + " (" + get_type_info_name(entity.type_id, level_data.format) + ")");
			entity_node->set_owner(scene_owner);
			entity_node->set_display_folded(true);

			if (level_data.types.moveable_info_map.has(entity.type_id)) {
				Node3D *new_node = create_godot_moveable_model(
					entity.type_id,
					level_data.types.moveable_info_map[entity.type_id],
					level_data.types,
					meshes,
					samples,
					level_data.format,
					false);
				ERR_FAIL_NULL_V(new_node, nullptr);
				Node3D *type = new_node;
				entity_node->add_child(type);
				set_owner_recursively(type, scene_owner);
				type->set_display_folded(true);
			}
			entity_idx++;
		}
	

		uint32_t room_idx = 0;
		for (const TRRoom& room : level_data.rooms) {
			Node3D *node_3d = memnew(Node3D);
			node_3d->set_name(String("Room_") + itos(room_idx));
			node_3d->set_display_folded(true);
			rooms_node->add_child(node_3d);
			node_3d->set_position(Vector3(room.info.x * TR_TO_GODOT_SCALE, 0.0, room.info.z * -TR_TO_GODOT_SCALE));
			node_3d->set_owner(rooms_node->get_owner());

			if (node_3d) {
				MeshInstance3D* mi = memnew(MeshInstance3D);
				if (mi) {
					mi->set_name(String("RoomMesh_") + itos(room_idx));
					node_3d->add_child(mi);

					Ref<ArrayMesh> mesh = tr_room_data_to_godot_mesh(room.data, level_materials, level_trans_materials, level_data.types);

					mi->set_position(Vector3());
					mi->set_owner(scene_owner);
					mi->set_mesh(mesh);
					mi->set_layer_mask(1 << 0);
				}

				StaticBody3D* static_body = memnew(StaticBody3D);
				if (static_body) {
					static_body->set_name(String("RoomStaticBody3D_") + itos(room_idx));
					node_3d->add_child(static_body);
					static_body->set_owner(scene_owner);
					static_body->set_position(Vector3());

					CollisionShape3D* collision_shape = tr_room_to_collision_shape(room, level_data.floor_data, level_data.rooms);

					static_body->add_child(collision_shape);
					collision_shape->set_owner(scene_owner);
					collision_shape->set_position(Vector3());
				}

				uint32_t static_mesh_idx = 0;
				for (const TRRoomStaticMesh& room_static_mesh : room.room_static_meshes) {
					int32_t mesh_static_number = room_static_mesh.mesh_id;

					if (level_data.types.static_info_map.has(mesh_static_number)) {
						TRStaticInfo static_info = level_data.types.static_info_map[mesh_static_number];
					}

					if (level_data.types.static_info_map.has(mesh_static_number)) {
						TRStaticInfo static_info = level_data.types.static_info_map[mesh_static_number];
						if (static_info.flags & 2) {
							int32_t mesh_number = static_info.mesh_number;
							if (mesh_number < meshes.size() && mesh_number >= 0) {
								Ref<ArrayMesh> mesh = meshes.get(mesh_number);
								MeshInstance3D* mi = memnew(MeshInstance3D);
								node_3d->add_child(mi);
								mi->set_mesh(mesh);
								mi->set_name(String("StaticMeshInstance_") + itos(static_mesh_idx));
								mi->set_owner(scene_owner);
								mi->set_transform(node_3d->get_transform().affine_inverse() * Transform3D(Basis().rotated(
									Vector3(0.0, 1.0, 0.0), Math::deg_to_rad((float)room_static_mesh.rotation / 16384.0f * -90)), Vector3(
										room_static_mesh.pos.x * TR_TO_GODOT_SCALE,
										room_static_mesh.pos.y * -TR_TO_GODOT_SCALE,
										room_static_mesh.pos.z * -TR_TO_GODOT_SCALE)));
								mi->set_layer_mask(1 << 0);
							}
						}
					}
				}
				static_mesh_idx++;
			}

			room_idx++;
		}
		return rooms_node;
	} else {
		Node3D *new_node = create_godot_moveable_model(
			0,
			level_data.types.moveable_info_map[0],
			level_data.types,
			meshes,
			samples,
			level_data.format,
			true);
		ERR_FAIL_COND_V(!new_node, nullptr);

		p_root->add_child(new_node);
		new_node->set_owner(scene_owner);
		set_owner_recursively(new_node, scene_owner);
		new_node->set_display_folded(true);

		return new_node;
	}
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

void dump_8bit_textures(Vector<PackedByteArray> p_textures, Vector<TRColor3> p_palette) {
	// Image Textures
	for (int32_t i = 0; i < p_textures.size(); i++) {
		PackedByteArray current_texture = p_textures.get(i);
		Ref<Image> image = memnew(Image(TR_TEXTILE_SIZE, TR_TEXTILE_SIZE, false, Image::FORMAT_RGBA8));
		for (int32_t x = 0; x < TR_TEXTILE_SIZE; x++) {
			for (int32_t y = 0; y < TR_TEXTILE_SIZE; y++) {
				uint8_t index = current_texture.get((y * TR_TEXTILE_SIZE) + x);

				if (index == 0x00) {
					TRColor3 color = p_palette.get(index);
					image->set_pixel(x, y, Color(0.0f, 0.0f, 0.0f, 0.0f));
				}
				else {
					TRColor3 color = p_palette.get(index);
					image->set_pixel(x, y, Color(((float)color.r / 255.0f), ((float)color.g / 255.0f), ((float)color.b / 255.0f), 1.0f));
				}
			}
		}

		String image_path = String("res://Texture_") + itos(i) + String(".png");
		image->save_png(image_path);
	}
}

void dump_32bit_textures(Vector<PackedByteArray> p_textures) {
	// Image Textures
	for (int32_t i = 0; i < p_textures.size(); i++) {
		PackedByteArray current_texture = p_textures.get(i);
		Ref<Image> image = memnew(Image(TR_TEXTILE_SIZE, TR_TEXTILE_SIZE, false, Image::FORMAT_RGBA8));
		for (int32_t x = 0; x < TR_TEXTILE_SIZE; x++) {
			for (int32_t y = 0; y < TR_TEXTILE_SIZE; y++) {

				uint8_t b = current_texture.get(((y * TR_TEXTILE_SIZE) + x) * sizeof(uint32_t));
				uint8_t g = current_texture.get(((y * TR_TEXTILE_SIZE) + x) * sizeof(uint32_t) + 1);
				uint8_t r = current_texture.get(((y * TR_TEXTILE_SIZE) + x) * sizeof(uint32_t) + 2);
				uint8_t a = current_texture.get(((y * TR_TEXTILE_SIZE) + x) * sizeof(uint32_t) + 3);

				image->set_pixel(x, y, Color(((float)r / 255.0f), ((float)g / 255.0f), ((float)b / 255.0f), a));
			}
		}

		String image_path = String("res://Texture_") + itos(i) + String(".png");
		image->save_png(image_path);
	}
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