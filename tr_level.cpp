#include "tr_level.hpp"

#include <scene/3d/mesh_instance_3d.h>
#include <scene/3d/bone_attachment_3d.h>
#include <scene/3d/collision_shape_3d.h>
#include <scene/3d/physics_body_3d.h>
#include <scene/animation/animation_player.h>
#include <scene/resources/mesh_data_tool.h>
#include <scene/resources/concave_polygon_shape_3d.h>
#include <scene/resources/surface_tool.h>
#include <scene/resources/primitive_meshes.h>
#include <scene/resources/image_texture.h>
#include <editor/editor_file_system.h>
#include <core/io/stream_peer_gzip.h>

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

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "level_path", PROPERTY_HINT_FILE, "*.phd,*.tr2"), "set_level_path", "get_level_path");
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
	if (p_format == TR2_PC || p_format == TR3_PC) {
		room_vertex.attributes  = p_file->get_u16();
		second_shade_value = p_file->get_s16();
	}

	if (p_format == TR3_PC) {
		room_vertex.color = tr_color_shade_to_godot_color(second_shade_value);
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

	for (int i = 0; i < 3; i++) {
		face_triangle.indices[i] = p_file->get_s16();
	}
	face_triangle.tex_info_id = p_file->get_s16();

	return face_triangle;
}

