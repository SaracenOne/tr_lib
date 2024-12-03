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

struct GeometryShift {
	int8_t z_floor_shift = 0;
	int8_t x_floor_shift = 0;
	int8_t z_ceiling_shift = 0;
	int8_t x_ceiling_shift = 0;
	uint8_t portal_room = 0xff;
};

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

struct TRInterpolatedFrame {
	TRAnimFrame first_frame;
	TRAnimFrame second_frame;
	real_t interpolation;
};

static real_t u_fixed_16_to_float(uint16_t p_fixed, bool no_fraction) {
	if (no_fraction) {
		return (real_t)((p_fixed & 0xff00) >> 8) + (real_t)((p_fixed & 0x00ff) / 255.0f);
	}
	else {
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

TRInterpolatedFrame get_final_frame_for_animation(int32_t p_anim_idx, Vector<TRAnimation>& p_animations) {
	TRInterpolatedFrame interpolated_frame;
	interpolated_frame.interpolation = 0.0;

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
				}
				else {
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

				const AnimationNodeMetaInfo* info = get_animation_node_meta_info_for_animation(p_type_info_id, animation_name, p_level_format);
				if (info) {
					state_machine->add_node(animation_name, animation_node, info->position);
				} else {
					size_t x_pos = anim_idx % grid_size;
					size_t y_pos = anim_idx / grid_size;

					state_machine->add_node(animation_name, animation_node, Vector2(real_t(x_pos) * ANIMATION_GRAPH_SPACING, real_t(y_pos) * ANIMATION_GRAPH_SPACING));
				}
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

Vector<Node3D *> create_godot_nodes_for_moveables(
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

CollisionShape3D *tr_room_to_godot_collision_shape(const TRRoom& p_current_room, PackedByteArray p_floor_data, Vector<TRRoom> p_rooms) {
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

			{
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
					buf.append(Vector3(SQUARE_SIZE* x_sector, sw_offset_f, -(SQUARE_SIZE * y_sector + SQUARE_SIZE)));
					buf.append(Vector3(SQUARE_SIZE * x_sector + SQUARE_SIZE, ne_offset_f, -(SQUARE_SIZE * y_sector)));
					buf.append(Vector3(SQUARE_SIZE* x_sector, nw_offset_f, -(SQUARE_SIZE * y_sector)));

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

					buf.append(Vector3(SQUARE_SIZE * x_sector + SQUARE_SIZE, ne_offset_f, -(SQUARE_SIZE * y_sector)));
					buf.append(Vector3(SQUARE_SIZE * x_sector + SQUARE_SIZE, se_offset_f, -(SQUARE_SIZE * y_sector + SQUARE_SIZE)));
					buf.append(Vector3(SQUARE_SIZE * x_sector, sw_offset_f, -(SQUARE_SIZE * y_sector + SQUARE_SIZE)));
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
									buf.append(next_top_left);
									buf.append(current_top_right);
									buf.append(current_top_left);
								}

								if (((current_top_right.y - next_top_right.y) > CMP_EPSILON)) {
									buf.append(next_top_left);
									buf.append(next_top_right);
									buf.append(current_top_right);
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
							buf.append(current_top_left);
							buf.append(current_bottom_right);
							buf.append(current_bottom_left);

							buf.append(current_top_left);
							buf.append(current_top_right);
							buf.append(current_bottom_right);
						} else {
							Vector3 next_bottom_left =	Vector3((SQUARE_SIZE * x_sector),				south_calc.floor_north_west,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
							Vector3 next_bottom_right =	Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE,	south_calc.floor_north_east,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
							Vector3 next_top_left =		Vector3((SQUARE_SIZE * x_sector),				south_calc.ceiling_north_west,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
							Vector3 next_top_right =	Vector3((SQUARE_SIZE * x_sector) + SQUARE_SIZE,	south_calc.ceiling_north_east,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
						
							// Bottom quad difference
							if (((current_bottom_left.y - next_bottom_left.y) < 0.0 || (current_bottom_right.y - next_bottom_right.y) < 0.0)) {
								if ((current_bottom_left.y - next_bottom_left.y) < CMP_EPSILON) {
									buf.append(next_bottom_left);
									buf.append(current_bottom_right);
									buf.append(current_bottom_left);
								}

								if ((current_bottom_right.y - next_bottom_right.y) < CMP_EPSILON) {
									buf.append(next_bottom_left);
									buf.append(next_bottom_right);
									buf.append(current_bottom_right);
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
									buf.append(next_top_left);
									buf.append(current_top_right);
									buf.append(current_top_left);
								}

								if (((current_top_right.y - next_top_right.y) > CMP_EPSILON)) {
									buf.append(next_top_left);
									buf.append(next_top_right);
									buf.append(current_top_right);
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
							buf.append(current_top_left);
							buf.append(current_bottom_right);
							buf.append(current_bottom_left);

							buf.append(current_top_left);
							buf.append(current_top_right);
							buf.append(current_bottom_right);
						} else {
							Vector3 next_bottom_left =	Vector3((SQUARE_SIZE * x_sector), west_calc.floor_north_east,	-(SQUARE_SIZE * y_sector));
							Vector3 next_bottom_right =	Vector3((SQUARE_SIZE * x_sector), west_calc.floor_south_east,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
							Vector3 next_top_left =		Vector3((SQUARE_SIZE * x_sector), west_calc.ceiling_north_east,	-(SQUARE_SIZE * y_sector));
							Vector3 next_top_right =	Vector3((SQUARE_SIZE * x_sector), west_calc.ceiling_south_east,	-(SQUARE_SIZE * y_sector) - SQUARE_SIZE);
							
							// Bottom quad difference
							if (((current_bottom_left.y - next_bottom_left.y) < 0.0 || (current_bottom_right.y - next_bottom_right.y) < 0.0)) {
								if ((current_bottom_left.y - next_bottom_left.y) < CMP_EPSILON) {
									buf.append(next_bottom_left);
									buf.append(current_bottom_right);
									buf.append(current_bottom_left);
								}

								if ((current_bottom_right.y - next_bottom_right.y) < CMP_EPSILON) {
									buf.append(next_bottom_left);
									buf.append(next_bottom_right);
									buf.append(current_bottom_right);
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


Ref<Material> generate_tr_godot_generic_material(Ref<ImageTexture> p_image_texture, bool p_is_transparent) {
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

Ref<Material> generate_tr_godot_shader_material(Ref<ImageTexture> p_image_texture, Ref<Shader> p_shader) {
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
		palette_material = generate_tr_godot_generic_material(it, false);

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
		level_materials.append(generate_tr_godot_shader_material(image_textures[i], level_shader));
		level_trans_materials.append(generate_tr_godot_shader_material(image_textures[i], level_trans_shader));
		entity_materials.append(generate_tr_godot_generic_material(image_textures[i], false));
		entity_trans_materials.append(generate_tr_godot_generic_material(image_textures[i], true));
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

		Vector<Node3D*> moveable_node = create_godot_nodes_for_moveables(
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

					CollisionShape3D* collision_shape = tr_room_to_godot_collision_shape(room, level_data.floor_data, level_data.rooms);

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