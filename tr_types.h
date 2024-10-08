#pragma once

#include "tr_module_extension_abstraction_layer.hpp"

#ifdef IS_MODULE
#include "scene/3d/node_3d.h"
#include "core/object/class_db.h"
#else
#include <godot_cpp/classes/node3D.hpp>
#include <godot_cpp/core/class_db.hpp>
#endif

typedef int16_t tr_angle;

struct TRVertex {
	int16_t x;
	int16_t y;
	int16_t z;
};

struct TRPos {
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;
};

struct TRRot {
	tr_angle x = 0;
	tr_angle y = 0;
	tr_angle z = 0;
};

struct TRTransform {
	TRPos pos = TRPos();
	TRRot rot = TRRot();
};

struct TRColor3 {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct TRColor4 {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};

struct TRRoomVertex {
	TRVertex vertex;
	Color color;
	uint16_t attributes;
	Color color2;
};

struct TRFaceTriangle {
	int16_t indices[3];
	int16_t tex_info_id;
	int16_t effect_info;
};

struct TRFaceQuad {
	int16_t indices[4];
	int16_t tex_info_id;
	int16_t effect_info;
};

struct TRRoomSprite {
	int16_t vertex;
	int16_t texture;
};

struct TRRoomData {
	int16_t room_vertex_count;
	Vector<TRRoomVertex> room_vertices;

	int16_t room_quad_count;
	Vector<TRFaceQuad> room_quads;

	int16_t room_triangle_count;
	Vector<TRFaceTriangle> room_triangles;

	int16_t room_sprite_count;
	Vector<TRRoomSprite> room_sprites;
};

struct TRRoomSector {
	uint16_t floor_data_index; // Pointer to the floor data buffer
	int16_t box_index; // Encodes more info in later games
	uint8_t room_below; // 0xff = none
	int8_t floor; // Height of the floor 
	uint8_t room_above; // 0xff = none
	int8_t ceiling; // Height of the ceiling
};

struct TRRoomLight {
	TRPos pos;

	TRColor3 color;

	uint8_t light_type;

	uint32_t intensity;
	uint32_t intensity_alt;
	
	uint32_t fade;
	uint32_t fade_alt;
};

struct TRRoomStaticMesh {
	TRPos pos;
	tr_angle rotation;
	uint16_t intensity;
	uint16_t intensity2;
	uint16_t mesh_id;
};

struct TRRoomPortal {
	uint16_t adjoining_room;
	TRVertex normal;
	TRVertex vertices[4];
};

struct TRRoomInfo {
	int32_t x;
	int32_t z;
	int32_t y_bottom;
	int32_t y_top;
};

struct TRRoom {
	TRRoomInfo info;
	TRRoomData data;

	int16_t portal_count;
	Vector<TRRoomPortal> portals;

	uint16_t sector_count_x;
	uint16_t sector_count_z;

	Vector<TRRoomSector> sectors;

	Color ambient_light;
	Color ambient_light_alt;
	int16_t light_mode;

	uint16_t light_count;
	Vector<TRRoomLight> lights;

	uint16_t room_static_mesh_count;
	Vector<TRRoomStaticMesh> room_static_meshes;

	int16_t alternative_room;
	uint16_t room_flags;

	uint8_t water_scheme;
	uint8_t reverb_info;
};

struct TRBoundingBox {
	int16_t x_min;
	int16_t x_max;
	int16_t y_min;
	int16_t y_max;
	int16_t z_min;
	int16_t z_max;
};

struct TRAnimFrame {
	Vector<TRTransform> transforms;
	TRBoundingBox bounding_box;
};

struct TRAnimation {
	int32_t frame_offset;
	uint8_t frame_skip;
	uint8_t frame_size;
	int16_t current_animation_state;
	int32_t velocity;
	int32_t acceleration;
	int32_t lateral_velocity;
	int32_t lateral_acceleration;
	int16_t frame_base;
	int16_t frame_end;
	int16_t next_animation_number;
	int16_t next_frame_number;
	int16_t number_state_changes;
	int16_t state_change_index;
	int16_t number_commands;
	int16_t command_index;
	Vector<TRAnimFrame> frames;
};

struct TRAnimationStateChange {
	int16_t target_animation_state;
	int16_t number_dispatches;
	int16_t dispatch_index;
};

struct TRAnimationDispatch {
	int16_t start_frame;
	int16_t end_frame;
	int16_t target_animation_number;
	int16_t target_frame_number;
};

struct TRAnimationCommand {
	int16_t command;
};

struct TRMoveableInfo {
	int16_t mesh_count;
	int16_t mesh_index;
	int32_t bone_index;
	int32_t frame_base;
	int16_t animation_index;
	int16_t animation_count;
	Vector<TRAnimation> animations;
};

struct TRStaticInfo {
	uint16_t mesh_number;
	TRBoundingBox visibility_box;
	TRBoundingBox collision_box;
	uint16_t flags;
};

struct TRUV {
	uint16_t u;
	uint16_t v;
};

struct TRTextureInfo {
	uint16_t draw_type;
	uint16_t texture_page;
	uint16_t extra_flags;
	TRUV uv[4];
	uint32_t original_u;
	uint32_t original_v;
	uint32_t width;
	uint32_t height;
};

struct tr_face4
{
	uint16_t verticies[4];
	uint16_t texture;
};

struct TRMesh {
	TRVertex center;
	int32_t collision_radius;