TRFaceQuad read_tr_face_quad(Ref<TRFileAccess> p_file) {
	TRFaceQuad face_quad;

	for (int i = 0; i < 4; i++) {
		face_quad.indices[i] = p_file->get_s16();
	}
	face_quad.tex_info_id = p_file->get_s16();

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
	for (int i = 0; i < room_data.room_vertex_count; i++) {
		room_data.room_vertices.set(i, read_tr_room_vertex(p_file, p_format));
	}

	// Quad buffer
	room_data.room_quad_count = p_file->get_s16();
	room_data.room_quads.resize(room_data.room_quad_count);
	for (int i = 0; i < room_data.room_quad_count; i++) {
		room_data.room_quads.set(i, read_tr_face_quad(p_file));
	}

	// Triangle buffer
	room_data.room_triangle_count = p_file->get_s16();
	room_data.room_triangles.resize(room_data.room_triangle_count);
	for (int i = 0; i < room_data.room_triangle_count; i++) {
		room_data.room_triangles.set(i, read_tr_face_triangle(p_file));
	}

	// Sprite buffer
	room_data.room_sprite_count = p_file->get_s16();
	room_data.room_sprites.resize(room_data.room_sprite_count);
	for (int i = 0; i < room_data.room_sprite_count; i++) {
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

TRRoomLight read_tr_light(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	TRRoomLight room_light;

	room_light.pos = read_tr_pos(p_file);
	// Intensity
	room_light.intensity = p_file->get_u16();
	if (p_level_format == TR2_PC || p_level_format == TR3_PC) {
		room_light.intensity2 = p_file->get_u16();
	} else {
		room_light.intensity2 = room_light.intensity;
	}
	// Fade
	room_light.fade = p_file->get_u32();
	if (p_level_format == TR2_PC || p_level_format == TR3_PC) {
		room_light.fade2 = p_file->get_u32();
	} else {
		room_light.fade2 = room_light.fade;
	}

	return room_light;
}

TRRoomStaticMesh read_tr_room_static_mesh(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	TRRoomStaticMesh room_static_mesh;

	room_static_mesh.pos = read_tr_pos(p_file);
	room_static_mesh.rotation = static_cast<tr_angle>(p_file->get_s16());
	room_static_mesh.intensity = p_file->get_u16();
	if (p_level_format == TR2_PC || p_level_format == TR3_PC) {
		room_static_mesh.intensity2 = p_file->get_u16();
	} else {
		room_static_mesh.intensity2 = room_static_mesh.intensity;
	}
	room_static_mesh.mesh_id = p_file->get_u16();

	return room_static_mesh;
}

TRRoomPortal read_tr_room_portal(Ref<TRFileAccess> p_file) {
	TRRoomPortal room_portal;

	room_portal.adjoining_room = p_file->get_u16();
	room_portal.normal = read_tr_vertex(p_file);
	for (int i = 0; i < 4; i++) {
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

	room.info = read_tr_room_info(p_file);

	// Room's mesh data parsing
	uint32_t data_count = p_file->get_u32();
	uint32_t data_end = p_file->get_position() + (data_count * sizeof(uint16_t));

	room.data = read_tr_room_data(p_file, p_level_format);

	p_file->seek(data_end);

	room.portal_count = p_file->get_s16();
	room.portals.resize(room.portal_count);
	for (int i = 0; i < room.portal_count; i++) {
		room.portals.set(i, read_tr_room_portal(p_file));
	}

	room.sector_count_x = p_file->get_u16();
	room.sector_count_z = p_file->get_u16();

	uint32_t floor_sectors_total = room.sector_count_x * room.sector_count_z;
	room.sectors.resize(floor_sectors_total);
	for (uint32_t i = 0; i < floor_sectors_total; i++) {
		room.sectors.set(i, read_tr_room_sector(p_file));
	}

	room.ambient_light = p_file->get_s16();
	if (p_level_format == TR2_PC || p_level_format == TR3_PC) {
		room.ambient_light2 = p_file->get_s16();
	} else {
		room.ambient_light2 = room.ambient_light;
	}

	if (p_level_format == TR2_PC) {
		room.light_mode = p_file->get_s16();
	} else {
		room.light_mode = 0;
	}

	room.light_count = p_file->get_u16();
	room.lights.resize(room.light_count);
	for (int i = 0; i < room.light_count; i++) {
		room.lights.set(i, read_tr_light(p_file, p_level_format));
	}

	room.room_static_mesh_count = p_file->get_u16();
	room.room_static_meshes.resize(room.room_static_mesh_count);
	for (int i = 0; i < room.room_static_mesh_count; i++) {
		room.room_static_meshes.set(i, read_tr_room_static_mesh(p_file, p_level_format));
	}

	room.alternative_room = p_file->get_s16();
	room.room_flags = p_file->get_u16();

	// If the room is underwater, change the color
	if (p_level_format == TR1_PC || p_level_format == TR2_PC) {
		if (room.room_flags & 1) {
			for (int i = 0; i < room.data.room_vertex_count; i++) {
				TRRoomVertex room_vertex = room.data.room_vertices.get(i);
				room_vertex.color *= Color(0.0, 0.5, 1.0);
				room.data.room_vertices.set(i, room_vertex);
			}
		}
	}

	if (p_level_format == TR3_PC) {
		room.water_scheme = p_file->get_u8();
		room.reverb_info = p_file->get_u8();

		uint8_t filler = p_file->get_u8();
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
		uint64_t texture_buffer_size = 256 * 256 * sizeof(uint16_t);
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

TRAnimation read_tr_animation(Ref<TRFileAccess> p_file) {
	TRAnimation animation;

	animation.frame_ofs = p_file->get_s32();
	animation.frame_skip = p_file->get_u8();
	animation.frame_size = p_file->get_u8();
	animation.current_anim_state = p_file->get_s16();
	animation.speed = p_file->get_s32();
	animation.acceleration = p_file->get_s32();
	animation.frame_base = p_file->get_s16();
	animation.frame_end = p_file->get_s16();
	animation.jump_anim_num = p_file->get_s16();
	animation.jump_frame_num = p_file->get_s16();
	animation.number_changes = p_file->get_s16();
	animation.change_index = p_file->get_s16();
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

TRMesh read_tr_mesh(Ref<TRFileAccess> p_file) {
	TRMesh tr_mesh;
	tr_mesh.center = read_tr_vertex(p_file);
	tr_mesh.coll_radius = p_file->get_s32();

	tr_mesh.num_verts = p_file->get_s16();
	tr_mesh.verticies.resize(tr_mesh.num_verts);
	for (int i = 0; i < tr_mesh.num_verts; i++) {
		TRVertex vertex = read_tr_vertex(p_file);
		tr_mesh.verticies.set(i, vertex);
	}

	tr_mesh.num_normals = p_file->get_s16();
	int16_t abs_num_normals = abs(tr_mesh.num_normals);

	ERR_FAIL_COND_V(abs_num_normals != tr_mesh.num_verts, tr_mesh);
	
	if (tr_mesh.num_normals > 0) {
		tr_mesh.normals.resize(abs_num_normals);
		for (int i = 0; i < abs_num_normals; i++) {
			TRVertex normal = read_tr_vertex(p_file);
			tr_mesh.normals.set(i, normal);
		}
	} else {
		tr_mesh.colors.resize(abs_num_normals);
		for (int i = 0; i < abs_num_normals; i++) {
			int16_t color = p_file->get_s16();
			tr_mesh.colors.set(i, color);
		}
	}

	// Texture Quad buffer
	tr_mesh.texture_quads_count = p_file->get_s16();
	tr_mesh.texture_quads.resize(tr_mesh.texture_quads_count);
	for (int i = 0; i < tr_mesh.texture_quads_count; i++) {
		tr_mesh.texture_quads.set(i, read_tr_face_quad(p_file));
	}

	// Texture Triangle buffer
	tr_mesh.texture_triangles_count = p_file->get_s16();
	tr_mesh.texture_triangles.resize(tr_mesh.texture_triangles_count);
	for (int i = 0; i < tr_mesh.texture_triangles_count; i++) {
		tr_mesh.texture_triangles.set(i, read_tr_face_triangle(p_file));
	}

	// Color Quad buffer
	tr_mesh.color_quads_count = p_file->get_s16();
	tr_mesh.color_quads.resize(tr_mesh.color_quads_count);
	for (int i = 0; i < tr_mesh.color_quads_count; i++) {
		tr_mesh.color_quads.set(i, read_tr_face_quad(p_file));
	}

	// Color Triangle buffer
	tr_mesh.color_triangles_count = p_file->get_s16();
	tr_mesh.color_triangles.resize(tr_mesh.color_triangles_count);
	for (int i = 0; i < tr_mesh.color_triangles_count; i++) {
		tr_mesh.color_triangles.set(i, read_tr_face_triangle(p_file));
	}

	return tr_mesh;
}

Node3D* create_movable_model(uint32_t p_type_info_id, TRMovableInfo p_movable_info, PackedInt32Array p_mesh_tree_buffer, Vector<Ref<ArrayMesh>> p_meshes, TRLevelFormat p_level_format) {
	Node3D* new_type_info = memnew(Node3D);
	new_type_info->set_name(get_type_info_name(p_type_info_id, p_level_format));

	Vector<int> bone_stack;

	int offset_bone_index = p_movable_info.bone_index;

	int current_parent = -1;
	if (p_movable_info.mesh_count > 0) {
		// TODO: seperate animation handling for single mesh types
		if (p_movable_info.mesh_count > 0) {
			Skeleton3D *skeleton = memnew(Skeleton3D);
			new_type_info->add_child(skeleton);
			skeleton->set_display_folded(true);
			skeleton->set_name("Skeleton");

			AnimationPlayer *animation_player = memnew(AnimationPlayer);
			new_type_info->add_child(animation_player);
			animation_player->set_name("AnimationPlayer");

			Ref<AnimationLibrary> animation_library = memnew(AnimationLibrary);
			Ref<Animation> reset_animation = memnew(Animation);
			reset_animation->set_name("RESET");
			animation_library->add_animation("RESET", reset_animation);

			Vector<Ref<Animation>> godot_animations;

			String first_animation_name = "RESET";
			for (int i = 0; i < p_movable_info.anims.size(); i++) {
				Ref<Animation> godot_animation = memnew(Animation);
				TRAnimation tr_animation = p_movable_info.anims.get(i);

				String animation_name = get_animation_name(p_type_info_id, i);
				if (i == 0) {
					first_animation_name = animation_name;
				}

				godot_animations.push_back(godot_animation);
				godot_animation->set_loop_mode(is_tr_animation_looping(p_type_info_id, i) ? Animation::LOOP_LINEAR : Animation::LOOP_NONE);
				godot_animation->set_length((float)((tr_animation.frames.size()) / TR_FPS) * tr_animation.frame_skip);
				animation_library->add_animation(get_animation_name(p_type_info_id, i), godot_animation);
			}

			animation_player->add_animation_library("", animation_library);
			animation_player->set_current_animation(first_animation_name);

			for (int i = 0; i < p_movable_info.mesh_count; i++) {
				int offset_mesh_index = p_movable_info.mesh_index + i;
				if (offset_mesh_index < p_meshes.size()) {
					Vector3 origin_offset;

					if (i > 0) {
						ERR_FAIL_COND_V(p_mesh_tree_buffer.size() < (offset_bone_index + 4), nullptr);

						int flags = p_mesh_tree_buffer.get(offset_bone_index++);
						int x = p_mesh_tree_buffer.get(offset_bone_index++);
						int y = p_mesh_tree_buffer.get(offset_bone_index++);
						int z = p_mesh_tree_buffer.get(offset_bone_index++);

						origin_offset = Vector3(
							x * TR_TO_GODOT_SCALE,
							y * -TR_TO_GODOT_SCALE,
							z * -TR_TO_GODOT_SCALE);

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
					}

					String bone_name = get_bone_name(p_type_info_id, i, p_level_format);

					skeleton->add_bone(bone_name);
					skeleton->set_bone_parent(i, current_parent);
					skeleton->set_bone_rest(i, Transform3D(Basis(), origin_offset));
					skeleton->set_bone_pose_position(i, origin_offset);

					reset_animation->add_track(Animation::TYPE_POSITION_3D);
					reset_animation->track_set_path(reset_animation->get_track_count() - 1, NodePath("Skeleton:" + bone_name));
					reset_animation->track_insert_key(reset_animation->get_track_count() - 1, 0.0, origin_offset);
					reset_animation->track_insert_key(reset_animation->get_track_count() - 1, 1.0, origin_offset);
					reset_animation->add_track(Animation::TYPE_ROTATION_3D);
					reset_animation->track_set_path(reset_animation->get_track_count() - 1, NodePath("Skeleton:" + bone_name));
					reset_animation->track_insert_key(reset_animation->get_track_count() - 1, 0.0, Quaternion());
					reset_animation->track_insert_key(reset_animation->get_track_count() - 1, 1.0, Quaternion());

					for (int anim_idx = 0; anim_idx < p_movable_info.anims.size(); anim_idx++) {
						TRAnimation tr_animation = p_movable_info.anims.get(anim_idx);
						Ref<Animation> godot_animation = godot_animations[anim_idx];

						godot_animation->add_track(Animation::TYPE_POSITION_3D);
						int32_t position_track_idx = godot_animation->get_track_count() - 1;
						godot_animation->track_set_path(position_track_idx, NodePath("Skeleton:" + bone_name));

						godot_animation->add_track(Animation::TYPE_ROTATION_3D);
						int32_t rotation_track_idx = godot_animation->get_track_count() - 1;
						godot_animation->track_set_path(rotation_track_idx, NodePath("Skeleton:" + bone_name));

						for (int frame_idx = 0; frame_idx < tr_animation.frames.size(); frame_idx++) {
							TRAnimFrame anim_frame = tr_animation.frames[frame_idx];
							TRTransform bone_transform = anim_frame.transforms.get(i);

							Vector3 frame_origin = Vector3(
								bone_transform.pos.x * TR_TO_GODOT_SCALE,
								bone_transform.pos.y * -TR_TO_GODOT_SCALE,
								bone_transform.pos.z * -TR_TO_GODOT_SCALE);

							float rot_y_deg = (float)(bone_transform.rot.y) / 16384.0f * -90.0f;
							float rot_x_deg = (float)(bone_transform.rot.x) / 16384.0f * 90.0f;
							float rot_z_deg = (float)(bone_transform.rot.z) / 16384.0f * -90.0f;

							float rot_y_rad = Math::deg_to_rad(rot_y_deg);
							float rot_x_rad = Math::deg_to_rad(rot_x_deg);
							float rot_z_rad = Math::deg_to_rad(rot_z_deg);

							Basis rotation_basis;
							rotation_basis.rotate(Vector3(rot_x_rad, rot_y_rad, rot_z_rad), EulerOrder::YXZ);

							godot_animation->track_insert_key(position_track_idx, (float)(frame_idx / TR_FPS) * tr_animation.frame_skip, origin_offset + frame_origin);
							godot_animation->track_insert_key(rotation_track_idx, (float)(frame_idx / TR_FPS) * tr_animation.frame_skip, rotation_basis.get_quaternion());
						}
					}

					Ref<ArrayMesh> mesh = p_meshes.get(offset_mesh_index);
					MeshInstance3D* mi = memnew(MeshInstance3D);

					BoneAttachment3D* bone_attachment = memnew(BoneAttachment3D);
					bone_attachment->set_name(get_bone_name(p_type_info_id, i, p_level_format) + "_attachment");
					skeleton->add_child(bone_attachment);
					bone_attachment->set_bone_idx(i);
					bone_attachment->add_child(mi);

					mi->set_mesh(mesh);
					mi->set_name(String("MeshInstance_") + itos(offset_mesh_index));

					current_parent = i;
				}
			}
		} else {
			int offset_mesh_index = p_movable_info.mesh_index;
			if (offset_mesh_index < p_meshes.size()) {
				Ref<ArrayMesh> mesh = p_meshes.get(offset_mesh_index);
				MeshInstance3D* mi = memnew(MeshInstance3D);

				new_type_info->add_child(mi);

				mi->set_mesh(mesh);
				mi->set_name(String("MeshInstance_") + itos(offset_mesh_index));
			}
		}
	}

	return new_type_info;
}

const int TR_FRAME_BOUND_MIN_X = 0;
const int TR_FRAME_BOUND_MAX_X = 1;
const int TR_FRAME_BOUND_MIN_Y = 2;
const int TR_FRAME_BOUND_MAX_Y = 3;
const int TR_FRAME_BOUND_MIN_Z = 4;
const int TR_FRAME_BOUND_MAX_Z = 5;
const int TR_FRAME_POS_X = 6;
const int TR_FRAME_POS_Y = 7;
const int TR_FRAME_POS_Z = 8;
const int TR_FRAME_ROT = 10;

Vector<Node3D *> create_nodes_for_movables(HashMap<int, TRMovableInfo> p_type_info_map, PackedInt32Array p_mesh_tree_buffer, Vector<Ref<ArrayMesh>> p_meshes, TRLevelFormat p_level_format) {
	Vector<Node3D *> types;

	for (int type_id = 0; type_id < 4096; type_id++) {
		if (p_type_info_map.has(type_id)) {
			Node3D* new_node = create_movable_model(type_id, p_type_info_map[type_id], p_mesh_tree_buffer, p_meshes, p_level_format);
			if (new_node) {
				types.push_back(new_node);
			}
		}
	}

	return types;
}

Vector<TRTextureInfo> read_tr_texture_infos(Ref<TRFileAccess> p_file) {
	int32_t texture_count = p_file->get_s32();
	Vector<TRTextureInfo> texture_infos;
	ERR_FAIL_COND_V(texture_count > 8192, Vector<TRTextureInfo>());

	for (int i = 0; i < texture_count; i++) {
		TRTextureInfo texture_info;

		texture_info.drawtype = p_file->get_s16();
		texture_info.texture_page = p_file->get_s16();
		for (int j = 0; j < 4; j++) {
			texture_info.uv[j].u = p_file->get_s16();
			texture_info.uv[j].v = p_file->get_s16();
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
	int mesh_buffer_pos = p_file->get_position();
	// Skip the buffer
	p_file->seek(p_file->get_position() + mesh_data_buffer_size * sizeof(uint16_t));

	// Mesh Pointers
	int32_t mesh_ptr_count = p_file->get_s32();
	PackedInt32Array mesh_ptr_buffer = p_file->get_buffer_int32(mesh_ptr_count * sizeof(uint32_t));

	// Save position of the end of the mesh pointer buffer
	int mesh_ptr_buffer_end = p_file->get_position();

	for (int i = 0; i < mesh_ptr_count; i++) {
		int32_t ptr = mesh_ptr_buffer[i];
		p_file->seek(mesh_buffer_pos + ptr);

		TRMesh mesh = read_tr_mesh(p_file);
		tr_types.meshes.push_back(mesh);
	}

	p_file->seek(mesh_ptr_buffer_end);

	// Anims
	int32_t anim_count = p_file->get_s32();
	for (int i = 0; i < anim_count; i++) {
		TRAnimation animation = read_tr_animation(p_file);
		tr_types.animations.push_back(animation);
	}

	// Anim Change
	int32_t anim_change_count = p_file->get_s32();
	p_file->seek(p_file->get_position() + (anim_change_count * sizeof(TRAnimationChange)));

	// Anim Range
	int32_t anim_range_count = p_file->get_s32();
	p_file->seek(p_file->get_position() + (anim_range_count * sizeof(TRAnimationRange)));

	// Anim Command
	int32_t anim_command_count = p_file->get_s32();
	p_file->seek(p_file->get_position() + (anim_command_count * sizeof(TRAnimationCommand)));

	// Mesh Tree Count
	int32_t mesh_tree_count = p_file->get_s32();
	tr_types.mesh_tree_buffer = p_file->get_buffer_int32(mesh_tree_count * sizeof(uint32_t));

	// Anim Frame Count
	int32_t anim_frame_count = p_file->get_s32();
	PackedByteArray anim_frame_buffer = p_file->get_buffer(anim_frame_count * sizeof(uint16_t));

	// Movable Info Count
	Vector<int32_t> id_list;
	Vector<TRMovableInfo> movable_infos;
	int32_t movable_info_count = p_file->get_s32();

	for (int i = 0; i < movable_info_count; i++) {
		int32_t type_info_id = p_file->get_u32();

		TRMovableInfo movable_info;
		movable_info.mesh_count = p_file->get_s16();
		movable_info.mesh_index = p_file->get_s16();
		movable_info.bone_index = p_file->get_s32();

		uint32_t tmp_anim_frames = p_file->get_s32();
		// TODO: set frame base
		movable_info.anim_index = p_file->get_s16();

		if (!id_list.has(type_info_id)) {
			id_list.push_back(type_info_id);
			movable_infos.push_back(movable_info);
		}
	}

	// Calculate animation count (should work fine in retail games, but may not work for custom levels)
	for (int i = 0; i < movable_infos.size() - 1; i++) {
		TRMovableInfo movable_info = movable_infos[i];

		if (movable_infos[i].anim_index >= 0) {
			movable_info.anim_count = movable_infos[i + 1].anim_index - movable_infos[i].anim_index;
		}
		movable_infos.set(i, movable_info);
	}

	// Animation setup (we're skipping the last model because we don't know the amount of animations it has)
	for (int i = 0; i < movable_infos.size() - 1; i++) {
		TRMovableInfo movable_info = movable_infos[i];

		for (int animation_idx = 0; animation_idx < movable_info.anim_count; animation_idx++) {
			TRAnimation animation = tr_types.animations[movable_info.anim_index + animation_idx];
			ERR_FAIL_COND_V(animation.frame_skip <= 0, TRTypes());

			int frame_count = (animation.frame_end - animation.frame_base) / animation.frame_skip;
			frame_count++;
			
			int frame_ptr = animation.frame_ofs;

			for (int frame_idx = 0; frame_idx < frame_count; frame_idx++) {				
				// This may be wrong

				if (animation.frame_size > 0) {
					frame_ptr = animation.frame_ofs + ((animation.frame_size * sizeof(uint16_t) * frame_idx));
				}

				TRBoundingBox bounding_box;
				
				int16_t* bounds[] = { &bounding_box.x_min, &bounding_box.x_max, &bounding_box.y_min, &bounding_box.y_max, &bounding_box.z_min, &bounding_box.z_max };
				for (int j = 0; j < sizeof(bounds) / sizeof(bounds[0]); j++) {
					uint8_t first = anim_frame_buffer[frame_ptr++];
					uint8_t second = anim_frame_buffer[frame_ptr++];

					*bounds[j] = (first | ((int16_t)second << 8));
				}

				TRPos pos;

				int32_t* coords[] = { &pos.x, &pos.y, &pos.z };
				for (int j = 0; j < sizeof(coords) / sizeof(coords[0]); j++) {
					uint8_t first = anim_frame_buffer[frame_ptr++];
					uint8_t second = anim_frame_buffer[frame_ptr++];

					*coords[j] = (int16_t)(first | ((int16_t)second << 8));
				}

				int16_t num_rotations = movable_info.mesh_count;
				if (p_level_format == TR1_PC) {
					uint8_t first = anim_frame_buffer[frame_ptr++];
					uint8_t second = anim_frame_buffer[frame_ptr++];

					num_rotations = (first) | ((int16_t)(second) << 8);
				}

				TRAnimFrame anim_frame;

				for (int j = 0; j < num_rotations; j++) {
					TRTransform bone_transform;

					uint8_t rot_1 = anim_frame_buffer[frame_ptr++];
					uint8_t rot_2 = anim_frame_buffer[frame_ptr++];
					uint16_t rot_16_a = (rot_1) | ((uint16_t)(rot_2) << 8);

					uint16_t flags = (rot_16_a & 0xc000) >> 14;

					if (p_level_format == TR1_PC || flags == 0) {
						uint8_t rot_3 = anim_frame_buffer[frame_ptr++];
						uint8_t rot_4 = anim_frame_buffer[frame_ptr++];

						uint16_t rot_16_b = (rot_3) | ((uint16_t)(rot_4) << 8);

						uint32_t rot_32;
						if (p_level_format == TR2_PC || p_level_format == TR3_PC) {
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

						// TODO: Different mask required to TR4!
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

						bone_transform.rot = rot;
					}

					if (j == 0) {
						bone_transform.pos = pos;
					}
					anim_frame.transforms.push_back(bone_transform);
				}

				animation.frames.push_back(anim_frame);
			}
			movable_info.anims.push_back(animation);
		}
		movable_infos.set(i, movable_info);
	}

	for (int i = 0; i < id_list.size(); i++) {
		tr_types.movable_info_map[id_list[i]] = movable_infos[i];
	}

	// TODO: Setup types

	// Statics Count
	HashMap<int, TRStaticInfo > static_map;
	int32_t static_count = p_file->get_s32();
	for (int i = 0; i < static_count; i++) {
		int32_t static_id = p_file->get_u32();
		
		TRStaticInfo static_info;

		static_info.mesh_number = p_file->get_u16();

		static_info.visibility_box = read_tr_bounding_box(p_file);
		static_info.collision_box = read_tr_bounding_box(p_file);

		static_info.flags = p_file->get_u16();

		tr_types.static_info_map[static_id] = static_info;
	}

	if (p_level_format != TR3_PC) {
		tr_types.texture_infos = read_tr_texture_infos(p_file);
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

	for (int i = 0; i < sprite_info_count; i++) {
		TRSpriteInfo sprite_info = read_tr_sprite_info(p_file);
	}

	int32_t sprite_count = p_file->get_s32();
	for (int i = 0; i < sprite_count; i++) {
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

	for (int i = 0; i < camera_info.fixed.size(); i++) {
		camera_info.fixed.set(i, read_tr_game_vector(p_file));
	}
}

Vector<TRObjectVector> read_tr_sound_effects(Ref<TRFileAccess> p_file) {
	int32_t num_sound_effects = p_file->get_s32();
	Vector<TRObjectVector> sound_effect_table;

	sound_effect_table.resize(num_sound_effects);

	for (int i = 0; i < sound_effect_table.size(); i++) {
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
	for (int i = 0; i < num_nav_cells; i++) {
		nav_cells.set(i, read_tr_nav_cell(p_file, p_level_format));
	}

	int32_t num_overlaps = p_file->get_s32();
	PackedByteArray overlaps = p_file->get_buffer(num_overlaps * sizeof(int16_t));

	for (int i = 0; i < 2; i++) {
		PackedByteArray ground_zone = p_file->get_buffer(num_nav_cells * sizeof(int16_t));
		PackedByteArray ground_zone2 = p_file->get_buffer(num_nav_cells * sizeof(int16_t));
		if (p_level_format != TR1_PC) {
			PackedByteArray ground_zone3 = p_file->get_buffer(num_nav_cells * sizeof(int16_t));
			PackedByteArray ground_zone4 = p_file->get_buffer(num_nav_cells * sizeof(int16_t));
		}
		PackedByteArray fly_zone = p_file->get_buffer(num_nav_cells * sizeof(int16_t));
	}
}

void read_tr_animated_textures(Ref<TRFileAccess> p_file) {
	int32_t animated_texture_range_count = p_file->get_s32();

	PackedByteArray animated_texture_ranges = p_file->get_buffer(animated_texture_range_count * sizeof(int16_t));
}

Vector<TREntity> read_tr_entities(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	Vector<TREntity> entities;

	int32_t entity_count = p_file->get_s32();

	if (entity_count) {
		ERR_FAIL_COND_V(entity_count > 10240, Vector<TREntity>());
		
		for (int i = 0; i < entity_count; i++) {
			TREntity entity;

			entity.type_id = p_file->get_s16();
			entity.room_number = p_file->get_s16();
			entity.transform.pos = read_tr_pos(p_file);
			entity.transform.rot.y = p_file->get_s16();
			entity.shade = p_file->get_s16();
			if (p_level_format == TR2_PC || p_level_format == TR3_PC) {
				entity.shade2 = p_file->get_s16();
			} else {
				entity.shade2 = entity.shade;
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
	int pos = p_file->get_position();
	p_file->seek(p_file->get_position() + (sizeof(uint8_t) * 32 * 256));
}

TRCameraFrame read_tr_camera_frame(Ref<TRFileAccess> p_file) {
	TRCameraFrame camera_frame;
	camera_frame.target = read_tr_pos(p_file);
	camera_frame.pos = read_tr_pos(p_file);
	camera_frame.fov = p_file->get_s16();
	camera_frame.roll = p_file->get_s16();

	return camera_frame;
}

Vector<TRCameraFrame> read_tr_camera_frames(Ref<TRFileAccess> p_file) {
	int16_t camera_frame_count = p_file->get_s16();

	Vector<TRCameraFrame> camera_frames;
	for (int i = 0; i < camera_frame_count; i++) {
		camera_frames.push_back(read_tr_camera_frame(p_file));
	}

	return camera_frames;
}

void read_tr_demo_frames(Ref<TRFileAccess> p_file) {
	int16_t demo_frame_count = p_file->get_s16();

	p_file->seek(p_file->get_position() + (demo_frame_count * sizeof(uint32_t)));

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

	return p_file->get_buffer_int32(indices_count);
}

Vector<TRColor3> read_tr_palette(Ref<TRFileAccess> p_file) {
	Vector<TRColor3> palette;
	palette.resize(256);

	int pos = p_file->get_position();

	for (int i = 0; i < 256; i++) {
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
	palette.resize(256);

	int pos = p_file->get_position();

	for (int i = 0; i < 256; i++) {
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
	bool is_portal = false;
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
				geo_shift.is_portal = true;
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

static float u_fixed_16_to_float(uint16_t p_fixed, bool no_fraction) {
	if (no_fraction) {
		return (float)((p_fixed & 0xff00) >> 8) + (float)((p_fixed & 0x00ff) / 255.0f);
	} else {
		return (float)((p_fixed & 0xff00) >> 8);
	}
}

#define INSERT_COLORED_VERTEX(p_current_idx, p_uv, p_vertex_id_list, p_color_index_map, p_verts_array) \
{ \
	ERR_FAIL_COND_V(p_current_idx < 0 || p_current_idx > p_verts_array.size(), Ref<ArrayMesh>()); \
	VertexAndUV vert_and_uv;\
	vert_and_uv.vertex_idx = p_current_idx;\
	vert_and_uv.uv = p_uv;\
	int result = -1;\
	int vertex_id_map_size = p_vertex_id_list.size();\
	for (int idx = 0; idx < vertex_id_map_size; idx++) {\
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
	int result = -1;\
	int vertex_uv_map_size = p_vertex_uv_map.get(p_texture_page).size();\
	for (int idx = 0; idx < vertex_uv_map_size; idx++) {\
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

	for (int i = 0; i < p_room_data.room_vertex_count; i++) {
		room_verts.push_back(p_room_data.room_vertices[i]);
	}

	struct VertexAndUV {
		int vertex_idx;
		TRUV uv;
	};

	int last_material_id = 0;

	HashMap<int, Vector<VertexAndUV>> vertex_uv_map;
	HashMap<int, Vector<int> > material_index_map;

	//
	for (int i = 0; i < p_room_data.room_quad_count; i++) {
		if (p_room_data.room_quads[i].tex_info_id < 0) {
			continue;
		}

		TRTextureInfo texture_info = p_types.texture_infos.get(p_room_data.room_quads[i].tex_info_id);
		int material_id = texture_info.texture_page;
		if (texture_info.drawtype == 1) {
			material_id += p_solid_level_materials.size();
		}
		last_material_id = material_id > last_material_id ? material_id : last_material_id;

		if (!vertex_uv_map.has(material_id)) {
			vertex_uv_map.insert(material_id, Vector<VertexAndUV>());
		}

		if (!material_index_map.has(material_id)) {
			material_index_map.insert(material_id, Vector<int>());
		}

		INSERT_TEXTURED_VERTEX(p_room_data.room_quads[i].indices[0], texture_info.uv[0], material_id, vertex_uv_map, material_index_map, room_verts);
		INSERT_TEXTURED_VERTEX(p_room_data.room_quads[i].indices[1], texture_info.uv[1], material_id, vertex_uv_map, material_index_map, room_verts);
		INSERT_TEXTURED_VERTEX(p_room_data.room_quads[i].indices[2], texture_info.uv[2], material_id, vertex_uv_map, material_index_map, room_verts);

		INSERT_TEXTURED_VERTEX(p_room_data.room_quads[i].indices[2], texture_info.uv[2], material_id, vertex_uv_map, material_index_map, room_verts);
		INSERT_TEXTURED_VERTEX(p_room_data.room_quads[i].indices[3], texture_info.uv[3], material_id, vertex_uv_map, material_index_map, room_verts);
		INSERT_TEXTURED_VERTEX(p_room_data.room_quads[i].indices[0], texture_info.uv[0], material_id, vertex_uv_map, material_index_map, room_verts);
	}

	for (int i = 0; i < p_room_data.room_triangle_count; i++) {
		if (p_room_data.room_triangles[i].tex_info_id < 0) {
			continue;
		}

		TRTextureInfo texture_info = p_types.texture_infos.get(p_room_data.room_triangles[i].tex_info_id);
		int material_id = texture_info.texture_page;
		if (texture_info.drawtype == 1) {
			material_id += p_solid_level_materials.size();
		}

		last_material_id = material_id > last_material_id ? material_id : last_material_id;

		if (!vertex_uv_map.has(material_id)) {
			vertex_uv_map.insert(material_id, Vector<VertexAndUV>());
		}

		if (!material_index_map.has(material_id)) {
			material_index_map.insert(material_id, Vector<int>());
		}

		INSERT_TEXTURED_VERTEX(p_room_data.room_triangles[i].indices[0], texture_info.uv[0], material_id, vertex_uv_map, material_index_map, room_verts);
		INSERT_TEXTURED_VERTEX(p_room_data.room_triangles[i].indices[1], texture_info.uv[1], material_id, vertex_uv_map, material_index_map, room_verts);
		INSERT_TEXTURED_VERTEX(p_room_data.room_triangles[i].indices[2], texture_info.uv[2], material_id, vertex_uv_map, material_index_map, room_verts);
	}

	Vector<Ref<Material>> all_materials = p_solid_level_materials;
	all_materials.append_array(p_level_trans_materials);

	for (int current_tex_page = 0; current_tex_page < last_material_id + 1; current_tex_page++) {

		st->begin(Mesh::PRIMITIVE_TRIANGLES);

		if (!vertex_uv_map.has(current_tex_page) || !material_index_map.has(current_tex_page)) {
			continue;
		}

		for (int i = 0; i < vertex_uv_map.get(current_tex_page).size(); i++) {
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

		for (int i = 0; i < material_index_map.get(current_tex_page).size(); i++) {
			st->add_index(material_index_map.get(current_tex_page).get(i));
		}

		st->set_material(all_materials.get(current_tex_page));
		ar_mesh = st->commit(ar_mesh);
	}

	return ar_mesh;
}

Ref<ArrayMesh> tr_mesh_to_godot_mesh(const TRMesh& p_mesh_data, const Vector<Ref<Material>> p_solid_level_materials, const Vector<Ref<Material>> p_level_trans_materials, const Ref<Material> p_level_palette_material, TRTypes& p_types) {
	Ref<SurfaceTool> st = memnew(SurfaceTool);
	Ref<ArrayMesh> ar_mesh = memnew(ArrayMesh);

	Vector<TRVertex> mesh_verts;

	for (int i = 0; i < p_mesh_data.verticies.size(); i++) {
		mesh_verts.push_back(p_mesh_data.verticies[i]);
	}

	struct VertexAndUV {
		int32_t vertex_idx;
		TRUV uv;
	};

	struct VertexAndID {
		int32_t vertex_idx;
		uint8_t id;
	};

	int last_material_id = 0;

	// Colors
	Vector<VertexAndUV> vertex_color_uv_map;
	Vector<int> color_index_map;
	for (int i = 0; i < p_mesh_data.color_quads_count; i++) {
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

	for (int i = 0; i < p_mesh_data.color_triangles_count; i++) {
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
	HashMap<int, Vector<VertexAndUV>> vertex_uv_map;
	HashMap<int, Vector<int> > material_index_map;
	for (int i = 0; i < p_mesh_data.texture_quads_count; i++) {
		TRTextureInfo texture_info = p_types.texture_infos.get(p_mesh_data.texture_quads[i].tex_info_id);
		int material_id = texture_info.texture_page;
		if (texture_info.drawtype == 1) {
			material_id += p_solid_level_materials.size();
		}
		last_material_id = material_id > last_material_id ? material_id : last_material_id;

		if (!vertex_uv_map.has(material_id)) {
			vertex_uv_map.insert(material_id, Vector<VertexAndUV>());
		}

		if (!material_index_map.has(material_id)) {
			material_index_map.insert(material_id, Vector<int>());
		}

		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_quads[i].indices[0], texture_info.uv[0], material_id, vertex_uv_map, material_index_map, mesh_verts);
		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_quads[i].indices[1], texture_info.uv[1], material_id, vertex_uv_map, material_index_map, mesh_verts);
		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_quads[i].indices[2], texture_info.uv[2], material_id, vertex_uv_map, material_index_map, mesh_verts);

		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_quads[i].indices[2], texture_info.uv[2], material_id, vertex_uv_map, material_index_map, mesh_verts);
		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_quads[i].indices[3], texture_info.uv[3], material_id, vertex_uv_map, material_index_map, mesh_verts);
		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_quads[i].indices[0], texture_info.uv[0], material_id, vertex_uv_map, material_index_map, mesh_verts);
	}

	for (int i = 0; i < p_mesh_data.texture_triangles_count; i++) {
		TRTextureInfo texture_info = p_types.texture_infos.get(p_mesh_data.texture_triangles[i].tex_info_id);
		int material_id = texture_info.texture_page;
		if (texture_info.drawtype == 1) {
			material_id += p_solid_level_materials.size();
		}

		last_material_id = material_id > last_material_id ? material_id : last_material_id;

		if (!vertex_uv_map.has(material_id)) {
			vertex_uv_map.insert(material_id, Vector<VertexAndUV>());
		}

		if (!material_index_map.has(material_id)) {
			material_index_map.insert(material_id, Vector<int>());
		}

		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_triangles[i].indices[0], texture_info.uv[0], material_id, vertex_uv_map, material_index_map, mesh_verts);
		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_triangles[i].indices[1], texture_info.uv[1], material_id, vertex_uv_map, material_index_map, mesh_verts);
		INSERT_TEXTURED_VERTEX(p_mesh_data.texture_triangles[i].indices[2], texture_info.uv[2], material_id, vertex_uv_map, material_index_map, mesh_verts);
	}

	if (vertex_color_uv_map.size() > 0) {
		st->begin(Mesh::PRIMITIVE_TRIANGLES);

		for (int i = 0; i < vertex_color_uv_map.size(); i++) {
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

		for (int i = 0; i < color_index_map.size(); i++) {
			st->add_index(color_index_map.get(i));
		}

		st->set_material(p_level_palette_material);
		ar_mesh = st->commit(ar_mesh);
	}

	Vector<Ref<Material>> all_materials = p_solid_level_materials;
	all_materials.append_array(p_level_trans_materials);

	for (int current_tex_page = 0; current_tex_page < last_material_id + 1; current_tex_page++) {
		st->begin(Mesh::PRIMITIVE_TRIANGLES);

		if (!vertex_uv_map.has(current_tex_page) || !material_index_map.has(current_tex_page)) {
			continue;
		}

		for (int i = 0; i < vertex_uv_map.get(current_tex_page).size(); i++) {
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

		for (int i = 0; i < material_index_map.get(current_tex_page).size(); i++) {
			st->add_index(material_index_map.get(current_tex_page).get(i));
		}

		st->set_material(all_materials.get(current_tex_page));
		ar_mesh = st->commit(ar_mesh);
	}

	return ar_mesh;
}


CollisionShape3D *tr_room_to_collision_shape(const TRRoom& p_room, PackedByteArray p_floor_data) {
	Ref<ConcavePolygonShape3D> collision_data = memnew(ConcavePolygonShape3D);

	const float SQUARE_SIZE = 1024 * TR_TO_GODOT_SCALE;
	const float CLICK_SIZE = SQUARE_SIZE / 4.0;

	PackedVector3Array buf;

	int y_bottom = p_room.info.y_bottom;
	int y_top = p_room.info.y_top;

	int current_sector = 0;
	for (int z = 0; z < p_room.sector_count_z; z++) {
		for (int x = 0; x < p_room.sector_count_x; x++) {
			uint8_t room_below = p_room.sectors[current_sector].room_below;
			uint8_t room_above = p_room.sectors[current_sector].room_above;

			int8_t floor_height = p_room.sectors[current_sector].floor;
			int8_t ceiling_height = p_room.sectors[current_sector].ceiling;
			uint16_t floor_data_index = p_room.sectors[current_sector].floor_data_index;

			GeometryShift geo_shift = parse_floor_data_entry(p_floor_data, floor_data_index);

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

			// floor
			if ((ne_ceiling_height != ne_floor_height
				|| se_ceiling_height != se_floor_height
				|| sw_ceiling_height != sw_floor_height
				|| nw_ceiling_height != nw_floor_height) && floor_height != -127 && room_below == 0xff && !geo_shift.is_portal) {

				float ne_offset_f = (float)(ne_floor_height) * CLICK_SIZE;
				float se_offset_f = (float)(se_floor_height) * CLICK_SIZE;
				float sw_offset_f = (float)(sw_floor_height) * CLICK_SIZE;
				float nw_offset_f = (float)(nw_floor_height) * CLICK_SIZE;

				buf.append(Vector3(SQUARE_SIZE * z,					nw_offset_f, -(SQUARE_SIZE * x)));
				buf.append(Vector3(SQUARE_SIZE * z + SQUARE_SIZE,	ne_offset_f, -(SQUARE_SIZE * x)));
				buf.append(Vector3(SQUARE_SIZE * z,					sw_offset_f, -(SQUARE_SIZE * x + SQUARE_SIZE)));

				buf.append(Vector3(SQUARE_SIZE * z,					sw_offset_f, -(SQUARE_SIZE * x + SQUARE_SIZE)));
				buf.append(Vector3(SQUARE_SIZE * z + SQUARE_SIZE,	se_offset_f, -(SQUARE_SIZE * x + SQUARE_SIZE)));
				buf.append(Vector3(SQUARE_SIZE * z + SQUARE_SIZE,	ne_offset_f, -(SQUARE_SIZE * x)));
			}

			// ceiling
			if ((ne_ceiling_height != ne_floor_height
				|| se_ceiling_height != se_floor_height
				|| sw_ceiling_height != sw_floor_height
				|| nw_ceiling_height != nw_floor_height) && ceiling_height != -127 && room_above == 0xff && !geo_shift.is_portal) {

				float ne_offset_f = (float)(ne_ceiling_height) * CLICK_SIZE;
				float se_offset_f = (float)(se_ceiling_height) * CLICK_SIZE;
				float sw_offset_f = (float)(sw_ceiling_height) * CLICK_SIZE;
				float nw_offset_f = (float)(nw_ceiling_height) * CLICK_SIZE;

				buf.append(Vector3(SQUARE_SIZE * z,					nw_offset_f, -(SQUARE_SIZE * x)));
				buf.append(Vector3(SQUARE_SIZE * z + SQUARE_SIZE,	ne_offset_f, -(SQUARE_SIZE * x)));
				buf.append(Vector3(SQUARE_SIZE * z,					sw_offset_f, -(SQUARE_SIZE * x + SQUARE_SIZE)));

				buf.append(Vector3(SQUARE_SIZE * z,					sw_offset_f, -(SQUARE_SIZE * x + SQUARE_SIZE)));
				buf.append(Vector3(SQUARE_SIZE * z + SQUARE_SIZE,	se_offset_f, -(SQUARE_SIZE * x + SQUARE_SIZE)));
				buf.append(Vector3(SQUARE_SIZE * z + SQUARE_SIZE,	ne_offset_f, -(SQUARE_SIZE * x)));
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


Ref<Material> generate_tr_material(Ref<ImageTexture> p_image_texture, bool p_is_transparent) {
	Ref<StandardMaterial3D> new_material = memnew(StandardMaterial3D);

	new_material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	new_material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	new_material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, p_image_texture);
	new_material->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_NEAREST);
	if (p_is_transparent) {
		new_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
	}

	return new_material;
}

void set_owner_recursively(Node *p_node, Node *p_owner) {
	p_node->set_owner(p_owner);
	for (int i = 0; i < p_node->get_child_count(); i++) {
		set_owner_recursively(p_node->get_child(i), p_owner);
	}
}

Node3D *generate_godot_scene(Node *p_root, Vector<PackedByteArray> p_textures, Vector<TRColor3> p_palette, Vector<TRRoom> p_rooms, Vector<TREntity> p_entities, TRTypes & p_types, PackedByteArray p_floor_data, TRLevelFormat p_level_format) {
	Vector<Ref<Material>> level_materials;
	Vector<Ref<Material>> level_trans_materials;
	Ref<Material> palette_material;
	
	// Palette Texture
	{
		Ref<Image> palette_image = memnew(Image(TR_TEXTILE_SIZE, TR_TEXTILE_SIZE, false, Image::FORMAT_RGBA8));
		for (int x = 0; x < TR_TEXTILE_SIZE; x++) {
			for (int y = 0; y < TR_TEXTILE_SIZE; y++) {
				uint32_t index = ((y / 16) * 16) + (x / 16);

				TRColor3 color = p_palette.get(index);
				palette_image->set_pixel(x, y, Color(((float)color.r / 255.0f), ((float)color.g / 255.0f), ((float)color.b / 255.0f), 1.0f));
			}
		}
		Ref<ImageTexture> it = ImageTexture::create_from_image(palette_image);
		palette_material = generate_tr_material(it, false);

		String palette_path = String("Pal") + String(".png");
		palette_image->save_png(palette_path);
	}

	// Image Textures
	for (int i = 0; i < p_textures.size(); i++) {
		PackedByteArray current_texture = p_textures.get(i);
		Ref<Image> image = memnew(Image(TR_TEXTILE_SIZE, TR_TEXTILE_SIZE, false, Image::FORMAT_RGBA8));
		for (int x = 0; x < TR_TEXTILE_SIZE; x++) {
			for (int y = 0; y < TR_TEXTILE_SIZE; y++) {
				uint8_t index = current_texture.get((y * TR_TEXTILE_SIZE) + x);

				if (index == 0x00) {
					TRColor3 color = p_palette.get(index);
					image->set_pixel(x, y, Color(0.0f, 0.0f, 0.0f, 0.0f));
				} else {
					TRColor3 color = p_palette.get(index);
					image->set_pixel(x, y, Color(((float)color.r / 255.0f), ((float)color.g / 255.0f), ((float)color.b / 255.0f), 1.0f));
				}
			}
		}

#if 1
		String image_path = String("Texture_") + itos(i) + String(".png");
		image->save_png(image_path);
#endif

		Ref<ImageTexture> it = ImageTexture::create_from_image(image);
		level_materials.append(generate_tr_material(it, false));
		level_trans_materials.append(generate_tr_material(it, true));
	}

	Node3D* rooms_node = memnew(Node3D);
	p_root->add_child(rooms_node);
	rooms_node->set_name("TRRooms");
	rooms_node->set_owner(p_root->get_owner());
	

	Vector<Ref<ArrayMesh>> meshes;
	for (TRMesh &tr_mesh : p_types.meshes) {
		Ref<ArrayMesh> mesh = tr_mesh_to_godot_mesh(tr_mesh, level_materials, level_trans_materials, palette_material, p_types);
		meshes.push_back(mesh);
#if 0
		MeshInstance3D* mi = memnew(MeshInstance3D);
		rooms_node->add_child(mi);
		mi->set_mesh(mesh);
		mi->set_name(String("MeshInstance_") + itos(i));
		mi->set_owner(p_root->get_owner());
#endif
	}

#if 1
	Vector<Node3D*> movable_node = create_nodes_for_movables(p_types.movable_info_map, p_types.mesh_tree_buffer, meshes, p_level_format);

	for (Node3D *movable : movable_node) {
		rooms_node->add_child(movable);
		set_owner_recursively(movable, p_root->get_owner());
		movable->set_display_folded(true);
	}
#endif

	Node3D* entities_node = memnew(Node3D);
	p_root->add_child(entities_node);
	entities_node->set_name("TREntities");
	entities_node->set_owner(p_root->get_owner());

	uint32_t entity_idx = 0;
	for (const TREntity& entity : p_entities) {
		Node3D * entity_node = memnew(Node3D);
		entity_node->set_transform(Transform3D(Basis().rotated(
			Vector3(0.0, 1.0, 0.0), Math::deg_to_rad((float)entity.transform.rot.y / 16384.0f * -90)), Vector3(
				entity.transform.pos.x * TR_TO_GODOT_SCALE,
				entity.transform.pos.y * -TR_TO_GODOT_SCALE,
				entity.transform.pos.z * -TR_TO_GODOT_SCALE)));

		entities_node->add_child(entity_node);
		entity_node->set_name("Entity_" + itos(entity_idx) + " (" + get_type_info_name(entity.type_id, p_level_format) + ")");
		entity_node->set_owner(p_root->get_owner());
		entity_node->set_display_folded(true);

		if (p_types.movable_info_map.has(entity.type_id)) {
			Node3D* new_node = create_movable_model(entity.type_id, p_types.movable_info_map[entity.type_id], p_types.mesh_tree_buffer, meshes, p_level_format);
			Node3D* type = new_node;
			entity_node->add_child(type);
			set_owner_recursively(type, p_root->get_owner());
			type->set_display_folded(true);
		}
		entity_idx++;
	}

	uint32_t room_idx = 0;
	for (const TRRoom &room : p_rooms) {
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

				Ref<ArrayMesh> mesh = tr_room_data_to_godot_mesh(room.data, level_materials, level_trans_materials, p_types);

				mi->set_position(Vector3());
				mi->set_owner(p_root->get_owner());
				mi->set_mesh(mesh);
			}

			StaticBody3D* static_body = memnew(StaticBody3D);
			if (static_body) {
				static_body->set_name(String("RoomStaticBody3D_") + itos(room_idx));
				node_3d->add_child(static_body);
				static_body->set_owner(p_root->get_owner());
				static_body->set_position(Vector3());

				CollisionShape3D* collision_shape = tr_room_to_collision_shape(room, p_floor_data);

				static_body->add_child(collision_shape);
				collision_shape->set_owner(p_root->get_owner());
				collision_shape->set_position(Vector3());
			}

			uint32_t static_mesh_idx = 0;
			for (const TRRoomStaticMesh &room_static_mesh : room.room_static_meshes) {
				int mesh_static_number = room_static_mesh.mesh_id;
				
				if (p_types.static_info_map.has(mesh_static_number)) {
					TRStaticInfo static_info = p_types.static_info_map[mesh_static_number];
				}

				if (p_types.static_info_map.has(mesh_static_number)) {
					TRStaticInfo static_info = p_types.static_info_map[mesh_static_number];
					if (static_info.flags & 2) {
						int mesh_number = static_info.mesh_number;
						if (mesh_number < meshes.size() && mesh_number >= 0) {
							Ref<ArrayMesh> mesh = meshes.get(mesh_number);
							MeshInstance3D* mi = memnew(MeshInstance3D);
							node_3d->add_child(mi);
							mi->set_mesh(mesh);
							mi->set_name(String("StaticMeshInstance_") + itos(static_mesh_idx));
							mi->set_owner(p_root->get_owner());
							mi->set_transform(node_3d->get_transform().affine_inverse() * Transform3D(Basis().rotated(
								Vector3(0.0, 1.0, 0.0), Math::deg_to_rad((float)room_static_mesh.rotation / 16384.0f * -90)), Vector3(
									room_static_mesh.pos.x * TR_TO_GODOT_SCALE,
									room_static_mesh.pos.y * -TR_TO_GODOT_SCALE,
									room_static_mesh.pos.z * -TR_TO_GODOT_SCALE)));
						}
					}
				}
			}
			static_mesh_idx++;
		}

		room_idx++;
	}

	return rooms_node;
}

void TRLevel::clear_level() {
	while (get_child_count() > 0) {
		get_child(0)->queue_free();
		remove_child(get_child(0));
	}
}

void TRLevel::load_level() {
	Error error;

	Ref<TRFileAccess> file = TRFileAccess::open(level_path, FileAccess::READ, &error);
	if (error != Error::OK) {
		return;
	}

	TRLevelFormat format = TR1_PC;
	int32_t version = file->get_s32();
	if (version == 0x00000020) {
		format = TR1_PC;
	} else if (version == 0x0000002D) {
		format = TR2_PC;
	} else if (version == 0xFF080038 || version == 0xFF180038) {
		format = TR3_PC;
	} else if (version == 0x00345254 && level_path.get_extension() == "tr4") {
		format = TR4_PC;
	}

	ERR_FAIL_COND(format == TR_VERSION_UNKNOWN);

	TRLevelData level_data = load_level_type(file, format);

	Node3D* rooms_node = generate_godot_scene(
		this, 
		level_data.textures,
		level_data.palette,
		level_data.rooms,
		level_data.entities,
		level_data.types,
		level_data.floor_data,
		format);
}

void dump_8bit_textures(Vector<PackedByteArray> p_textures, Vector<TRColor3> p_palette) {
	// Image Textures
	for (int i = 0; i < p_textures.size(); i++) {
		PackedByteArray current_texture = p_textures.get(i);
		Ref<Image> image = memnew(Image(TR_TEXTILE_SIZE, TR_TEXTILE_SIZE, false, Image::FORMAT_RGBA8));
		for (int x = 0; x < TR_TEXTILE_SIZE; x++) {
			for (int y = 0; y < TR_TEXTILE_SIZE; y++) {
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

		String image_path = String("Texture_") + itos(i) + String(".png");
		image->save_png(image_path);
	}
}

void dump_32bit_textures(Vector<PackedByteArray> p_textures) {
	// Image Textures
	for (int i = 0; i < p_textures.size(); i++) {
		PackedByteArray current_texture = p_textures.get(i);
		Ref<Image> image = memnew(Image(TR_TEXTILE_SIZE, TR_TEXTILE_SIZE, false, Image::FORMAT_RGBA8));
		for (int x = 0; x < TR_TEXTILE_SIZE; x++) {
			for (int y = 0; y < TR_TEXTILE_SIZE; y++) {
				uint8_t a = current_texture.get(((y * TR_TEXTILE_SIZE) + x) * sizeof(uint32_t));
				uint8_t r = current_texture.get(((y * TR_TEXTILE_SIZE) + x) * sizeof(uint32_t) + 1);
				uint8_t g = current_texture.get(((y * TR_TEXTILE_SIZE) + x) * sizeof(uint32_t) + 2);
				uint8_t b = current_texture.get(((y * TR_TEXTILE_SIZE) + x) * sizeof(uint32_t) + 3);

				image->set_pixel(x, y, Color(((float)r / 255.0f), ((float)g / 255.0f), ((float)b / 255.0f), a));
			}
		}

		String image_path = String("Texture_") + itos(i) + String(".png");
		image->save_png(image_path);
	}
}

TRLevelData TRLevel::load_level_type(Ref<TRFileAccess> p_file, TRLevelFormat p_level_format) {
	Vector<TRColor3> palette;
	Vector<TRColor4> palette32;
	Vector<PackedByteArray> textures;

	if (p_level_format == TR1_PC || p_level_format == TR2_PC || p_level_format == TR3_PC) {
		if (p_level_format == TR2_PC || p_level_format == TR3_PC) {
			palette = read_tr_palette(p_file);
			palette32 = read_tr_palette_32(p_file);
		}

		textures = read_tr_texture_pages(p_file, p_level_format);
	} else {
		uint16_t num_room_textiles = p_file->get_u16();
		uint16_t num_obj_textiles = p_file->get_u16();
		uint16_t num_bump_textiles = p_file->get_u16();

		uint32_t total_textiles = num_room_textiles + num_obj_textiles + num_bump_textiles;

		uint32_t textiles32_uncompressed_size = p_file->get_u32();
		uint32_t textiles32_compressed_size = p_file->get_u32();

		StreamPeerGZIP zip_buffer;
		zip_buffer.put_data(p_file->get_buffer(textiles32_compressed_size).ptr(), textiles32_compressed_size);
		zip_buffer.start_decompression(true, textiles32_uncompressed_size);

		Vector<PackedByteArray> textures_32;
		for (uint32_t i = 0; i < total_textiles; i++) {
			uint64_t texture_buffer_size = TR_TEXTILE_SIZE * TR_TEXTILE_SIZE * sizeof(uint32_t);

			PackedByteArray texture_buf;
			texture_buf.resize(texture_buffer_size);

			Error err = zip_buffer.get_data(texture_buf.ptrw(), texture_buffer_size);
			ERR_FAIL_COND_V(err != OK, TRLevelData());

			textures_32.push_back(texture_buf);
		}

		dump_32bit_textures(textures_32);

		return TRLevelData();
	}

	int32_t file_level_num = p_file->get_s32();

	Vector<TRRoom> rooms = read_tr_rooms(p_file, p_level_format);

	PackedByteArray floor_data = read_tr_floor_data(p_file);

	TRTypes types = read_tr_types(p_file, p_level_format);

	read_tr_sprites(p_file);

	read_tr_cameras(p_file);

	read_tr_sound_effects(p_file);

	read_tr_nav_cells(p_file, p_level_format);

	read_tr_animated_textures(p_file);

	if (p_level_format == TR3_PC) {
		types.texture_infos = read_tr_texture_infos(p_file);
	}

	Vector<TREntity> entities = read_tr_entities(p_file, p_level_format);

	read_tr_lightmap(p_file);

	if (p_level_format == TR1_PC) {
		palette = read_tr_palette(p_file);
	}

	if (p_level_format == TR1_PC || p_level_format == TR2_PC || p_level_format == TR3_PC) {
		read_tr_camera_frames(p_file);
		read_tr_demo_frames(p_file);
	}

	// CURRENTLY BROKEN
	if (0) {
		Vector<uint16_t> sound_map = read_tr_sound_map(p_file, p_level_format);
		Vector<TRSoundInfo> sound_infos = read_tr_sound_infos(p_file, p_level_format);

		PackedByteArray buffer = read_tr_sound_buffer(p_file);
		PackedInt32Array indices = read_tr_sound_indices(p_file);

		for (int i = 0; i < indices.size(); i++) {
		}
	}

#if 0
	dump_8bit_textures(textures, palette);
#endif

	TRLevelData level_data;
	level_data.textures = textures;
	level_data.palette = palette;
	level_data.rooms = rooms;
	level_data.entities = entities;
	level_data.types = types;
	level_data.floor_data = floor_data;

	return level_data;
}

void TRLevel::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			break;
	}
}