	int16_t vertex_count;
	Vector<TRVertex> vertices;

	int16_t normal_count;
	// Normals if positive, colors if negative
	Vector<TRVertex> normals;

	Vector<int16_t> colors;

	int16_t texture_quads_count;
	Vector<TRFaceQuad> texture_quads;

	int16_t texture_triangles_count;
	Vector<TRFaceTriangle> texture_triangles;

	int16_t color_quads_count;
	Vector<TRFaceQuad> color_quads;

	int16_t color_triangles_count;
	Vector<TRFaceTriangle> color_triangles;
};

struct TRSoundInfo {
	uint16_t sample_index;
	uint16_t volume;
	uint8_t range;
	uint16_t chance;
	uint8_t pitch;
	uint16_t characteristics;
};

struct TRTypes {
	Vector<TRTextureInfo> texture_infos;
	Vector<TRMesh> meshes;
	Vector<TRAnimation> animations;
	Vector<TRAnimationStateChange> animation_state_changes;
	Vector<TRAnimationDispatch> animation_dispatches;
	Vector<TRAnimationCommand> animation_commands;
	Vector<uint16_t> sound_map;
	PackedInt32Array mesh_tree_buffer;

	HashMap<int, TRMoveableInfo > moveable_info_map;
	HashMap<int, TRStaticInfo> static_info_map;
};

enum TRBoneBits {
	TR_BONE_POP = 1 << 0,
	TR_BONE_PUSH = 1 << 1,
	TR_BONE_ROT_X = 1 << 2,
	TR_BONE_ROT_Y = 1 << 3,
	TR_BONE_ROT_Z = 1 << 4,
};

struct TRSpriteInfo {
	uint16_t texture_page;
	uint16_t offset;
	uint16_t width;
	uint16_t height;
	int16_t x1;
	int16_t y1;
	int16_t x2;
	int16_t y2;
};

struct TRGameVector {
	TRPos pos;
	int16_t room_number;
	int16_t box_number;
};

struct TRObjectVector {
	TRPos pos;
	int16_t data;
	int16_t flags;
};

struct TRCameraInfo {
	TRGameVector pos;
	TRGameVector target;
	int32_t type;
	int32_t shift;
	int32_t flags;
	int32_t fixed_camera;
	int32_t number_frames;
	int32_t bounce;
	int32_t underwater;
	int32_t target_distance;
	int32_t target_square;
	int16_t target_angle;
	int16_t actual_angle;
	int16_t target_elevation;
	int16_t box;
	int16_t number;
	int16_t last;
	int16_t timer;
	int16_t speed;
	Vector<TRGameVector> fixed;
};

struct TRFlybyCameraInfo {
	TRPos position;
	TRPos angles;
	uint8_t sequence;
	uint8_t index;
	uint16_t fov;
	int16_t roll;
	uint16_t timer;
	uint16_t speed;
	uint16_t flags;
	uint32_t room_id;
};

struct TRNavCell {
	int32_t left;
	int32_t right;
	int32_t top;
	int32_t bottom;
	int16_t height;
	int16_t overlap_index;
};

struct TREntity {
	uint32_t type_id;
	int16_t room_number;
	TRTransform transform;
	int16_t timer;
	int16_t flags;
	int16_t shade;
	int16_t shade2;
	int16_t ocb;
};

struct TRCameraFrame {
	TRPos target;
	TRPos pos;
	int16_t fov;
	int16_t roll;
};

struct TRLevelData {
	Vector<PackedByteArray> textures;
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