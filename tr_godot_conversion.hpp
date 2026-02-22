#include "tr_level.hpp"

#include <core/io/stream_peer_gzip.h>
#include <core/math/math_funcs.h>
#include <editor/file_system/editor_file_system.h>
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
#include <scene/3d/physics/animatable_body_3d.h>
#include <scene/3d/reflection_probe.h>
#include <core/crypto/crypto_core.h>
#include <core/crypto/hashing_context.h>
#include <core/io/config_file.h>
#include <core/variant/variant_utility.h>

#define TR_TO_GODOT_SCALE 0.001 * 2.0

const real_t TR_SQUARE_SIZE = 1024.0 * TR_TO_GODOT_SCALE;
const real_t TR_CLICK_SIZE = TR_SQUARE_SIZE / 4.0;

struct GeometryShift {
	int8_t ne_floor_shift = 0;
	int8_t nw_floor_shift = 0;
	int8_t se_floor_shift = 0;
	int8_t sw_floor_shift = 0;
	
	int8_t ne_ceiling_shift = 0;
	int8_t nw_ceiling_shift = 0;
	int8_t se_ceiling_shift = 0;
	int8_t sw_ceiling_shift = 0;

	bool rotate_floor_triangles = false;
	bool rotate_ceiling_triangles = false;

	bool cull_first_floor_triangle = false;
	bool cull_second_floor_triangle = false;

	bool cull_first_ceiling_triangle = false;
	bool cull_second_ceiling_triangle = false;

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

static uint32_t get_lowest_corner(uint32_t v1, uint32_t v2, uint32_t v3, uint32_t v4) {
	v1 = (v1 > v2) ? (v1) : (v2);
	v2 = (v3 > v4) ? (v3) : (v4);

	return (v1 > v2) ? (v1) : (v2);
}

static GeometryShift parse_floor_data_entry(PackedByteArray p_floor_data, uint16_t p_index) {
	GeometryShift geo_shift{};

	bool parsing = true;

	// ???
	if (p_index == 0) {
		return geo_shift;
	}

	int8_t x_floor_shift = 0;
	int8_t z_floor_shift = 0;

	int8_t x_ceiling_shift = 0;
	int8_t z_ceiling_shift = 0;

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
				x_floor_shift = p_floor_data.get(p_index * 2);
				z_floor_shift = p_floor_data.get((p_index * 2) + 1);

				p_index++;
				break;
			}
			case 0x03: {
				printf("ceiling_slant\n");
				x_ceiling_shift = p_floor_data.get(p_index * 2);
				z_ceiling_shift = p_floor_data.get((p_index * 2) + 1);

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

				uint8_t first_byte = p_floor_data.get(p_index * 2);
				uint8_t second_byte = p_floor_data.get((p_index * 2) + 1);

				uint8_t ne = first_byte & 0x0f;
				uint8_t nw = (first_byte & 0xf0) >> 4;
				uint8_t sw = second_byte & 0x0f;
				uint8_t se = (second_byte & 0xf0) >> 4;

				int8_t base = get_lowest_corner(ne, nw, sw, se);

				geo_shift.ne_floor_shift = -base + ne;
				geo_shift.se_floor_shift = -base + se;
				geo_shift.nw_floor_shift = -base + nw;
				geo_shift.sw_floor_shift = -base + sw;

				geo_shift.rotate_floor_triangles = false;
				geo_shift.cull_first_floor_triangle = false;
				geo_shift.cull_second_floor_triangle = false;

				p_index++;
				break;
			}
			case 0x08: {
				printf("ne_sw_floor_triangle\n");

				uint8_t first_byte = p_floor_data.get(p_index * 2);
				uint8_t second_byte = p_floor_data.get((p_index * 2) + 1);

				uint8_t ne = first_byte & 0x0f;
				uint8_t nw = (first_byte & 0xf0) >> 4;
				uint8_t sw = second_byte & 0x0f;
				uint8_t se = (second_byte & 0xf0) >> 4;

				int8_t base = get_lowest_corner(ne, nw, sw, se);

				geo_shift.ne_floor_shift = -base + ne;
				geo_shift.se_floor_shift = -base + se;
				geo_shift.nw_floor_shift = -base + nw;
				geo_shift.sw_floor_shift = -base + sw;

				geo_shift.rotate_floor_triangles = true;
				geo_shift.cull_first_floor_triangle = false;
				geo_shift.cull_second_floor_triangle = false;

				p_index++;
				break;
			}
			case 0x09: {
				printf("nw_se_ceiling_triangle\n");

				uint8_t first_byte = p_floor_data.get(p_index * 2);
				uint8_t second_byte = p_floor_data.get((p_index * 2) + 1);

				uint8_t se = first_byte & 0x0f;
				uint8_t sw = (first_byte & 0xf0) >> 4;
				uint8_t nw = second_byte & 0x0f;
				uint8_t ne = (second_byte & 0xf0) >> 4;

				int8_t base = get_lowest_corner(ne, nw, sw, se);

				geo_shift.ne_ceiling_shift = base - ne;
				geo_shift.se_ceiling_shift = base - se;
				geo_shift.nw_ceiling_shift = base - nw;
				geo_shift.sw_ceiling_shift = base - sw;

				geo_shift.rotate_ceiling_triangles = false;
				geo_shift.cull_first_ceiling_triangle = false;
				geo_shift.cull_second_ceiling_triangle = false;

				p_index++;
				break;
			}
			case 0x0a: {
				printf("ne_sw_ceiling_triangle\n");

				uint8_t first_byte = p_floor_data.get(p_index * 2);
				uint8_t second_byte = p_floor_data.get((p_index * 2) + 1);

				uint8_t se = first_byte & 0x0f;
				uint8_t sw = (first_byte & 0xf0) >> 4;
				uint8_t nw = second_byte & 0x0f;
				uint8_t ne = (second_byte & 0xf0) >> 4;

				int8_t base = get_lowest_corner(ne, nw, sw, se);

				geo_shift.ne_ceiling_shift = base - ne;
				geo_shift.se_ceiling_shift = base - se;
				geo_shift.nw_ceiling_shift = base - nw;
				geo_shift.sw_ceiling_shift = base - sw;

				geo_shift.rotate_ceiling_triangles = true;
				geo_shift.cull_first_ceiling_triangle = false;
				geo_shift.cull_second_ceiling_triangle = false;

				p_index++;
				break;
			}
			case 0x0b: {
				printf("nw_floor_triangle\n");

				uint8_t first_byte = p_floor_data.get(p_index * 2);
				uint8_t second_byte = p_floor_data.get((p_index * 2) + 1);

				uint8_t ne = first_byte & 0x0f;
				uint8_t nw = (first_byte & 0xf0) >> 4;
				uint8_t sw = second_byte & 0x0f;
				uint8_t se = (second_byte & 0xf0) >> 4;

				int8_t base = get_lowest_corner(ne, nw, sw, se);

				geo_shift.ne_floor_shift = -base + ne;
				geo_shift.se_floor_shift = -base + se;
				geo_shift.nw_floor_shift = -base + nw;
				geo_shift.sw_floor_shift = -base + sw;

				geo_shift.rotate_floor_triangles = false;
				geo_shift.cull_first_floor_triangle = true;
				geo_shift.cull_second_floor_triangle = false;

				p_index++;
				break;
			}
			case 0x0c: {
				printf("se_floor_triangle\n");

				uint8_t first_byte = p_floor_data.get(p_index * 2);
				uint8_t second_byte = p_floor_data.get((p_index * 2) + 1);

				uint8_t ne = first_byte & 0x0f;
				uint8_t nw = (first_byte & 0xf0) >> 4;
				uint8_t sw = second_byte & 0x0f;
				uint8_t se = (second_byte & 0xf0) >> 4;

				int8_t base = get_lowest_corner(ne, nw, sw, se);

				geo_shift.ne_floor_shift = -base + ne;
				geo_shift.se_floor_shift = -base + se;
				geo_shift.nw_floor_shift = -base + nw;
				geo_shift.sw_floor_shift = -base + sw;

				geo_shift.rotate_floor_triangles = false;
				geo_shift.cull_first_floor_triangle = false;
				geo_shift.cull_second_floor_triangle = true;

				p_index++;
				break;
			}
			case 0x0d: {
				printf("ne_floor_triangle\n");

				uint8_t first_byte = p_floor_data.get(p_index * 2);
				uint8_t second_byte = p_floor_data.get((p_index * 2) + 1);

				uint8_t ne = first_byte & 0x0f;
				uint8_t nw = (first_byte & 0xf0) >> 4;
				uint8_t sw = second_byte & 0x0f;
				uint8_t se = (second_byte & 0xf0) >> 4;

				int8_t base = get_lowest_corner(ne, nw, sw, se);

				geo_shift.ne_floor_shift = -base + ne;
				geo_shift.se_floor_shift = -base + se;
				geo_shift.nw_floor_shift = -base + nw;
				geo_shift.sw_floor_shift = -base + sw;

				geo_shift.rotate_floor_triangles = true;
				geo_shift.cull_first_floor_triangle = true;
				geo_shift.cull_second_floor_triangle = false;

				p_index++;
				break;
			}
			case 0x0e: {
				printf("sw_floor_triangle\n");

				uint8_t first_byte = p_floor_data.get(p_index * 2);
				uint8_t second_byte = p_floor_data.get((p_index * 2) + 1);

				uint8_t ne = first_byte & 0x0f;
				uint8_t nw = (first_byte & 0xf0) >> 4;
				uint8_t sw = second_byte & 0x0f;
				uint8_t se = (second_byte & 0xf0) >> 4;

				int8_t base = get_lowest_corner(ne, nw, sw, se);

				geo_shift.ne_floor_shift = -base + ne;
				geo_shift.se_floor_shift = -base + se;
				geo_shift.nw_floor_shift = -base + nw;
				geo_shift.sw_floor_shift = -base + sw;

				geo_shift.rotate_floor_triangles = true;
				geo_shift.cull_first_floor_triangle = false;
				geo_shift.cull_second_floor_triangle = true;

				p_index++;
				break;
			}

			case 0x0f: {
				printf("nw_ceiling_triangle\n");

				uint8_t first_byte = p_floor_data.get(p_index * 2);
				uint8_t second_byte = p_floor_data.get((p_index * 2) + 1);

				uint8_t se = first_byte & 0x0f;
				uint8_t sw = (first_byte & 0xf0) >> 4;
				uint8_t nw = second_byte & 0x0f;
				uint8_t ne = (second_byte & 0xf0) >> 4;

				int8_t base = get_lowest_corner(ne, nw, sw, se);

				geo_shift.ne_ceiling_shift = base - ne;
				geo_shift.se_ceiling_shift = base - se;
				geo_shift.nw_ceiling_shift = base - nw;
				geo_shift.sw_ceiling_shift = base - sw;

				geo_shift.rotate_ceiling_triangles = false;
				geo_shift.cull_first_ceiling_triangle = true;
				geo_shift.cull_second_ceiling_triangle = false;

				p_index++;
				break;
			}
			case 0x10: {
				printf("se_ceiling_triangle\n");

				uint8_t first_byte = p_floor_data.get(p_index * 2);
				uint8_t second_byte = p_floor_data.get((p_index * 2) + 1);

				uint8_t se = first_byte & 0x0f;
				uint8_t sw = (first_byte & 0xf0) >> 4;
				uint8_t nw = second_byte & 0x0f;
				uint8_t ne = (second_byte & 0xf0) >> 4;

				int8_t base = get_lowest_corner(ne, nw, sw, se);

				geo_shift.ne_ceiling_shift = base - ne;
				geo_shift.se_ceiling_shift = base - se;
				geo_shift.nw_ceiling_shift = base - nw;
				geo_shift.sw_ceiling_shift = base - sw;

				geo_shift.rotate_ceiling_triangles = false;
				geo_shift.cull_first_ceiling_triangle = false;
				geo_shift.cull_second_ceiling_triangle = true;

				p_index++;
				break;
			}
			case 0x11: {
				printf("ne_ceiling_triangle\n");

				uint8_t first_byte = p_floor_data.get(p_index * 2);
				uint8_t second_byte = p_floor_data.get((p_index * 2) + 1);

				uint8_t se = first_byte & 0x0f;
				uint8_t sw = (first_byte & 0xf0) >> 4;
				uint8_t nw = second_byte & 0x0f;
				uint8_t ne = (second_byte & 0xf0) >> 4;

				int8_t base = get_lowest_corner(ne, nw, sw, se);

				geo_shift.ne_ceiling_shift = base - ne;
				geo_shift.se_ceiling_shift = base - se;
				geo_shift.nw_ceiling_shift = base - nw;
				geo_shift.sw_ceiling_shift = base - sw;

				geo_shift.rotate_ceiling_triangles = true;
				geo_shift.cull_first_ceiling_triangle = true;
				geo_shift.cull_second_ceiling_triangle = false;

				p_index++;
				break;
			}
			case 0x12: {
				printf("sw_ceiling_triangle\n");

				uint8_t first_byte = p_floor_data.get(p_index * 2);
				uint8_t second_byte = p_floor_data.get((p_index * 2) + 1);

				uint8_t se = first_byte & 0x0f;
				uint8_t sw = (first_byte & 0xf0) >> 4;
				uint8_t nw = second_byte & 0x0f;
				uint8_t ne = (second_byte & 0xf0) >> 4;

				int8_t base = get_lowest_corner(ne, nw, sw, se);

				geo_shift.ne_ceiling_shift = base - ne;
				geo_shift.se_ceiling_shift = base - se;
				geo_shift.nw_ceiling_shift = base - nw;
				geo_shift.sw_ceiling_shift = base - sw;

				geo_shift.rotate_ceiling_triangles = true;
				geo_shift.cull_first_ceiling_triangle = false;
				geo_shift.cull_second_ceiling_triangle = true;

				p_index++;
				break;
			}
		}
	}

	if (x_floor_shift != 0 || z_floor_shift != 0) {
		geo_shift.ne_floor_shift = 0;
		geo_shift.se_floor_shift = 0;
		geo_shift.nw_floor_shift = 0;
		geo_shift.sw_floor_shift = 0;

		if (x_floor_shift < 0) {
			geo_shift.ne_floor_shift = x_floor_shift;
			geo_shift.se_floor_shift = x_floor_shift;
		} else {
			geo_shift.nw_floor_shift = -x_floor_shift;
			geo_shift.sw_floor_shift = -x_floor_shift;
		}

		if (z_floor_shift < 0) {
			geo_shift.se_floor_shift += z_floor_shift;
			geo_shift.sw_floor_shift += z_floor_shift;
		} else {
			geo_shift.ne_floor_shift += -z_floor_shift;
			geo_shift.nw_floor_shift += -z_floor_shift;
		}
	}

	if (x_ceiling_shift != 0 || z_ceiling_shift != 0) {
		geo_shift.ne_ceiling_shift = 0;
		geo_shift.se_ceiling_shift = 0;
		geo_shift.nw_ceiling_shift = 0;
		geo_shift.sw_ceiling_shift = 0;

		if (x_ceiling_shift < 0) {
			geo_shift.nw_ceiling_shift += -x_ceiling_shift;
			geo_shift.sw_ceiling_shift += -x_ceiling_shift;
		} else {
			geo_shift.ne_ceiling_shift += x_ceiling_shift;
			geo_shift.se_ceiling_shift += x_ceiling_shift;
		}

		if (z_ceiling_shift < 0) {
			geo_shift.se_ceiling_shift += -z_ceiling_shift;
			geo_shift.sw_ceiling_shift += -z_ceiling_shift;
		} else {
			geo_shift.ne_ceiling_shift += z_ceiling_shift;
			geo_shift.nw_ceiling_shift += z_ceiling_shift;
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
	} else {
		if (tr_next_animation.frame_skip > 0) {
			int32_t keyframe_idx = next_frame_idx / tr_next_animation.frame_skip;

			// Clamp the keyframe idx.
			if (keyframe_idx >= tr_next_animation.frames.size()) {
				keyframe_idx = tr_next_animation.frames.size() - 1;
			}

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
		} else {
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

Ref<Animation> create_slice(Ref<Animation> p_anim, const uint32_t p_start_frame, const uint32_t p_end_frame) {
	float from = float(p_start_frame) / float(TR_FPS);
	float to = float(p_end_frame) / float(TR_FPS);
	Animation::LoopMode loop_mode = Animation::LOOP_NONE;
	if (from > to) {
		return nullptr;
	}

	bool p_bake_all = true;

	Ref<Animation> new_anim = memnew(Animation);

	for (int j = 0; j < p_anim->get_track_count(); j++) {
		List<float> keys;
		int kc = p_anim->track_get_key_count(j);
		int dtrack = -1;
		for (int k = 0; k < kc; k++) {
			float kt = p_anim->track_get_key_time(j, k);
			if (kt >= from && kt < to) {
				if (dtrack == -1) {
					new_anim->add_track(p_anim->track_get_type(j));
					dtrack = new_anim->get_track_count() - 1;
					new_anim->track_set_path(dtrack, p_anim->track_get_path(j));
					new_anim->track_set_imported(dtrack, true);

					if (kt > (from + 0.01) && k > 0) {
						if (p_anim->track_get_type(j) == Animation::TYPE_POSITION_3D) {
							Vector3 p;
							p_anim->try_position_track_interpolate(j, from, &p);
							new_anim->position_track_insert_key(dtrack, 0, p);
						} else if (p_anim->track_get_type(j) == Animation::TYPE_ROTATION_3D) {
							Quaternion r;
							p_anim->try_rotation_track_interpolate(j, from, &r);
							new_anim->rotation_track_insert_key(dtrack, 0, r);
						} else if (p_anim->track_get_type(j) == Animation::TYPE_SCALE_3D) {
							Vector3 s;
							p_anim->try_scale_track_interpolate(j, from, &s);
							new_anim->scale_track_insert_key(dtrack, 0, s);
						} else if (p_anim->track_get_type(j) == Animation::TYPE_VALUE) {
							Variant var = p_anim->value_track_interpolate(j, from);
							new_anim->track_insert_key(dtrack, 0, var);
						} else if (p_anim->track_get_type(j) == Animation::TYPE_BLEND_SHAPE) {
							float interp;
							p_anim->try_blend_shape_track_interpolate(j, from, &interp);
							new_anim->blend_shape_track_insert_key(dtrack, 0, interp);
						}
					}
				}

				if (p_anim->track_get_type(j) == Animation::TYPE_POSITION_3D) {
					Vector3 p;
					p_anim->position_track_get_key(j, k, &p);
					new_anim->position_track_insert_key(dtrack, kt - from, p);
				} else if (p_anim->track_get_type(j) == Animation::TYPE_ROTATION_3D) {
					Quaternion r;
					p_anim->rotation_track_get_key(j, k, &r);
					new_anim->rotation_track_insert_key(dtrack, kt - from, r);
				} else if (p_anim->track_get_type(j) == Animation::TYPE_SCALE_3D) {
					Vector3 s;
					p_anim->scale_track_get_key(j, k, &s);
					new_anim->scale_track_insert_key(dtrack, kt - from, s);
				} else if (p_anim->track_get_type(j) == Animation::TYPE_VALUE) {
					Variant var = p_anim->track_get_key_value(j, k);
					new_anim->track_insert_key(dtrack, kt - from, var);
				} else if (p_anim->track_get_type(j) == Animation::TYPE_BLEND_SHAPE) {
					float interp;
					p_anim->blend_shape_track_get_key(j, k, &interp);
					new_anim->blend_shape_track_insert_key(dtrack, kt - from, interp);
				}
			}

			if (dtrack != -1 && kt >= to) {
				if (p_anim->track_get_type(j) == Animation::TYPE_POSITION_3D) {
					Vector3 p;
					p_anim->try_position_track_interpolate(j, to, &p);
					new_anim->position_track_insert_key(dtrack, to - from, p);
				} else if (p_anim->track_get_type(j) == Animation::TYPE_ROTATION_3D) {
					Quaternion r;
					p_anim->try_rotation_track_interpolate(j, to, &r);
					new_anim->rotation_track_insert_key(dtrack, to - from, r);
				} else if (p_anim->track_get_type(j) == Animation::TYPE_SCALE_3D) {
					Vector3 s;
					p_anim->try_scale_track_interpolate(j, to, &s);
					new_anim->scale_track_insert_key(dtrack, to - from, s);
				} else if (p_anim->track_get_type(j) == Animation::TYPE_VALUE) {
					Variant var = p_anim->value_track_interpolate(j, to);
					new_anim->track_insert_key(dtrack, to - from, var);
				} else if (p_anim->track_get_type(j) == Animation::TYPE_BLEND_SHAPE) {
					float interp;
					p_anim->try_blend_shape_track_interpolate(j, to, &interp);
					new_anim->blend_shape_track_insert_key(dtrack, to - from, interp);
				}
			}
		}

		if (dtrack == -1 && p_bake_all) {
			new_anim->add_track(p_anim->track_get_type(j));
			dtrack = new_anim->get_track_count() - 1;
			new_anim->track_set_path(dtrack, p_anim->track_get_path(j));
			new_anim->track_set_imported(dtrack, true);
			if (p_anim->track_get_type(j) == Animation::TYPE_POSITION_3D) {
				Vector3 p;
				p_anim->try_position_track_interpolate(j, from, &p);
				new_anim->position_track_insert_key(dtrack, 0, p);
				p_anim->try_position_track_interpolate(j, to, &p);
				new_anim->position_track_insert_key(dtrack, to - from, p);
			} else if (p_anim->track_get_type(j) == Animation::TYPE_ROTATION_3D) {
				Quaternion r;
				p_anim->try_rotation_track_interpolate(j, from, &r);
				new_anim->rotation_track_insert_key(dtrack, 0, r);
				p_anim->try_rotation_track_interpolate(j, to, &r);
				new_anim->rotation_track_insert_key(dtrack, to - from, r);
			} else if (p_anim->track_get_type(j) == Animation::TYPE_SCALE_3D) {
				Vector3 s;
				p_anim->try_scale_track_interpolate(j, from, &s);
				new_anim->scale_track_insert_key(dtrack, 0, s);
				p_anim->try_scale_track_interpolate(j, to, &s);
				new_anim->scale_track_insert_key(dtrack, to - from, s);
			} else if (p_anim->track_get_type(j) == Animation::TYPE_VALUE) {
				Variant var = p_anim->value_track_interpolate(j, from);
				new_anim->track_insert_key(dtrack, 0, var);
				Variant to_var = p_anim->value_track_interpolate(j, to);
				new_anim->track_insert_key(dtrack, to - from, to_var);
			} else if (p_anim->track_get_type(j) == Animation::TYPE_BLEND_SHAPE) {
				float interp;
				p_anim->try_blend_shape_track_interpolate(j, from, &interp);
				new_anim->blend_shape_track_insert_key(dtrack, 0, interp);
				p_anim->try_blend_shape_track_interpolate(j, to, &interp);
				new_anim->blend_shape_track_insert_key(dtrack, to - from, interp);
			}
		}
	}

	new_anim->set_loop_mode(loop_mode);
	new_anim->set_length(to - from);
	new_anim->set_step(p_anim->get_step());

	return new_anim;
}

Ref<AnimationNodeStateMachineTransition> create_animation_transition(
	AnimationNodeStateMachine* p_state_machine,
	int32_t p_start_frame,
	int32_t p_end_frame,
	int32_t p_length,
	int32_t p_target_frame,
	int32_t p_target_length,
	int32_t p_next_state_id) {

	if (p_start_frame < 0) {
		p_start_frame = 0;
	}
	if (p_end_frame > p_length) {
		p_end_frame = p_length;
	}

	real_t start_time = (real_t(p_start_frame) / TR_FPS) - CMP_EPSILON;
	real_t end_time = (real_t(p_end_frame) / TR_FPS) + CMP_EPSILON;
	real_t target_time = real_t(p_target_frame) / TR_FPS;

	Ref<AnimationNodeStateMachineTransition> transition = memnew(AnimationNodeStateMachineTransition);

	transition->set_switch_mode(AnimationNodeStateMachineTransition::SWITCH_MODE_IMMEDIATE);
	transition->set_xfade_time(0.0);
	transition->set_advance_mode(AnimationNodeStateMachineTransition::ADVANCE_MODE_AUTO);
	
	// Experimental
	real_t offset = 0.0;
	if (p_target_length > 0) {
		offset = target_time / (real_t(((p_target_length))) / TR_FPS);
	}
	transition->set("transition_offset", offset);

	String expression = "next_state_id == " + String::num_uint64(p_next_state_id);
	if (p_start_frame > 0) {
		expression += " && playback_position >= " + String::num(start_time);
	}
	if (p_end_frame < p_length) {
		expression += " && playback_position <= " + String::num(end_time);
	}

	transition->set_advance_expression(expression);

	return transition;
}

Vector2 get_position_for_node(uint32_t p_type_info_id, TRLevelFormat p_level_format, String p_animation_name, String p_fallback_animation_name, uint32_t p_anim_idx, uint32_t p_grid_size) {
#define ANIMATION_GRAPH_SPACING 256.0
	
	const AnimationNodeMetaInfo* info = get_animation_node_meta_info_for_animation(p_type_info_id, p_animation_name, p_level_format);
	
	// Attempt to find fallback.
	if (!info && !p_fallback_animation_name.is_empty()) {
		info = get_animation_node_meta_info_for_animation(p_type_info_id, p_fallback_animation_name, p_level_format);
	}
	
	if (info) {
		return info->position;
	} else {
		size_t x_pos = p_anim_idx % p_grid_size;
		size_t y_pos = p_anim_idx / p_grid_size;

		return Vector2(real_t(x_pos) * ANIMATION_GRAPH_SPACING, real_t(y_pos) * ANIMATION_GRAPH_SPACING);
	}
}

Node3D *create_godot_moveable_model(
	uint32_t p_type_info_id,
	TRMoveableInfo p_moveable_info,
	TRTypes p_types,
	Vector<Ref<ArrayMesh>> p_meshes,
	Vector<Ref<AudioStream>> p_samples,
	TRLevelFormat p_level_format,
	bool p_using_auxiliary_animation,
	bool p_use_unique_names,
	bool p_use_skinning) {
	Node3D *new_type_info = memnew(Node3D);
	new_type_info->set_name(get_type_info_name(p_type_info_id, p_level_format));

	Vector<int32_t> bone_stack;

	int32_t offset_bone_index = p_moveable_info.bone_index;

	AudioStreamPlayer3D *audio_stream_player = nullptr;

	Vector<TRPos> animation_position_offsets;

	HashMap<int32_t, int32_t> mesh_to_bone_mapping;
	HashMap<int32_t, int32_t> bone_to_mesh_mapping;

#define LOOPING_ANIMATION_SUFFIX "_looping"

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

			AnimatableBody3D *avatar_bounds = memnew(AnimatableBody3D);
			avatar_bounds->set_name("AvatarBounds");
			avatar_bounds->set_collision_layer(0);
			avatar_bounds->set_collision_mask(0);
			avatar_bounds->call("set_sync_to_physics", true); // TODO: this method should probably be made public in the engine's codebase.
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
			new_type_info->add_child(animation_tree);
			animation_tree->set_animation_player(animation_tree->get_path_to(animation_player));
			animation_tree->set_active(false);
			animation_tree->set_callback_mode_process(AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS);

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

			Vector<bool> apply_180_rotation_on_final_frame;
			Vector<bool> apply_180_rotation_on_first_frame;
			apply_180_rotation_on_first_frame.resize(p_moveable_info.animations.size());
			apply_180_rotation_on_final_frame.resize(p_moveable_info.animations.size());
			for (int64_t i = 0; i < p_moveable_info.animations.size(); i++) {
				apply_180_rotation_on_first_frame.set(i, false);
				apply_180_rotation_on_final_frame.set(i, false);
			}

			HashMap<uint32_t, Vector<uint32_t>> animation_split_table;
			HashMap<uint32_t, uint32_t> animation_loop_offset_table;

			for (int64_t anim_idx = 0; anim_idx < p_moveable_info.animations.size(); anim_idx++) {
				Ref<Animation> godot_animation = memnew(Animation);
				TRAnimation tr_animation = p_moveable_info.animations.get(anim_idx);

				if (!animation_split_table.has(anim_idx)) {
					Vector<uint32_t> split_array;
					animation_split_table.insert(anim_idx, split_array);
				}

				// Always at a split at the beginning.
				animation_split_table.get(anim_idx).append(0);

				String animation_name = get_animation_name(p_type_info_id, anim_idx, p_level_format, p_using_auxiliary_animation);

				godot_animation->set_name(animation_name);

				godot_animations.push_back(godot_animation);

				real_t animation_length = (real_t)((tr_animation.frames.size()) / TR_FPS) * tr_animation.frame_skip;

				if (tr_animation.next_animation_number == p_moveable_info.animation_index + anim_idx && tr_animation.next_frame_number == tr_animation.frame_base) {
					godot_animation->set_loop_mode(Animation::LOOP_LINEAR);
				} else {
					godot_animation->set_loop_mode(Animation::LOOP_NONE);
				}

				godot_animation->set_length(((tr_animation.frame_end - tr_animation.frame_base) + 1) / TR_FPS);
				
				animation_library->add_animation(get_animation_name(p_type_info_id, anim_idx, p_level_format, p_using_auxiliary_animation), godot_animation);

				int32_t target_animation_number = tr_animation.next_animation_number - p_moveable_info.animation_index;

				TRAnimation target_animation = tr_animation;
				if (target_animation_number < p_moveable_info.animations.size() && target_animation_number >= 0) {
					target_animation = p_moveable_info.animations.get(target_animation_number);
				}

				if (tr_animation.next_frame_number != target_animation.frame_base) {
					if (!animation_split_table.has(target_animation_number)) {
						Vector<uint32_t> split_array;
						animation_split_table.insert(target_animation_number, split_array);
					}
					if (!animation_split_table.get(target_animation_number).has(tr_animation.next_frame_number - target_animation.frame_base)) {
						animation_split_table.get(target_animation_number).append(tr_animation.next_frame_number - target_animation.frame_base);
					}

					if (tr_animation.next_animation_number == anim_idx) {
						int32_t frame_start = tr_animation.next_frame_number - tr_animation.frame_base;
						int32_t frame_end = (tr_animation.frame_end - tr_animation.frame_base) + 1;
						if (frame_start > frame_end) {
							frame_start = frame_end;
						}

						animation_loop_offset_table[anim_idx] = frame_start;
					}
				}
				Array animation_state_changes;

				int16_t state_change_index = tr_animation.state_change_index;
				for (int64_t state_change_offset_idx = 0; state_change_offset_idx < tr_animation.number_state_changes; state_change_offset_idx++) {
					ERR_FAIL_INDEX_V(state_change_index + state_change_offset_idx, p_types.animation_state_changes.size(), nullptr);

					TRAnimationStateChange state_change = p_types.animation_state_changes.get(state_change_index + state_change_offset_idx);

					Dictionary state_change_dict;
					int16_t target_animation_state = state_change.target_animation_state;
					int16_t dispatch_index = state_change.dispatch_index;
					state_change_dict.set("target_animation_state_id", target_animation_state);
					state_change_dict.set("target_animation_state_name", get_state_name(p_type_info_id, target_animation_state, p_level_format, p_using_auxiliary_animation));

					Array dispatches;
					for (int64_t dispatch_offset_idx = 0; dispatch_offset_idx < state_change.number_dispatches; dispatch_offset_idx++) {
						ERR_FAIL_INDEX_V(dispatch_index + dispatch_offset_idx, p_types.animation_dispatches.size(), nullptr);
						TRAnimationDispatch dispatch = p_types.animation_dispatches.get(dispatch_index + dispatch_offset_idx);
						if (dispatch.target_animation_number >= p_moveable_info.animation_index
							&& dispatch.target_animation_number < p_moveable_info.animation_index + p_moveable_info.animation_count) {
							TRAnimation new_tr_animation = p_types.animations.get(dispatch.target_animation_number);

							Dictionary dispatch_dict;

							int32_t start_frame = dispatch.start_frame;
							int32_t end_frame = dispatch.end_frame;

							real_t start_time = real_t(start_frame - tr_animation.frame_base) / real_t(TR_FPS);
							real_t end_time = real_t(end_frame - tr_animation.frame_base) / real_t(TR_FPS);
							real_t target_frame_time = real_t(dispatch.target_frame_number - new_tr_animation.frame_base) / real_t(TR_FPS);

							dispatch_dict.set("start_frame", dispatch.start_frame - tr_animation.frame_base);
							dispatch_dict.set("end_frame", dispatch.end_frame - tr_animation.frame_base);

							dispatch_dict.set("start_time", start_time);
							dispatch_dict.set("end_time", end_time);

							target_animation_number = dispatch.target_animation_number - p_moveable_info.animation_index;
							String target_animation_name = get_animation_name(p_type_info_id, target_animation_number, p_level_format, p_using_auxiliary_animation);

							dispatch_dict.set("target_animation_id", target_animation_number);
							dispatch_dict.set("target_animation_name", target_animation_name);
							dispatch_dict.set("target_frame_number", dispatch.target_frame_number - new_tr_animation.frame_base);
							dispatch_dict.set("target_frame_time", target_frame_time);

							target_animation = p_moveable_info.animations.get(target_animation_number);
							if (dispatch.target_frame_number != target_animation.frame_base) {
								if (!animation_split_table.has(target_animation_number)) {
									Vector<uint32_t> split_array;
									animation_split_table.insert(target_animation_number, split_array);
								}
								if (!animation_split_table.get(target_animation_number).has(dispatch.target_frame_number - new_tr_animation.frame_base)) {
									animation_split_table.get(target_animation_number).append(dispatch.target_frame_number - new_tr_animation.frame_base);
								}
							}

							dispatches.append(dispatch_dict);

#if 0
							godot_animation->add_marker(vformat("frame_%d", dispatch.start_frame - tr_animation.frame_base), start_time);
							godot_animation->add_marker(vformat("frame_%d", dispatch.end_frame - tr_animation.frame_base), end_time);
#endif
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

				String next_animation_name = get_animation_name(p_type_info_id, tr_animation.next_animation_number - p_moveable_info.animation_index, p_level_format, p_using_auxiliary_animation);

				godot_animation->set_meta("tr_animation_state_changes", animation_state_changes);
				godot_animation->set_meta("tr_animation_current_animation_state_id", tr_animation.current_animation_state);
				godot_animation->set_meta("tr_animation_current_animation_state_name", get_state_name(p_type_info_id, tr_animation.current_animation_state, p_level_format, p_using_auxiliary_animation));

				godot_animation->set_meta("tr_animation_frame_skip", tr_animation.frame_skip);
				
				godot_animation->set_meta("tr_animation_next_animation_id", tr_animation.next_animation_number - p_moveable_info.animation_index);
				godot_animation->set_meta("tr_animation_next_animation_name", get_animation_name(p_type_info_id, tr_animation.next_animation_number - p_moveable_info.animation_index, p_level_format, p_using_auxiliary_animation));
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

				animation_position_offsets.append(position_offset);
			}

			// Add nodes to the state machine.
			size_t grid_size = size_t(Math::floor(Math::sqrt(real_t(p_moveable_info.animations.size()))));
			for (int64_t anim_idx = 0; anim_idx < p_moveable_info.animations.size(); anim_idx++) {
				String animation_name = get_animation_name(p_type_info_id, anim_idx, p_level_format, p_using_auxiliary_animation);

				// If we have a looping variation of the animation, add that.
				Ref<AnimationNodeAnimation> loop_animation_node = nullptr;
				if (animation_loop_offset_table.has(anim_idx)) {
					real_t loop_offset = real_t(animation_loop_offset_table.get(anim_idx)) / TR_FPS;

					String loop_animation_name = animation_name + LOOPING_ANIMATION_SUFFIX;
					loop_animation_node = memnew(AnimationNodeAnimation);
					loop_animation_node->set_animation(animation_name);

					loop_animation_node->set_use_custom_timeline(true);
					
					Ref<Animation> godot_animiation = godot_animations.get(anim_idx);

					loop_animation_node->set_timeline_length(godot_animations.get(anim_idx)->get_length() - loop_offset);
					if (loop_animation_node->get_timeline_length() == 0.0) {
						loop_animation_node->set_stretch_time_scale(true);
					} else {
						loop_animation_node->set_stretch_time_scale(false);
					}
					loop_animation_node->set_start_offset(loop_offset);
					loop_animation_node->set_loop_mode(Animation::LOOP_LINEAR);

					state_machine->add_node(
						loop_animation_name,
						loop_animation_node,
						get_position_for_node(
							p_type_info_id,
							p_level_format,
							loop_animation_name,
							animation_name,
							anim_idx,
							grid_size));
				}

				Ref<AnimationNodeAnimation> animation_node = memnew(AnimationNodeAnimation);
				animation_node->set_animation(animation_name);

				state_machine->add_node(
					animation_name,
					animation_node,
					get_position_for_node(
						p_type_info_id,
						p_level_format,
						animation_name,
						"",
						anim_idx,
						grid_size));

				TRAnimation tr_animation = p_moveable_info.animations.get(anim_idx);
				animation_node->set_meta("tr_animation_state_name", get_state_name(p_type_info_id, tr_animation.current_animation_state, p_level_format, p_using_auxiliary_animation));
				animation_node->set_meta("tr_animation_state_id", tr_animation.current_animation_state);
			}

			// Now wire up the transitions.
			for (int64_t anim_idx = 0; anim_idx < p_moveable_info.animations.size(); anim_idx++) {
				TRAnimation tr_animation = p_moveable_info.animations.get(anim_idx);

				int32_t animation_length = tr_animation.frame_end - tr_animation.frame_base;

				String animation_name = get_animation_name(p_type_info_id, anim_idx, p_level_format, p_using_auxiliary_animation);
				String next_animation_name = get_animation_name(p_type_info_id, tr_animation.next_animation_number - p_moveable_info.animation_index, p_level_format, p_using_auxiliary_animation);

				int32_t target_animation_number = tr_animation.next_animation_number - p_moveable_info.animation_index;
				TRAnimation target_animation = tr_animation;
				if (target_animation_number < p_moveable_info.animations.size() && target_animation_number >= 0) {
					target_animation = p_moveable_info.animations.get(target_animation_number);
				}

				if (animation_name != next_animation_name) {
					Ref<AnimationNodeStateMachineTransition> transition = memnew(AnimationNodeStateMachineTransition);
					transition->set_switch_mode(AnimationNodeStateMachineTransition::SWITCH_MODE_AT_END);
					transition->set_xfade_time(0.0);
					transition->set_advance_mode(AnimationNodeStateMachineTransition::ADVANCE_MODE_AUTO);
					if (animation_name != next_animation_name) {
						if (!state_machine->has_transition(animation_name, next_animation_name)) {
							if (state_machine->has_node(animation_name) && state_machine->has_node(next_animation_name)) {
								state_machine->add_transition(animation_name, next_animation_name, transition);
							}
						}
					}
					// Experimental
					int32_t target_animation_frame_length = (target_animation.frame_end - target_animation.frame_base) + 1;
					if (target_animation_frame_length > 0) {
						real_t target_frame_time = real_t(tr_animation.next_frame_number - target_animation.frame_base) / TR_FPS;
						real_t target_length = real_t(target_animation_frame_length) / TR_FPS;
						real_t offset = target_frame_time / target_length;
						transition->set("transition_offset", offset);
					}
				} else {
					if (animation_loop_offset_table.has(anim_idx)) {
						Ref<AnimationNodeStateMachineTransition> transition = memnew(AnimationNodeStateMachineTransition);
						transition->set_switch_mode(AnimationNodeStateMachineTransition::SWITCH_MODE_AT_END);
						transition->set_xfade_time(0.0);
						transition->set_advance_mode(AnimationNodeStateMachineTransition::ADVANCE_MODE_AUTO);

						if (state_machine->has_node(animation_name) && state_machine->has_node(next_animation_name + LOOPING_ANIMATION_SUFFIX)) {
							state_machine->add_transition(animation_name, next_animation_name + LOOPING_ANIMATION_SUFFIX, transition);
						}
					}
				}

				// Set up default start animation (only valid for Lara)
				if (state_machine->has_node("Start") && state_machine->has_node("stand_idle")) {
					Ref<AnimationNodeStateMachineTransition> transition = memnew(AnimationNodeStateMachineTransition);
					transition->set_switch_mode(AnimationNodeStateMachineTransition::SWITCH_MODE_IMMEDIATE);
					transition->set_xfade_time(0.0);
					transition->set_advance_mode(AnimationNodeStateMachineTransition::ADVANCE_MODE_AUTO);
					//state_machine->add_transition("Start", "stand_idle", transition);
				}

				int16_t state_change_index = tr_animation.state_change_index;
				for (int64_t state_change_offset_idx = 0; state_change_offset_idx < tr_animation.number_state_changes; state_change_offset_idx++) {
					ERR_FAIL_INDEX_V(state_change_index + state_change_offset_idx, p_types.animation_state_changes.size(), nullptr);

					TRAnimationStateChange state_change = p_types.animation_state_changes.get(state_change_index + state_change_offset_idx);

					int16_t target_animation_state = state_change.target_animation_state;
					int16_t dispatch_index = state_change.dispatch_index;

					Array dispatches;
					for (int64_t dispatch_offset_idx = 0; dispatch_offset_idx < state_change.number_dispatches; dispatch_offset_idx++) {
						ERR_FAIL_INDEX_V(dispatch_index + dispatch_offset_idx, p_types.animation_dispatches.size(), nullptr);
						TRAnimationDispatch dispatch = p_types.animation_dispatches.get(dispatch_index + dispatch_offset_idx);
						if (dispatch.target_animation_number >= p_moveable_info.animation_index && dispatch.target_animation_number < p_moveable_info.animation_index + p_moveable_info.animation_count) {
							target_animation_number = dispatch.target_animation_number - p_moveable_info.animation_index;
							target_animation = p_types.animations.get(target_animation_number);

							int32_t start_frame = dispatch.start_frame - tr_animation.frame_base;
							int32_t end_frame = dispatch.end_frame - tr_animation.frame_base;
							int32_t target_frame = dispatch.target_frame_number - target_animation.frame_base;

							String target_animation_name = get_animation_name(p_type_info_id, target_animation_number, p_level_format, p_using_auxiliary_animation);

							if (animation_name == target_animation_name) {
								printf("Transitioning into self.\n");
								continue;
							}

							int32_t target_animation_length = (target_animation.frame_end - target_animation.frame_base);

							{
								target_animation = p_moveable_info.animations.get(target_animation_number);
								Ref<AnimationNodeStateMachineTransition> transition = create_animation_transition(
									state_machine,
									start_frame,
									end_frame,
									animation_length,
									target_frame,
									target_animation_length,
									state_change.target_animation_state);

								if (!state_machine->has_transition(animation_name, target_animation_name)) {
									if (state_machine->has_node(animation_name) && state_machine->has_node(target_animation_name)) {
										state_machine->add_transition(animation_name, target_animation_name, transition);
									} else {
										printf("Missing node.\n");
									}
								} else {
									String transition_animation_name = animation_name + "_to_" + target_animation_name;
									Ref<AnimationNodeAnimation> transition_animation_node = memnew(AnimationNodeAnimation);
									transition_animation_node->set_animation(transition_animation_name);

									state_machine->add_node(
										transition_animation_name,
										transition_animation_node,
										get_position_for_node(
											p_type_info_id,
											p_level_format,
											transition_animation_name,
											animation_name,
											anim_idx,
											grid_size));

									Ref<AnimationNodeStateMachineTransition> final_transition = transition->duplicate();

									if (transition->has_method("set_transition_offset")) {
										transition->call("set_transition_offset", 0.0);
									}
									transition->set_reset(false);
									transition->set_switch_mode(AnimationNodeStateMachineTransition::SWITCH_MODE_SYNC);
									state_machine->add_transition(animation_name, transition_animation_name, transition);

									final_transition->set_advance_expression("");
									state_machine->add_transition(transition_animation_name, target_animation_name, final_transition);

								}
							}

							if (animation_loop_offset_table.has(anim_idx)) {
								int32_t offset = animation_loop_offset_table.get(anim_idx);

								Ref<AnimationNodeStateMachineTransition> transition = create_animation_transition(
									state_machine,
									start_frame - offset,
									end_frame - offset,
									animation_length - offset,
									target_frame,
									target_animation_length,
									state_change.target_animation_state);

								if (!state_machine->has_transition(animation_name + LOOPING_ANIMATION_SUFFIX, target_animation_name)) {
									if (state_machine->has_node(animation_name + LOOPING_ANIMATION_SUFFIX) && state_machine->has_node(target_animation_name)) {
										state_machine->add_transition(animation_name + LOOPING_ANIMATION_SUFFIX, target_animation_name, transition);
									} else {
										printf("Missing node.\n");
									}
								} else {
									String transition_animation_name = animation_name + LOOPING_ANIMATION_SUFFIX + "_to_" + target_animation_name;
									Ref<AnimationNodeAnimation> transition_animation_node = memnew(AnimationNodeAnimation);
									transition_animation_node->set_animation(transition_animation_name);

									state_machine->add_node(
										transition_animation_name,
										transition_animation_node,
										get_position_for_node(
											p_type_info_id,
											p_level_format,
											transition_animation_name,
											animation_name,
											anim_idx,
											grid_size));

									Ref<AnimationNodeStateMachineTransition> final_transition = transition->duplicate();

									if (transition->has_method("set_transition_offset")) {
										transition->call("set_transition_offset", 0.0);
									}
									transition->set_reset(false);
									transition->set_switch_mode(AnimationNodeStateMachineTransition::SWITCH_MODE_SYNC);
									state_machine->add_transition(animation_name + LOOPING_ANIMATION_SUFFIX, transition_animation_name, transition);

									final_transition->set_advance_expression("");
									state_machine->add_transition(transition_animation_name, target_animation_name, final_transition);
								}
							}
						} else {
							ERR_PRINT("Target animation out of range.");
						}
					}
				}
			}
			
			animation_tree->set_root_animation_node(get_animation_tree_root_node_for_object(p_type_info_id, p_level_format, state_machine));

			animation_player->add_animation_library("", animation_library);
			animation_player->set_assigned_animation("RESET");

			animation_player->play("RESET");

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
							transform_offset = Transform3D().rotated(Vector3(0.0, 1.0, 0.0), Math::PI);
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
							TRTransform reference_bone_transform;
							if (reference_tr_animation.frames.size() > 0) {
								TRAnimFrame reference_anim_frame = reference_tr_animation.frames.get(0);
								if (reference_anim_frame.transforms.size() > 0) {
									reference_bone_transform = reference_anim_frame.transforms.get(mesh_idx);
								}
							}

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
									reset_final_transform = Transform3D().rotated(Vector3(0.0, 1.0, 0.0), Math::PI) * reset_final_transform;
								}

								if (skeleton && skeleton->get_bone_count() > bone_idx) {
									skeleton->set_bone_rest(bone_idx, Transform3D(Basis(), Vector3(0.0, motion_scale, 0.0)));
								}
								//reset_final_transform.origin.y *= motion_scale;
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

						String animation_name = get_animation_name(p_type_info_id, anim_idx, p_level_format, p_using_auxiliary_animation);

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
										Vector3 calculated_position = Vector3((-double(end_pos_x) * TR_TO_GODOT_SCALE) / motion_scale, 0.0, (-double(end_pos_z) * TR_TO_GODOT_SCALE) / motion_scale);
										real_t calculated_time = (real_t)(frame_idx / TR_FPS) * tr_animation.frame_skip;
										godot_animation->position_track_insert_key(root_position_track_idx, calculated_time, calculated_position);
									}

									for (; frame_skips < tr_animation.frame_skip; frame_skips++) {
										end_pos_x -= (tr_animation.lateral_velocity + (tr_animation.lateral_acceleration * frame_counter)) >> 16;
										end_pos_z -= (tr_animation.velocity + (tr_animation.acceleration * frame_counter)) >> 16;

										if (tr_animation.acceleration != 0 || tr_animation.lateral_acceleration != 0 || tr_animation.velocity != 0 || tr_animation.lateral_velocity != 0) {
											Vector3 calculated_position = Vector3((-double(end_pos_x) * TR_TO_GODOT_SCALE) / motion_scale, 0.0, (-double(end_pos_z) * TR_TO_GODOT_SCALE) / motion_scale);
											real_t calculated_time = (1.0 / TR_FPS) * frame_counter;
											godot_animation->position_track_insert_key(root_position_track_idx, calculated_time, calculated_position);
										}
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
										TRInterpolatedFrame interpolated_frame = get_final_frame_for_animation(p_moveable_info.animation_index + anim_idx, p_types.animations);

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

								real_t key_insertion_time = (real_t)(frame_idx / TR_FPS) * tr_animation.frame_skip;
								if (key_insertion_time > godot_animation->get_length()) {
									key_insertion_time = godot_animation->get_length();
								}

								godot_animation->position_track_insert_key(shape_position_track_idx, key_insertion_time, gd_bbox_position);
								godot_animation->track_insert_key(shape_size_track_idx, key_insertion_time, gd_bbox_scale);

								Vector3 center_grip_point = Vector3(0.0, -gd_bbox_min.y, gd_bbox_max.z) / motion_scale;
								Vector3 camera_target = Vector3(0.0, gd_bbox_position.y + (256 * TR_TO_GODOT_SCALE), 0.0);

								godot_animation->position_track_insert_key(grip_center_position_track_idx, key_insertion_time, center_grip_point);
								godot_animation->position_track_insert_key(camera_target_track_idx, key_insertion_time, camera_target);
							}

							int32_t target_animation_number = tr_animation.next_animation_number - p_moveable_info.animation_index;
							TRAnimation tr_next_animation = tr_animation;
							if (target_animation_number < p_moveable_info.animations.size() && target_animation_number >= 0) {
								tr_next_animation = p_types.animations.get(target_animation_number);
							}

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
								if (!(next_animation_number < p_moveable_info.animations.size() && next_animation_number >= 0)) {
									next_animation_number = anim_idx;
								}

								if (godot_animation->get_loop_mode() == Animation::LOOP_LINEAR) {
									ERR_FAIL_INDEX_V(0, tr_animation.frames.size(), nullptr);
									first_anim_frame = second_anim_frame = tr_animation.frames[0];
								} else if (godot_animation->get_loop_mode() == Animation::LOOP_NONE) {
									TRInterpolatedFrame interpolated_frame = get_final_frame_for_animation(p_moveable_info.animation_index + anim_idx, p_types.animations);
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

									int32_t target_animation_number = next_animation_number;
									TRAnimation tr_next_animation = p_moveable_info.animations.get(target_animation_number);
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
								pre_fix = pre_fix.rotated(Vector3(0.0, 1.0, 0.0), Math::PI);
							}

							Transform3D final_transform = pre_fix * first_transform.interpolate_with(second_transform, interpolation);
							final_transform.origin = ((transform_offset.origin + final_transform.origin) * Vector3(1.0, 1.0 / motion_scale, 1.0));

							final_transform = bone_pre_transforms.get(bone_idx) * final_transform * bone_post_transforms.get(bone_idx);

							real_t frame_insertion_time = (real_t)(frame_idx / TR_FPS) * tr_animation.frame_skip;
							if (frame_insertion_time > godot_animation->get_length()) {
								frame_insertion_time = godot_animation->get_length();
								printf("Tried to write keyframes outside of animation's length.\n");
							}

							if (position_track_idx >= 0) {
								godot_animation->position_track_insert_key(position_track_idx, frame_insertion_time, final_transform.origin);
							}
							if (rotation_track_idx >= 0) {
								godot_animation->rotation_track_insert_key(rotation_track_idx, frame_insertion_time, final_transform.basis.get_quaternion());
							}
						}
					}

					// Create loop offset animations
#if 0
					for (int64_t anim_idx = 0; anim_idx < p_moveable_info.animations.size(); anim_idx++) {
						Ref<Animation> godot_animation = godot_animations.get(anim_idx);
						TRAnimation tr_animation = p_moveable_info.animations.get(anim_idx);

						int32_t frame_length = (tr_animation.frame_end - tr_animation.frame_base) + 1;

						if (animation_loop_offset_table.has(anim_idx)) {
							int32_t frame_start = animation_loop_offset_table.get(anim_idx);
							int32_t frame_end = frame_length;

							Ref<Animation> loop_animation = create_slice(godot_animation, frame_start, frame_end);
							if (loop_animation.is_valid()) {
								String loop_animation_name = get_animation_name(p_type_info_id, anim_idx, p_level_format, p_using_auxiliary_animation) + LOOPING_ANIMATION_SUFFIX;

								loop_animation->set_name(loop_animation_name);
								loop_animation->set_loop_mode(Animation::LOOP_LINEAR);

								animation_library->add_animation(loop_animation_name, loop_animation);
							}
						}
					}

					// Create split animations
					for (int64_t anim_idx = 0; anim_idx < p_moveable_info.animations.size(); anim_idx++) {
						Ref<Animation> godot_animation = godot_animations.get(anim_idx);
						TRAnimation tr_animation = p_moveable_info.animations.get(anim_idx);

						int32_t frame_length = (tr_animation.frame_end - tr_animation.frame_base) + 1;

						Vector<uint32_t> animation_splits = animation_split_table.get(anim_idx);
						animation_splits.sort();

						if (animation_splits.size() > 1) {
							for (int64_t split_idx = 0; split_idx < animation_splits.size(); split_idx++) {
								int32_t frame_start = animation_splits[split_idx];
								int32_t frame_end = frame_length;
								if (split_idx < animation_splits.size() - 1) {
									frame_end = animation_splits[split_idx + 1];
								}

								if (frame_start == frame_end) {
									continue;
								}

								Ref<Animation> split_animation = create_slice(godot_animation, frame_start, frame_end);
								if (split_animation.is_valid()) {
									String split_animation_name = get_animation_name(p_type_info_id, anim_idx, p_level_format, p_using_auxiliary_animation) + "_" + itos(frame_start) + "_" + itos(frame_end);

									split_animation->set_name(split_animation_name);

									animation_library->add_animation(split_animation_name, split_animation);
								}
							}
						}
					}
#endif

					BoneAttachment3D *bone_attachment = memnew(BoneAttachment3D);
					bone_attachment->set_name(get_bone_name(p_type_info_id, mesh_idx, p_level_format) + "_attachment");
					skeleton->add_child(bone_attachment);
					bone_attachment->set_bone_idx(mesh_to_bone_mapping[mesh_idx]);

					skeleton->clear_bones_global_pose_override();
					skeleton->reset_bone_poses();
					skeleton->clear_gizmos();
					
					if (!p_use_skinning) {
						Ref<ArrayMesh> mesh = p_meshes.get(offset_mesh_index);
						MeshInstance3D* mi = memnew(MeshInstance3D);
						bone_attachment->add_child(mi);

						mi->set_mesh(mesh);
						mi->set_name(String("MeshInstance_") + itos(offset_mesh_index));
						mi->set_layer_mask(1 << 1); // Dynamic
						mi->set_gi_mode(GeometryInstance3D::GI_MODE_DYNAMIC);

						if (does_overwrite_mesh_rotation(p_type_info_id, p_level_format)) {
							mi->set_transform(get_overwritten_mesh_rotation(p_type_info_id, mesh_idx, p_level_format).inverse() * Transform3D().rotated(Vector3(0.0, 1.0, 0.0), Math::PI).basis);
						}

						mesh_instances.append(mi);
					}
				}
			}

			// Now create the skinned mesh...
			if (p_use_skinning) {
				BitField<Mesh::ArrayFormat> combined_mesh_flags = Mesh::ARRAY_FORMAT_VERTEX
					| Mesh::ARRAY_FORMAT_BONES
					| Mesh::ARRAY_FORMAT_WEIGHTS
					| Mesh::ARRAY_FORMAT_INDEX;

				for (int64_t mesh_idx = 0; mesh_idx < p_moveable_info.mesh_count; mesh_idx++) {
					Ref<ArrayMesh> current_mesh = p_meshes.get(mesh_idx);
					int32_t surface_count = current_mesh->get_surface_count();
					for (int64_t surface_idx = 0; surface_idx < surface_count; surface_idx++) {
						BitField<Mesh::ArrayFormat> current_mesh_flags_mesh_flags = current_mesh->surface_get_format(surface_idx);
						combined_mesh_flags = (int64_t(combined_mesh_flags) | int64_t(current_mesh_flags_mesh_flags));
					}
				}


				Vector<Ref<Material>> used_materials;
				HashMap<Ref<Material>, Array> material_mesh_map;
				Ref<ArrayMesh> combined_mesh = memnew(ArrayMesh);
				for (int64_t mesh_idx = 0; mesh_idx < p_moveable_info.mesh_count; mesh_idx++) {
					int32_t bone_idx = mesh_to_bone_mapping[mesh_idx];

					Ref<ArrayMesh> current_mesh = p_meshes.get(mesh_idx);
					int32_t surface_count = current_mesh->get_surface_count();
					for (int64_t surface_idx = 0; surface_idx < surface_count; surface_idx++) {
						Ref<Material> material = current_mesh->surface_get_material(surface_idx);
						if (!material_mesh_map.has(material)) {
							used_materials.append(material);

							material_mesh_map[material] = Array();
							material_mesh_map[material].resize(Mesh::ArrayType::ARRAY_MAX);

							material_mesh_map[material][Mesh::ArrayType::ARRAY_VERTEX] = PackedVector3Array();
							
							if (combined_mesh_flags & Mesh::ARRAY_FORMAT_NORMAL) {
								material_mesh_map[material][Mesh::ArrayType::ARRAY_NORMAL] = PackedVector3Array();
							}
							if (combined_mesh_flags & Mesh::ARRAY_FORMAT_COLOR) {
								material_mesh_map[material][Mesh::ArrayType::ARRAY_COLOR] = PackedColorArray();
							}
							if (combined_mesh_flags & Mesh::ARRAY_FORMAT_TEX_UV) {
								material_mesh_map[material][Mesh::ArrayType::ARRAY_TEX_UV] = PackedVector2Array();
							}
							
							material_mesh_map[material][Mesh::ArrayType::ARRAY_BONES] = PackedInt32Array();
							material_mesh_map[material][Mesh::ArrayType::ARRAY_WEIGHTS] = PackedFloat32Array();

							if (combined_mesh_flags & Mesh::ARRAY_FORMAT_INDEX) {
								material_mesh_map[material][Mesh::ArrayType::ARRAY_INDEX] = PackedInt32Array();
							}
						}

						Transform3D pose = skeleton->get_bone_global_pose(bone_idx);// .rotated_local(Vector3(0.0, 1.0, 0.0), Math_PI);

						if (does_overwrite_mesh_rotation(p_type_info_id, p_level_format)) {
							pose.basis *= (get_overwritten_mesh_rotation(p_type_info_id, mesh_idx, p_level_format).inverse() * Transform3D().rotated(Vector3(0.0, 1.0, 0.0), Math::PI).basis);
						}

						Array merged_surface_arrays = material_mesh_map[material];

						Array new_surface_arrays = current_mesh->surface_get_arrays(surface_idx);
						PackedVector3Array merged_vertex_array = merged_surface_arrays[Mesh::ArrayType::ARRAY_VERTEX];
						PackedVector3Array new_vertex_array = new_surface_arrays[Mesh::ArrayType::ARRAY_VERTEX];
						int32_t original_vertex_count = merged_vertex_array.size();
						for (int32_t i = 0; i < new_vertex_array.size(); i++) {
							new_vertex_array.set(i, pose.xform(new_vertex_array.get(i)));
						}
						merged_vertex_array.append_array(new_vertex_array);
						material_mesh_map[material][Mesh::ArrayType::ARRAY_VERTEX] = merged_vertex_array;

						if (combined_mesh_flags & Mesh::ARRAY_FORMAT_NORMAL) {
							PackedVector3Array merged_normal_array = merged_surface_arrays[Mesh::ArrayType::ARRAY_NORMAL];
							PackedVector3Array new_normal_array = new_surface_arrays[Mesh::ArrayType::ARRAY_NORMAL];
							for (int32_t i = 0; i < new_normal_array.size(); i++) {
								new_normal_array.set(i, pose.get_basis().xform(new_normal_array.get(i)));
							}
							merged_normal_array.append_array(new_normal_array);
							material_mesh_map[material][Mesh::ArrayType::ARRAY_NORMAL] = merged_normal_array;
						}

						if (combined_mesh_flags & Mesh::ARRAY_FORMAT_COLOR) {
							PackedColorArray merged_color_array = merged_surface_arrays[Mesh::ArrayType::ARRAY_COLOR];
							PackedColorArray new_color_array = new_surface_arrays[Mesh::ArrayType::ARRAY_COLOR];
							if (new_color_array.size() != current_mesh->surface_get_array_len(surface_idx)) {
								new_color_array.resize(current_mesh->surface_get_array_len(surface_idx));
								for (int32_t i = 0; i < current_mesh->surface_get_array_len(surface_idx); i++) {
									new_color_array.set(i, Color(1.0, 1.0, 1.0, 1.0));
								}
							}
							merged_color_array.append_array(new_color_array);
							material_mesh_map[material][Mesh::ArrayType::ARRAY_COLOR] = merged_color_array;
						}

						if (combined_mesh_flags & Mesh::ARRAY_FORMAT_TEX_UV) {
							PackedVector2Array merged_uv_array = merged_surface_arrays[Mesh::ArrayType::ARRAY_TEX_UV];
							PackedVector2Array new_uv_array = new_surface_arrays[Mesh::ArrayType::ARRAY_TEX_UV];
							if (new_uv_array.size() != current_mesh->surface_get_array_len(surface_idx)) {
								new_uv_array.resize(current_mesh->surface_get_array_len(surface_idx));
								for (int32_t i = 0; i < current_mesh->surface_get_array_len(surface_idx); i++) {
									new_uv_array.set(i, Vector2(0.0, 0.0));
								}
							}
							merged_uv_array.append_array(new_uv_array);
							material_mesh_map[material][Mesh::ArrayType::ARRAY_TEX_UV] = merged_uv_array;
						}

						//
						PackedInt32Array merged_bone_array = merged_surface_arrays[Mesh::ArrayType::ARRAY_BONES];
						PackedInt32Array new_bone_array = new_surface_arrays[Mesh::ArrayType::ARRAY_BONES];
						new_bone_array.resize(current_mesh->surface_get_array_len(surface_idx) * 4);
						for (int32_t i = 0; i < current_mesh->surface_get_array_len(surface_idx); i++) {
							int32_t idx = i * 4;
							new_bone_array.set(idx + 0, bone_idx);
							new_bone_array.set(idx + 1, bone_idx);
							new_bone_array.set(idx + 2, bone_idx);
							new_bone_array.set(idx + 3, bone_idx);
						}
						merged_bone_array.append_array(new_bone_array);
						material_mesh_map[material][Mesh::ArrayType::ARRAY_BONES] = merged_bone_array;

						//
						PackedFloat32Array merged_weight_array = merged_surface_arrays[Mesh::ArrayType::ARRAY_WEIGHTS];
						PackedFloat32Array new_weight_array = new_surface_arrays[Mesh::ArrayType::ARRAY_WEIGHTS];
						new_weight_array.resize(current_mesh->surface_get_array_len(surface_idx) * 4);
						for (int32_t i = 0; i < current_mesh->surface_get_array_len(surface_idx); i++) {
							int32_t idx = i * 4;
							new_weight_array.set(idx + 0, 1.0f);
							new_weight_array.set(idx + 1, 0.0f);
							new_weight_array.set(idx + 2, 0.0f);
							new_weight_array.set(idx + 3, 0.0f);
						}
						merged_weight_array.append_array(new_weight_array);
						material_mesh_map[material][Mesh::ArrayType::ARRAY_WEIGHTS] = merged_weight_array;

						if (combined_mesh_flags & Mesh::ARRAY_FORMAT_INDEX) {
							PackedInt32Array merged_index_array = merged_surface_arrays[Mesh::ArrayType::ARRAY_INDEX];
							PackedInt32Array new_index_array = new_surface_arrays[Mesh::ArrayType::ARRAY_INDEX];
							if (new_index_array.is_empty()) {
								new_index_array.resize(current_mesh->surface_get_array_len(surface_idx));
								for (int32_t i = 0; i < current_mesh->surface_get_array_len(surface_idx); i++) {
									new_index_array.set(i, original_vertex_count + i);
								}
							} else {
								for (int32_t i = 0; i < new_index_array.size(); i++) {
									new_index_array.set(i, original_vertex_count + new_index_array.get(i));
								}
							}
							merged_index_array.append_array(new_index_array);
							material_mesh_map[material][Mesh::ArrayType::ARRAY_INDEX] = merged_index_array;
						}
					}
				}

				for (Ref<Material>& material : used_materials) {
					combined_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, material_mesh_map[material], Array(), Dictionary(), combined_mesh_flags);
					combined_mesh->surface_set_material(combined_mesh->get_surface_count() - 1, material);
				}

				MeshInstance3D* combined_mesh_instance = memnew(MeshInstance3D);
				combined_mesh_instance->set_name("MeshInstance3D");
				combined_mesh_instance->set_mesh(combined_mesh);

				combined_mesh_instance->set_layer_mask(1 << 1); // Dynamic
				combined_mesh_instance->set_gi_mode(GeometryInstance3D::GI_MODE_DYNAMIC);

				skeleton->add_child(combined_mesh_instance);

				skeleton->clear_bones_global_pose_override();
				skeleton->reset_bone_poses();
				skeleton->update_gizmos();
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
	HashMap<int32_t, TRMoveableInfo> p_type_info_map,
	TRTypes p_types,
	Vector<Ref<ArrayMesh>> p_meshes,
	Vector<Ref<AudioStream>> p_samples,
	TRLevelFormat p_level_format,
	bool p_using_auxiliary_animation) {
	Vector<Node3D *> types;

	for (int32_t type_id = 0; type_id < 4096; type_id++) {
		if (p_type_info_map.has(type_id)) {
			Node3D *new_node = create_godot_moveable_model(type_id, p_type_info_map[type_id], p_types, p_meshes, p_samples, p_level_format, p_using_auxiliary_animation, false, false);
			if (new_node) {
				types.push_back(new_node);
			}
		}
	}

	return types;
}

bool ray_triangle_intersect_test(const Vector3& p_start, const Vector3& p_end, const Vector3& v0, const Vector3& v1, const Vector3& v2) {
	Vector3 hit_position = Vector3();
	if (Geometry3D::segment_intersects_triangle(p_start, p_end, v0, v1, v2, &hit_position)) {
		if (p_end.distance_to(hit_position) > 1.0) {
			return true;
		}
	}

	return false;
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


#define TEST_AGAINST_POTENTIAL_OCCLUDERS \
for (int32_t occluder_idx = 0; occluder_idx < p_room_data.room_quad_count; occluder_idx++) { \
	if (occluder_idx == cur_test_poly_idx) { \
		continue; \
	} \
	\
	TRFaceQuad occluder_face = p_room_data.room_quads[occluder_idx]; \
	\
	TRVertex v0 = room_verts[occluder_face.indices[0]].vertex; \
	TRVertex v1 = room_verts[occluder_face.indices[1]].vertex; \
	TRVertex v2 = room_verts[occluder_face.indices[2]].vertex; \
	TRVertex v3 = room_verts[occluder_face.indices[3]].vertex; \
	\
	Vector3 v0_pos = (Vector3(v0.x, -v0.y, -v0.z)) * TR_TO_GODOT_SCALE; \
	Vector3 v1_pos = (Vector3(v1.x, -v1.y, -v1.z)) * TR_TO_GODOT_SCALE; \
	Vector3 v2_pos = (Vector3(v2.x, -v2.y, -v2.z)) * TR_TO_GODOT_SCALE; \
	Vector3 v3_pos = (Vector3(v3.x, -v3.y, -v3.z)) * TR_TO_GODOT_SCALE; \
	\
	if (ray_triangle_intersect_test(portal_pos, test_pos, v0_pos, v1_pos, v2_pos)) { \
			ray_blocked = true; \
			break; \
	}\
	\
	if (ray_triangle_intersect_test(portal_pos, test_pos, v0_pos, v2_pos, v3_pos)) { \
		ray_blocked = true; \
		break; \
	}\
}\
\
for (int32_t occluder_idx = 0; occluder_idx < p_room_data.room_triangle_count; occluder_idx++) {\
	if (occluder_idx == cur_test_poly_idx) { \
		continue; \
	}\
	\
	TRFaceTriangle occluder_face = p_room_data.room_triangles[occluder_idx]; \
	\
	TRVertex v0 = room_verts[occluder_face.indices[0]].vertex; \
	TRVertex v1 = room_verts[occluder_face.indices[1]].vertex; \
	TRVertex v2 = room_verts[occluder_face.indices[2]].vertex; \
	\
	Vector3 v0_pos = (Vector3(v0.x, -v0.y, -v0.z)) * TR_TO_GODOT_SCALE; \
	Vector3 v1_pos = (Vector3(v1.x, -v1.y, -v1.z)) * TR_TO_GODOT_SCALE; \
	Vector3 v2_pos = (Vector3(v2.x, -v2.y, -v2.z)) * TR_TO_GODOT_SCALE; \
	\
	if (ray_triangle_intersect_test(portal_pos, test_pos, v0_pos, v1_pos, v2_pos)) { \
			ray_blocked = true; \
			break; \
	}\
}\

struct TRGodotMaterials {
	Vector<Ref<Material>> level_solid_materials;
	Vector<Ref<Material>> level_transparent_materials;
	Vector<Ref<Material>> entity_solid_materials;
	Vector<Ref<Material>> entity_transparent_materials;

};

struct TRGodotMaterialTable {
	TRGodotMaterials materials;
	HashMap<int32_t, TRGodotMaterials> read_stencil_materials;
	HashMap<int32_t, Ref<Material>> portal_stencil_materials;
};

Ref<ArrayMesh> tr_room_data_to_godot_mesh(
	const TRRoomData &p_room_data,
	const TRGodotMaterialTable &p_material_table,
	const TRTypes& p_types,
	const Vector3 p_offset,
	const Vector<TRRoomPortal> p_visible_from_portals,
	const int32_t stencil_id = -1) {

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

	// If we're creating a dummy room, check each polygon against the visible from portals.
	Vector<uint32_t> skipped_quads = Vector<uint32_t>();
	Vector<uint32_t> skipped_triangles = Vector<uint32_t>();

	const real_t PORTAL_TEST_SCALE = 0.001;

	if (p_visible_from_portals.size() > 0) {
		// Loop through every quad polygon we want to test
		for (int32_t cur_test_poly_idx = 0; cur_test_poly_idx < p_room_data.room_quad_count; cur_test_poly_idx++) {
			bool polygon_is_visible = false;

			// For each portal, check if at least one vertex of our polygon is visible
			for (const TRRoomPortal& room_portal : p_visible_from_portals) {
				bool portal_can_see_polygon = false;

				// Check each vertex of our polygon against this portal
				for (int32_t test_vert_idx = 0; test_vert_idx < 4; test_vert_idx++) {
					TRVertex test_vertex = room_verts[p_room_data.room_quads[cur_test_poly_idx].indices[test_vert_idx]].vertex;
					Vector3 test_pos = (
						Vector3(
							test_vertex.x,
							-test_vertex.y,
							-test_vertex.z)
						) * TR_TO_GODOT_SCALE;

					bool vertex_visible_from_some_portal_vertex = false;
					Vector3 portal_vertex_0 = (Vector3(room_portal.vertices[0].x, -room_portal.vertices[0].y, -room_portal.vertices[0].z)) * TR_TO_GODOT_SCALE;
					Vector3 portal_vertex_1 = (Vector3(room_portal.vertices[1].x, -room_portal.vertices[1].y, -room_portal.vertices[1].z)) * TR_TO_GODOT_SCALE;
					Vector3 portal_vertex_2 = (Vector3(room_portal.vertices[2].x, -room_portal.vertices[2].y, -room_portal.vertices[2].z)) * TR_TO_GODOT_SCALE;
					Vector3 portal_vertex_3 = (Vector3(room_portal.vertices[3].x, -room_portal.vertices[3].y, -room_portal.vertices[3].z)) * TR_TO_GODOT_SCALE;

					Vector3 center_portal_pos = (portal_vertex_0 + portal_vertex_1 + portal_vertex_2 + portal_vertex_3) / 4.0;

					// Check from each portal vertex
					for (const TRPos& portal_vertex : room_portal.vertices) {
						Vector3 portal_pos = (Vector3(portal_vertex.x, -portal_vertex.y, -portal_vertex.z)) * TR_TO_GODOT_SCALE;
						portal_pos = portal_pos.lerp(center_portal_pos, PORTAL_TEST_SCALE);

						bool ray_blocked = false;

						TEST_AGAINST_POTENTIAL_OCCLUDERS

						if (!ray_blocked) {
							vertex_visible_from_some_portal_vertex = true;
							break;
						}
					}

					if (vertex_visible_from_some_portal_vertex) {
						portal_can_see_polygon = true;
						break;
					}
				}

				if (portal_can_see_polygon) {
					polygon_is_visible = true;
					break;
				}
			}

			if (!polygon_is_visible) {
				skipped_quads.append(cur_test_poly_idx);
			}
		}

		// Loop through every triangle polygon we want to test
		for (int32_t cur_test_poly_idx = 0; cur_test_poly_idx < p_room_data.room_triangle_count; cur_test_poly_idx++) {
			bool polygon_is_visible = false;

			// For each portal, check if at least one vertex of our polygon is visible
			for (const TRRoomPortal& room_portal : p_visible_from_portals) {
				bool portal_can_see_polygon = false;

				// Check each vertex of our polygon against this portal
				for (int32_t test_vert_idx = 0; test_vert_idx < 3; test_vert_idx++) {
					TRVertex test_vertex = room_verts[p_room_data.room_triangles[cur_test_poly_idx].indices[test_vert_idx]].vertex;
					Vector3 test_pos = (
						Vector3(
							test_vertex.x,
							-test_vertex.y,
							-test_vertex.z)
						) * TR_TO_GODOT_SCALE;

					bool vertex_visible_from_some_portal_vertex = false;
					Vector3 portal_vertex_0 = (Vector3(room_portal.vertices[0].x, -room_portal.vertices[0].y, -room_portal.vertices[0].z)) * TR_TO_GODOT_SCALE;
					Vector3 portal_vertex_1 = (Vector3(room_portal.vertices[1].x, -room_portal.vertices[1].y, -room_portal.vertices[1].z)) * TR_TO_GODOT_SCALE;
					Vector3 portal_vertex_2 = (Vector3(room_portal.vertices[2].x, -room_portal.vertices[2].y, -room_portal.vertices[2].z)) * TR_TO_GODOT_SCALE;
					Vector3 portal_vertex_3 = (Vector3(room_portal.vertices[3].x, -room_portal.vertices[3].y, -room_portal.vertices[3].z)) * TR_TO_GODOT_SCALE;

					Vector3 center_portal_pos = (portal_vertex_0 + portal_vertex_1 + portal_vertex_2 + portal_vertex_3) / 4.0;

					// Check from each portal vertex
					for (const TRPos& portal_vertex : room_portal.vertices) {
						Vector3 portal_pos = (Vector3(portal_vertex.x, -portal_vertex.y, -portal_vertex.z)) * TR_TO_GODOT_SCALE;
						portal_pos = portal_pos.lerp(center_portal_pos, PORTAL_TEST_SCALE);

						bool ray_blocked = false;

						TEST_AGAINST_POTENTIAL_OCCLUDERS

						if (!ray_blocked) {
							vertex_visible_from_some_portal_vertex = true;
							break;
						}
					}

					if (vertex_visible_from_some_portal_vertex) {
						portal_can_see_polygon = true;
						break;
					}
				}

				if (portal_can_see_polygon) {
					polygon_is_visible = true;
					break;
				}
			}

			if (!polygon_is_visible) {
				skipped_triangles.append(cur_test_poly_idx);
			}
		}
	}

	//
	for (int32_t i = 0; i < p_room_data.room_quad_count; i++) {
		if (skipped_quads.has(i)) {
			continue;
		}

		if (p_room_data.room_quads[i].tex_info_id < 0) {
			continue;
		}

		if (p_room_data.room_quads[i].tex_info_id >= p_types.texture_infos.size()) {
			continue;
		}

		TRTextureInfo texture_info = p_types.texture_infos.get(p_room_data.room_quads[i].tex_info_id);
		int32_t material_id = texture_info.texture_page;
		if (texture_info.draw_type == 1) {
			material_id += p_material_table.materials.level_solid_materials.size();
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
		if (skipped_triangles.has(i)) {
			continue;
		}

		if (p_room_data.room_triangles[i].tex_info_id >= p_types.texture_infos.size()) {
			continue;
		}

		TRTextureInfo texture_info = p_types.texture_infos.get(p_room_data.room_triangles[i].tex_info_id);
		int32_t material_id = texture_info.texture_page;
		if (texture_info.draw_type == 1) {
			material_id += p_material_table.materials.level_solid_materials.size();
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

	Vector<Ref<Material>> all_materials;
	if (p_material_table.read_stencil_materials.has(stencil_id)) {
		all_materials = p_material_table.read_stencil_materials.get(stencil_id).level_solid_materials;
		all_materials.append_array(p_material_table.read_stencil_materials.get(stencil_id).level_transparent_materials);
	} else {
		all_materials = p_material_table.materials.level_solid_materials;
		all_materials.append_array(p_material_table.materials.level_transparent_materials);
	}

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
				room_vertex.vertex.z * -TR_TO_GODOT_SCALE) + p_offset;

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

	// Debugging
#if 1
	Ref<StandardMaterial3D> portal_debug_material = memnew(StandardMaterial3D);
	portal_debug_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	portal_debug_material->set_albedo(Color(1.0, 0.0, 1.0, 0.5));
	portal_debug_material->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
	portal_debug_material->set_emission(Color(1.0, 0.0, 1.0, 1.0));

	if (p_visible_from_portals.size() > 0) {
		st->begin(Mesh::PRIMITIVE_TRIANGLES);
		
		uint32_t index_offset = 0;
		for (const TRRoomPortal& room_portal : p_visible_from_portals) {
			for (int32_t idx = 0; idx < 4; idx++) {
				Vector3 portal_vertex = Vector3(
					room_portal.vertices[idx].x * TR_TO_GODOT_SCALE,
					room_portal.vertices[idx].y * -TR_TO_GODOT_SCALE,
					room_portal.vertices[idx].z * -TR_TO_GODOT_SCALE) + p_offset;
				st->add_vertex(portal_vertex);
			}
			st->add_index(index_offset + 0);
			st->add_index(index_offset + 1);
			st->add_index(index_offset + 2);

			st->add_index(index_offset + 0);
			st->add_index(index_offset + 2);
			st->add_index(index_offset + 3);

			index_offset += 4;
		}

		if (p_material_table.portal_stencil_materials.has(stencil_id)) {
			st->set_material(p_material_table.portal_stencil_materials.get(stencil_id));
		} else {
			st->set_material(portal_debug_material);
		}

		st->generate_normals();
		ar_mesh = st->commit(ar_mesh);
	}
#endif

	return ar_mesh;
}

Ref<ArrayMesh> tr_mesh_to_godot_mesh(
	const TRMesh& p_mesh_data,
	const Vector<Ref<Material>> p_solid_materials,
	const Vector<Ref<Material>> p_level_materials,
	const Ref<Material> p_level_palette_material,
	const Vector<TRTextureInfo> p_texture_infos) {
	Ref<SurfaceTool> st = memnew(SurfaceTool);
	Ref<ArrayMesh> ar_mesh = memnew(ArrayMesh);

	Vector<TRVertex> mesh_verts;
	Vector<TRVertex> mesh_normals;

	for (int32_t i = 0; i < p_mesh_data.vertices.size(); i++) {
		mesh_verts.push_back(p_mesh_data.vertices[i]);
	}

	// Only use mesh normals if normals count is positive. It otherwise counts as vertex lighting.
	if (p_mesh_data.normal_count == p_mesh_data.normals.size()) {
		for (int32_t i = 0; i < p_mesh_data.normals.size(); i++) {
			mesh_normals.push_back(p_mesh_data.normals[i]);
		}
	}

	struct VertexAndUV {
		int32_t vertex_idx;
		TRUV uv;
	};

	int32_t last_material_id = 0;

	// Colors
	Vector<VertexAndUV> vertex_color_uv_map;
	Vector<int32_t> color_index_map;
	for (int32_t i = 0; i < p_mesh_data.color_quads_count; i++) {
		uint16_t id = p_mesh_data.color_quads[i].tex_info_id;

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
		uint16_t id = p_mesh_data.color_triangles[i].tex_info_id;

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
		if (i < p_mesh_data.texture_quads.size()) { // Cape Fear bug
			TRTextureInfo texture_info = p_texture_infos.get(p_mesh_data.texture_quads[i].tex_info_id);
			int32_t material_id = texture_info.texture_page;
			if (texture_info.draw_type == 1) {
				material_id += p_solid_materials.size();
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
	}

	for (int32_t i = 0; i < p_mesh_data.texture_triangles_count; i++) {
		if (i < p_mesh_data.texture_triangles.size()) { // Cape Fear bug
			TRTextureInfo texture_info = p_texture_infos.get(p_mesh_data.texture_triangles[i].tex_info_id);
			int32_t material_id = texture_info.texture_page;
			if (texture_info.draw_type == 1) {
				material_id += p_solid_materials.size();
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
	}

	if (vertex_color_uv_map.size() > 0) {
		st->begin(Mesh::PRIMITIVE_TRIANGLES);

		st->set_smooth_group(0);

		for (int32_t i = 0; i < vertex_color_uv_map.size(); i++) {
			VertexAndUV vertex_and_uv = vertex_color_uv_map.get(i);
			TRVertex vertex = mesh_verts[vertex_and_uv.vertex_idx];

			Vector2 uv = Vector2(u_fixed_16_to_float(vertex_and_uv.uv.u, false) / 255.0f, u_fixed_16_to_float(vertex_and_uv.uv.v, false) / 255.0f);
			st->set_uv(uv);
			
			if (vertex_and_uv.vertex_idx < mesh_normals.size()) {
				TRVertex normal = mesh_normals[vertex_and_uv.vertex_idx];
				Vector3 normal_float = Vector3(
					normal.x * TR_TO_GODOT_SCALE,
					normal.y * -TR_TO_GODOT_SCALE,
					normal.z * -TR_TO_GODOT_SCALE);

				st->set_normal(normal_float);
			}

			Vector3 vertex_float = Vector3(
				vertex.x * TR_TO_GODOT_SCALE,
				vertex.y * -TR_TO_GODOT_SCALE,
				vertex.z * -TR_TO_GODOT_SCALE);

			st->add_vertex(vertex_float);
		}

		for (int32_t i = 0; i < color_index_map.size(); i++) {
			st->add_index(color_index_map.get(i));
		}

		st->set_material(p_level_palette_material);
		ar_mesh = st->commit(ar_mesh);
	}

	Vector<Ref<Material>> all_materials = p_solid_materials;
	all_materials.append_array(p_level_materials);

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

			if (vertex_and_uv.vertex_idx < mesh_normals.size()) {
				TRVertex normal = mesh_normals[vertex_and_uv.vertex_idx];
				Vector3 normal_float = Vector3(
					normal.x * TR_TO_GODOT_SCALE,
					normal.y * -TR_TO_GODOT_SCALE,
					normal.z * -TR_TO_GODOT_SCALE);

				st->set_normal(normal_float);
			}

			Vector3 vertex_float = Vector3(
				vertex.x * TR_TO_GODOT_SCALE,
				vertex.y * -TR_TO_GODOT_SCALE,
				vertex.z * -TR_TO_GODOT_SCALE);

			st->add_vertex(vertex_float);
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

CollisionShape3D *tr_room_to_godot_collision_shape(
	const TRRoom& p_current_room,
	const PackedByteArray p_floor_data,
	const Vector<TRRoom> p_rooms,
	const Vector3 p_offset) {
	Ref<ConcavePolygonShape3D> collision_data = memnew(ConcavePolygonShape3D);

	PackedVector3Array buf;

	int32_t y_bottom = p_current_room.info.y_bottom;
	int32_t y_top = p_current_room.info.y_top;

	Vector<GeometryCalculation> calculated;
	calculated.resize(p_current_room.sector_count_z * p_current_room.sector_count_x);

	int32_t current_sector = 0;

	// Floors and ceilings
	for (int32_t z_sector = 0; z_sector < p_current_room.sector_count_z; z_sector++) {
		for (int32_t x_sector = 0; x_sector < p_current_room.sector_count_x; x_sector++) {
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
					uint32_t global_z_sector = (current_room_x + z_sector);
					uint32_t global_x_sector = (current_room_z + x_sector);

					uint32_t new_room_x = (new_room.info.x >> 10);
					uint32_t new_room_z = (new_room.info.z >> 10);

					// SWAP
					uint32_t new_room_local_z = (global_z_sector - new_room_x);
					uint32_t new_room_local_x = (global_x_sector - new_room_z);

					int32_t new_sector_id = (new_room_local_z * new_room.sector_count_x) + new_room_local_x;
					if (new_sector_id > 0 && new_sector_id <= new_room.sectors.size()) {
						room_sector = new_room.sectors.get(new_sector_id);
					}

					room_below = room_sector.room_below;
					room_above = room_sector.room_above;

					if (room_below != 0xff) {
						calc.portal_floor = true;
					}

					if (room_above != 0xff) {
						calc.portal_ceiling = true;
					}

					floor_height = room_sector.floor;
					ceiling_height = room_sector.ceiling;

					floor_data_index = room_sector.floor_data_index;

					geo_shift = parse_floor_data_entry(p_floor_data, floor_data_index);
				}
			}


			int16_t ne_floor_height = -floor_height + geo_shift.ne_floor_shift;
			int16_t se_floor_height = -floor_height + geo_shift.se_floor_shift;
			int16_t sw_floor_height = -floor_height + geo_shift.sw_floor_shift;
			int16_t nw_floor_height = -floor_height + geo_shift.nw_floor_shift;

			int16_t ne_ceiling_height = -ceiling_height + geo_shift.ne_ceiling_shift;
			int16_t se_ceiling_height = -ceiling_height + geo_shift.se_ceiling_shift;
			int16_t sw_ceiling_height = -ceiling_height + geo_shift.sw_ceiling_shift;
			int16_t nw_ceiling_height = -ceiling_height + geo_shift.nw_ceiling_shift;

			if (calc.portal_wall) {
				int8_t floor_max = -(p_current_room.info.y_top >> 8);
				if (ne_floor_height > floor_max) {
					ne_floor_height = floor_max;
				}
				if (se_floor_height > floor_max) {
					se_floor_height = floor_max;
				}
				if (sw_floor_height > floor_max) {
					sw_floor_height = floor_max;
				}
				if (nw_floor_height > floor_max) {
					nw_floor_height = floor_max;
				}

				int8_t ceiling_max = -(p_current_room.info.y_bottom >> 8);
				if (ne_ceiling_height < ceiling_max) {
					ne_ceiling_height = ceiling_max;
				}
				if (se_ceiling_height < ceiling_max) {
					se_ceiling_height = ceiling_max;
				}
				if (sw_ceiling_height < ceiling_max) {
					sw_ceiling_height = ceiling_max;
				}
				if (nw_ceiling_height < ceiling_max) {
					nw_ceiling_height = ceiling_max;
				}
			}

			{
				if (calc.portal_floor) {
					calc.floor_north_east = (real_t)(-floor_height) * TR_CLICK_SIZE;
					calc.floor_north_west = (real_t)(-floor_height) * TR_CLICK_SIZE;
					calc.floor_south_east = (real_t)(-floor_height) * TR_CLICK_SIZE;
					calc.floor_south_west = (real_t)(-floor_height) * TR_CLICK_SIZE;
					calc.solid = false;
				}

				if (calc.portal_ceiling) {
					calc.ceiling_north_east = (real_t)(-ceiling_height) * TR_CLICK_SIZE;
					calc.ceiling_north_west = (real_t)(-ceiling_height) * TR_CLICK_SIZE;
					calc.ceiling_south_east = (real_t)(-ceiling_height) * TR_CLICK_SIZE;
					calc.ceiling_south_west = (real_t)(-ceiling_height) * TR_CLICK_SIZE;
					calc.solid = false;
				}
			}

			// floor
			if ((ne_ceiling_height != ne_floor_height
				|| se_ceiling_height != se_floor_height
				|| sw_ceiling_height != sw_floor_height
				|| nw_ceiling_height != nw_floor_height) && ((floor_height != -127 && room_below == 0xff) || (geo_shift.cull_first_floor_triangle || geo_shift.cull_second_floor_triangle))) {

				real_t ne_offset_f = (real_t)(ne_floor_height) * TR_CLICK_SIZE;
				real_t se_offset_f = (real_t)(se_floor_height) * TR_CLICK_SIZE;
				real_t sw_offset_f = (real_t)(sw_floor_height) * TR_CLICK_SIZE;
				real_t nw_offset_f = (real_t)(nw_floor_height) * TR_CLICK_SIZE;

				calc.floor_north_east = ne_offset_f;
				calc.floor_south_east = se_offset_f;
				calc.floor_north_west = nw_offset_f;
				calc.floor_south_west = sw_offset_f;

				if (!calc.portal_wall) {
					if (geo_shift.rotate_floor_triangles) {
						if (!geo_shift.cull_first_floor_triangle) {
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector, nw_offset_f, -(TR_SQUARE_SIZE * x_sector)) + p_offset);
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector, sw_offset_f, -(TR_SQUARE_SIZE * x_sector + TR_SQUARE_SIZE)) + p_offset);
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector + TR_SQUARE_SIZE, se_offset_f, -(TR_SQUARE_SIZE * x_sector + TR_SQUARE_SIZE)) + p_offset);
						}

						if (!geo_shift.cull_second_floor_triangle) {
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector, nw_offset_f, -(TR_SQUARE_SIZE * x_sector)) + p_offset);
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector + TR_SQUARE_SIZE, se_offset_f, -(TR_SQUARE_SIZE * x_sector + TR_SQUARE_SIZE)) + p_offset);
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector + TR_SQUARE_SIZE, ne_offset_f, -(TR_SQUARE_SIZE * x_sector)) + p_offset);
						}
					} else {
						if (!geo_shift.cull_first_floor_triangle) {
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector, sw_offset_f, -(TR_SQUARE_SIZE * x_sector + TR_SQUARE_SIZE)) + p_offset);
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector + TR_SQUARE_SIZE, ne_offset_f, -(TR_SQUARE_SIZE * x_sector)) + p_offset);
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector, nw_offset_f, -(TR_SQUARE_SIZE * x_sector)) + p_offset);
						}

						if (!geo_shift.cull_second_floor_triangle) {
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector, sw_offset_f, -(TR_SQUARE_SIZE * x_sector + TR_SQUARE_SIZE)) + p_offset);
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector + TR_SQUARE_SIZE, se_offset_f, -(TR_SQUARE_SIZE * x_sector + TR_SQUARE_SIZE)) + p_offset);
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector + TR_SQUARE_SIZE, ne_offset_f, -(TR_SQUARE_SIZE * x_sector)) + p_offset);
						}
					}
				}

				calc.solid = false;
			}

			// ceiling
			if ((ne_ceiling_height != ne_floor_height
				|| se_ceiling_height != se_floor_height
				|| sw_ceiling_height != sw_floor_height
				|| nw_ceiling_height != nw_floor_height) && ((ceiling_height != -127 && room_above == 0xff) || (geo_shift.cull_first_ceiling_triangle || geo_shift.cull_second_ceiling_triangle))) {

				real_t ne_offset_f = (real_t)(ne_ceiling_height) * TR_CLICK_SIZE;
				real_t se_offset_f = (real_t)(se_ceiling_height) * TR_CLICK_SIZE;
				real_t sw_offset_f = (real_t)(sw_ceiling_height) * TR_CLICK_SIZE;
				real_t nw_offset_f = (real_t)(nw_ceiling_height) * TR_CLICK_SIZE;

				calc.ceiling_north_east = ne_offset_f;
				calc.ceiling_south_east = se_offset_f;
				calc.ceiling_north_west = nw_offset_f;
				calc.ceiling_south_west = sw_offset_f;

				if (!calc.portal_wall) {
					if (geo_shift.rotate_ceiling_triangles) {
						if (!geo_shift.cull_first_ceiling_triangle) {
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector + TR_SQUARE_SIZE, se_offset_f, -(TR_SQUARE_SIZE * x_sector + TR_SQUARE_SIZE)) + p_offset);
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector, sw_offset_f, -(TR_SQUARE_SIZE * x_sector + TR_SQUARE_SIZE)) + p_offset);
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector, nw_offset_f, -(TR_SQUARE_SIZE * x_sector)) + p_offset);
						}

						if (!geo_shift.cull_second_ceiling_triangle) {
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector + TR_SQUARE_SIZE, ne_offset_f, -(TR_SQUARE_SIZE * x_sector)) + p_offset);
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector + TR_SQUARE_SIZE, se_offset_f, -(TR_SQUARE_SIZE * x_sector + TR_SQUARE_SIZE)) + p_offset);
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector, nw_offset_f, -(TR_SQUARE_SIZE * x_sector)) + p_offset);
						}
					} else {
						if (!geo_shift.cull_first_ceiling_triangle) {
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector, nw_offset_f, -(TR_SQUARE_SIZE * x_sector)) + p_offset);
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector + TR_SQUARE_SIZE, ne_offset_f, -(TR_SQUARE_SIZE * x_sector)) + p_offset);
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector, sw_offset_f, -(TR_SQUARE_SIZE * x_sector + TR_SQUARE_SIZE)) + p_offset);
						}

						if (!geo_shift.cull_second_ceiling_triangle) {
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector + TR_SQUARE_SIZE, ne_offset_f, -(TR_SQUARE_SIZE * x_sector)) + p_offset);
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector + TR_SQUARE_SIZE, se_offset_f, -(TR_SQUARE_SIZE * x_sector + TR_SQUARE_SIZE)) + p_offset);
							buf.append(Vector3(TR_SQUARE_SIZE * z_sector, sw_offset_f, -(TR_SQUARE_SIZE * x_sector + TR_SQUARE_SIZE)) + p_offset);
						}
					}
				}

				calc.solid = false;
			}

			calculated.set(current_sector, calc);
			current_sector++;
		}
	}

	current_sector = 0;

	// Walls
	for (int32_t z_sector = 0; z_sector < p_current_room.sector_count_z; z_sector++) {
		for (int32_t x_sector = 0; x_sector < p_current_room.sector_count_x; x_sector++) {
			GeometryCalculation calc = calculated.get(current_sector);

			if (z_sector >= 1 && z_sector < p_current_room.sector_count_z - 1 && x_sector >= 1 && x_sector < p_current_room.sector_count_x) {
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
						Vector3 current_bottom_left =	Vector3((TR_SQUARE_SIZE * z_sector),					calc.floor_north_west,		-(TR_SQUARE_SIZE * x_sector)) + p_offset;
						Vector3 current_bottom_right =	Vector3((TR_SQUARE_SIZE * z_sector) + TR_SQUARE_SIZE,	calc.floor_north_east,		-(TR_SQUARE_SIZE * x_sector)) + p_offset;
						Vector3 current_top_left =		Vector3((TR_SQUARE_SIZE * z_sector),					calc.ceiling_north_west,	-(TR_SQUARE_SIZE * x_sector)) + p_offset;
						Vector3 current_top_right =		Vector3((TR_SQUARE_SIZE * z_sector) + TR_SQUARE_SIZE,	calc.ceiling_north_east,	-(TR_SQUARE_SIZE * x_sector)) + p_offset;

						if (north_calc.solid) {
							buf.append(current_bottom_left);
							buf.append(current_bottom_right);
							buf.append(current_top_left);

							buf.append(current_bottom_right);
							buf.append(current_top_right);
							buf.append(current_top_left);
						} else {
							Vector3 next_bottom_left =	Vector3((TR_SQUARE_SIZE * z_sector),					north_calc.floor_south_west,	-(TR_SQUARE_SIZE * x_sector)) + p_offset;
							Vector3 next_bottom_right =	Vector3((TR_SQUARE_SIZE * z_sector) + TR_SQUARE_SIZE,	north_calc.floor_south_east,	-(TR_SQUARE_SIZE * x_sector)) + p_offset;
							Vector3 next_top_left =		Vector3((TR_SQUARE_SIZE * z_sector),					north_calc.ceiling_south_west,	-(TR_SQUARE_SIZE * x_sector)) + p_offset;
							Vector3 next_top_right =	Vector3((TR_SQUARE_SIZE * z_sector) + TR_SQUARE_SIZE,	north_calc.ceiling_south_east,	-(TR_SQUARE_SIZE * x_sector)) + p_offset;

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
						Vector3 current_bottom_left =	Vector3((TR_SQUARE_SIZE * z_sector),					calc.floor_south_west,		-(TR_SQUARE_SIZE * x_sector) - TR_SQUARE_SIZE) + p_offset;
						Vector3 current_bottom_right =	Vector3((TR_SQUARE_SIZE * z_sector) + TR_SQUARE_SIZE,	calc.floor_south_east,		-(TR_SQUARE_SIZE * x_sector) - TR_SQUARE_SIZE) + p_offset;
						Vector3 current_top_left =		Vector3((TR_SQUARE_SIZE * z_sector),					calc.ceiling_south_west,	-(TR_SQUARE_SIZE * x_sector) - TR_SQUARE_SIZE) + p_offset;
						Vector3 current_top_right =		Vector3((TR_SQUARE_SIZE * z_sector) + TR_SQUARE_SIZE,	calc.ceiling_south_east,	-(TR_SQUARE_SIZE * x_sector) - TR_SQUARE_SIZE) + p_offset;

						if (south_calc.solid) {
							buf.append(current_top_left);
							buf.append(current_bottom_right);
							buf.append(current_bottom_left);

							buf.append(current_top_left);
							buf.append(current_top_right);
							buf.append(current_bottom_right);
						} else {
							Vector3 next_bottom_left =	Vector3((TR_SQUARE_SIZE * z_sector),					south_calc.floor_north_west,	-(TR_SQUARE_SIZE * x_sector) - TR_SQUARE_SIZE) + p_offset;
							Vector3 next_bottom_right =	Vector3((TR_SQUARE_SIZE * z_sector) + TR_SQUARE_SIZE,	south_calc.floor_north_east,	-(TR_SQUARE_SIZE * x_sector) - TR_SQUARE_SIZE) + p_offset;
							Vector3 next_top_left =		Vector3((TR_SQUARE_SIZE * z_sector),					south_calc.ceiling_north_west,	-(TR_SQUARE_SIZE * x_sector) - TR_SQUARE_SIZE) + p_offset;
							Vector3 next_top_right =	Vector3((TR_SQUARE_SIZE * z_sector) + TR_SQUARE_SIZE,	south_calc.ceiling_north_east,	-(TR_SQUARE_SIZE * x_sector) - TR_SQUARE_SIZE) + p_offset;
						
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
						Vector3 current_bottom_left =	Vector3((TR_SQUARE_SIZE * z_sector) + TR_SQUARE_SIZE, calc.floor_north_east,		-(TR_SQUARE_SIZE * x_sector))					+ p_offset;
						Vector3 current_bottom_right =	Vector3((TR_SQUARE_SIZE * z_sector) + TR_SQUARE_SIZE, calc.floor_south_east,		-(TR_SQUARE_SIZE * x_sector) - TR_SQUARE_SIZE)	+ p_offset;
						Vector3 current_top_left =		Vector3((TR_SQUARE_SIZE * z_sector) + TR_SQUARE_SIZE, calc.ceiling_north_east,		-(TR_SQUARE_SIZE * x_sector))					+ p_offset;
						Vector3 current_top_right =		Vector3((TR_SQUARE_SIZE * z_sector) + TR_SQUARE_SIZE, calc.ceiling_south_east,		-(TR_SQUARE_SIZE * x_sector) - TR_SQUARE_SIZE)	+ p_offset;

						if (east_calc.solid) {
							buf.append(current_bottom_left);
							buf.append(current_bottom_right);
							buf.append(current_top_left);

							buf.append(current_bottom_right);
							buf.append(current_top_right);
							buf.append(current_top_left);
						} else {
							Vector3 next_bottom_left =	Vector3((TR_SQUARE_SIZE * z_sector) + TR_SQUARE_SIZE, east_calc.floor_north_west,		-(TR_SQUARE_SIZE * x_sector))					+ p_offset;
							Vector3 next_bottom_right =	Vector3((TR_SQUARE_SIZE * z_sector) + TR_SQUARE_SIZE, east_calc.floor_south_west,		-(TR_SQUARE_SIZE * x_sector) - TR_SQUARE_SIZE)	+ p_offset;
							Vector3 next_top_left =		Vector3((TR_SQUARE_SIZE * z_sector) + TR_SQUARE_SIZE, east_calc.ceiling_north_west,		-(TR_SQUARE_SIZE * x_sector))					+ p_offset;
							Vector3 next_top_right =	Vector3((TR_SQUARE_SIZE * z_sector) + TR_SQUARE_SIZE, east_calc.ceiling_south_west,		-(TR_SQUARE_SIZE * x_sector) - TR_SQUARE_SIZE)	+ p_offset;

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
						Vector3 current_bottom_left =	Vector3((TR_SQUARE_SIZE * z_sector), calc.floor_north_west,		-(TR_SQUARE_SIZE * x_sector))					+ p_offset;
						Vector3 current_bottom_right =	Vector3((TR_SQUARE_SIZE * z_sector), calc.floor_south_west,		-(TR_SQUARE_SIZE * x_sector) - TR_SQUARE_SIZE)	+ p_offset;
						Vector3 current_top_left =		Vector3((TR_SQUARE_SIZE * z_sector), calc.ceiling_north_west,	-(TR_SQUARE_SIZE * x_sector))					+ p_offset;
						Vector3 current_top_right =		Vector3((TR_SQUARE_SIZE * z_sector), calc.ceiling_south_west,	-(TR_SQUARE_SIZE * x_sector) - TR_SQUARE_SIZE)	+ p_offset;

						if (west_calc.solid) {
							buf.append(current_top_left);
							buf.append(current_bottom_right);
							buf.append(current_bottom_left);

							buf.append(current_top_left);
							buf.append(current_top_right);
							buf.append(current_bottom_right);
						} else {
							Vector3 next_bottom_left =	Vector3((TR_SQUARE_SIZE * z_sector), west_calc.floor_north_east,	-(TR_SQUARE_SIZE * x_sector))					+ p_offset;
							Vector3 next_bottom_right =	Vector3((TR_SQUARE_SIZE * z_sector), west_calc.floor_south_east,	-(TR_SQUARE_SIZE * x_sector) - TR_SQUARE_SIZE)	+ p_offset;
							Vector3 next_top_left =		Vector3((TR_SQUARE_SIZE * z_sector), west_calc.ceiling_north_east,	-(TR_SQUARE_SIZE * x_sector))					+ p_offset;
							Vector3 next_top_right =	Vector3((TR_SQUARE_SIZE * z_sector), west_calc.ceiling_south_east,	-(TR_SQUARE_SIZE * x_sector) - TR_SQUARE_SIZE)	+ p_offset;
							
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
	new_material->set_shading_mode(BaseMaterial3D::SHADING_MODE_PER_VERTEX);
	new_material->set_specular(0.0);
	new_material->set_roughness(1.0);
	new_material->set_metallic(0.0);
	new_material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	new_material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, p_image_texture);
	new_material->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_NEAREST);
	new_material->set_flag(BaseMaterial3D::Flags::FLAG_USE_TEXTURE_REPEAT, false);
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

void add_pixel_padding(Ref<Image> p_target_image, Ref<Image> p_source_image, Rect2i p_source_rect, Point2i p_target_position, int32_t p_pixel_padding) {
	for (int32_t i = 0; i < p_pixel_padding / 2; i++) {
		// Top
		for (int32_t x = 0 - (i + 1); x < p_source_rect.size.width + (i + 1); x++) {
			p_target_image->set_pixel(p_target_position.x + x, p_target_position.y - (i + 1), p_source_image->get_pixel(CLAMP(x, 0, p_source_image->get_width() - 1), 0));
		}

		// Bottom
		for (int32_t x = 0 - (i + 1); x < p_source_rect.size.width + (i + 1); x++) {
			p_target_image->set_pixel(p_target_position.x + x, p_target_position.y + p_source_rect.get_size().y + (i), p_source_image->get_pixel(CLAMP(x, 0, p_source_image->get_width() - 1), p_source_rect.get_size().y - 1));
		}

		// Left
		for (int32_t y = 0 - (i + 1); y < p_source_rect.size.height + (i + 1); y++) {
			p_target_image->set_pixel(p_target_position.x - (i + 1), p_target_position.y + y, p_source_image->get_pixel(0, CLAMP(y, 0, p_source_image->get_height() - 1)));
		}

		// Right
		for (int32_t y = 0 - (i + 1); y < p_source_rect.size.height + (i + 1); y++) {
			p_target_image->set_pixel(p_target_position.x + p_source_rect.get_size().x + (i), p_target_position.y + y, p_source_image->get_pixel(p_source_rect.get_size().x - 1, CLAMP(y, 0, p_source_image->get_height() - 1)));
		}
	}
}

uint32_t get_hash_for_image(Ref<Image> p_image) {
	Array image_array;
	image_array.append(p_image->get_width());
	image_array.append(p_image->get_height());
	image_array.append(p_image->get_format());
	image_array.append(p_image->get_data());

	return image_array.hash();
}

struct TRTextureInfoRemapInfoChart {
	Point2i src_points[4];
	uint32_t src_texture_page_id;
	uint32_t src_chart_id;
};

struct TRTexturePageAtlas {
	Vector<Rect2i> used_charts;
};

struct TRMoveableReAtlasResult {
	//HashMap<uint32_t, TRMoveableReAtlasedMeshData> mesh_table;
	Vector<Ref<Image>> custom_atlases;
};

TRMoveableReAtlasResult reatlas_object_group_textures(
	const Vector<TRMoveableInfo> p_moveable_infos,
	const Vector<TRMesh> &p_meshes,
	const Vector<TRTextureInfo> p_texture_infos,
	const Vector<Ref<Image>> p_images) {

	// This is a pretty suboptimal texture-atlas creator, but it should hopefully be *good enough* for now.

	Vector<TRTextureInfoRemapInfoChart> texture_info_remaps;
	texture_info_remaps.resize(p_texture_infos.size());

	TRMoveableReAtlasResult result;
	HashMap<uint16_t, TRTexturePageAtlas> atlases;
	Vector<uint16_t> used_texture_pages;

	HashMap<uint32_t, TRTextureInfoRemapInfoChart> remapped_texture_infos;

	for (const TRMoveableInfo& moveable_info : p_moveable_infos) {
		if (moveable_info.mesh_count > 0) {
			for (int64_t mesh_idx = 0; mesh_idx < moveable_info.mesh_count; mesh_idx++) {
				int32_t offset_mesh_index = moveable_info.mesh_index + mesh_idx;
				if (offset_mesh_index < p_meshes.size()) {
					TRMesh mesh = p_meshes.get(offset_mesh_index);

					// Quads
					for (int32_t i = 0; i < mesh.texture_quads_count; i++) {
						uint16_t tex_info_id = mesh.texture_quads[i].tex_info_id;
						TRTextureInfo texture_info = p_texture_infos.get(tex_info_id);
						uint16_t texture_page_id = texture_info.texture_page;

						// Get the rounded UV positions from the original atlas.
						Point2i uv[4];
						for (int32_t j = 0; j < 4; j++) {
							uv[j] = Point2i(int32_t(round(u_fixed_16_to_float(texture_info.uv[j].u, true))), int32_t(round(u_fixed_16_to_float(texture_info.uv[j].v, true))));
						}

						// Calculate a bounding box for the UVs.
						Rect2i texture_rect;
						texture_rect.set_position(uv[0]);
						texture_rect.expand_to(uv[1]);
						texture_rect.expand_to(uv[2]);
						texture_rect.expand_to(uv[3]);

						// Mark the texture page as having been used.
						if (!used_texture_pages.has(texture_page_id)) {
							used_texture_pages.append(texture_page_id);
						}

						// Add the chart to the list of the ones we've used for the original texture page.
						int64_t chart_index = atlases[texture_page_id].used_charts.find(texture_rect);
						if (chart_index < 0) {
							if (atlases[texture_page_id].used_charts.append(texture_rect)) {
								chart_index = atlases[texture_page_id].used_charts.size() - 1;
							}
						}

						// Write the exact UV positions here with the corresponding chart.
						if (!remapped_texture_infos.has(tex_info_id) && chart_index >= 0) {
							remapped_texture_infos[tex_info_id].src_texture_page_id = texture_page_id;
							remapped_texture_infos[tex_info_id].src_chart_id = chart_index;
							for (int32_t j = 0; j < 4; j++) {
								remapped_texture_infos[tex_info_id].src_points[j] = uv[j];
							}
						}
					}

					// Triangles
					for (int32_t i = 0; i < mesh.texture_triangles_count; i++) {
						uint16_t tex_info_id = mesh.texture_triangles[i].tex_info_id;
						TRTextureInfo texture_info = p_texture_infos.get(tex_info_id);
						uint16_t texture_page_id = texture_info.texture_page;

						// Get the rounded UV positions from the original atlas.
						Point2i uv[4];
						for (int32_t j = 0; j < 4; j++) {
							uv[j] = Point2i(int32_t(round(u_fixed_16_to_float(texture_info.uv[j].u, true))), int32_t(round(u_fixed_16_to_float(texture_info.uv[j].v, true))));
						}

						// Calculate a bounding box for the UVs.
						Rect2i texture_rect;
						texture_rect.set_position(uv[0]);
						texture_rect.expand_to(uv[1]);
						texture_rect.expand_to(uv[2]);

						// Mark the texture page as having been used.
						if (!used_texture_pages.has(texture_page_id)) {
							used_texture_pages.append(texture_page_id);
						}

						// Add the chart to the list of the ones we've used for the original texture page.
						int64_t chart_index = atlases[texture_page_id].used_charts.find(texture_rect);
						if (chart_index < 0) {
							if (atlases[texture_page_id].used_charts.append(texture_rect)) {
								chart_index = atlases[texture_page_id].used_charts.size() - 1;
							}
						}

						// Write the exact UV positions here with the corresponding chart.
						if (!remapped_texture_infos.has(tex_info_id) && chart_index >= 0) {
							remapped_texture_infos[tex_info_id].src_texture_page_id = texture_page_id;
							remapped_texture_infos[tex_info_id].src_chart_id = chart_index;
							for (int32_t j = 0; j < 4; j++) {
								remapped_texture_infos[tex_info_id].src_points[j] = uv[j];
							}
						}
					}
				}
			}
		}
	}

	// Use this to test for charts which are not enclosed by other charts.
	HashMap<uint16_t, TRTexturePageAtlas> root_atlases;
	for (uint16_t& used_texture_page_id : used_texture_pages) {
		root_atlases[used_texture_page_id] = TRTexturePageAtlas();

		TRTexturePageAtlas texture_page_atlas = atlases[used_texture_page_id];
		for (Rect2i& current_chart : texture_page_atlas.used_charts) {
			bool valid_chart = true;
			for (Rect2i& comparison_chart : texture_page_atlas.used_charts) {
				if (current_chart != comparison_chart) {
					if (comparison_chart.encloses(current_chart)) {
						valid_chart = false;
						break;
					}
				}
			}
			if (valid_chart) {
				root_atlases[used_texture_page_id].used_charts.append(current_chart);
			}
		}
	}

	struct TRSortedTextureAtlasChart {
		uint32_t overall_size;
		Point2i size;
		Ref<Image> image;
		bool rotated;
	};

	Vector<TRSortedTextureAtlasChart> sorted_charts;

	const int32_t PADDING_PIXELS = 4;

	for (uint16_t& used_texture_page_id : used_texture_pages) {
		TRTexturePageAtlas texture_page_atlas = root_atlases[used_texture_page_id];
		for (Rect2i& chart : texture_page_atlas.used_charts) {
			
			TRSortedTextureAtlasChart new_entry;
			real_t width = chart.size.x;
			real_t height = chart.size.y;

			Ref<Image> src_texture = p_images.get(used_texture_page_id);
			
			new_entry.image = memnew(Image(chart.size.width, chart.size.height, false, Image::FORMAT_RGBA8));
			new_entry.image->blit_rect(src_texture, chart, Vector2i());

			if (height > width) {
				new_entry.size = Point2i(height, width);
				new_entry.image->rotate_90(CLOCKWISE);
				new_entry.rotated = true;
			} else {
				new_entry.size = Point2i(width, height);
				new_entry.rotated = false;
			}

			new_entry.overall_size = new_entry.size.x * new_entry.size.y;
			sorted_charts.append(new_entry);
		}
	}

	struct SortAtlasCharts{
		bool operator()(const TRSortedTextureAtlasChart& p_a, const TRSortedTextureAtlasChart& p_b) const {
			if (p_a.overall_size > p_b.overall_size) {
				return true;
			} else if (p_a.overall_size < p_b.overall_size) {
				return false;
			} else {
				if (p_a.size.y > p_b.size.y) {
					return true;
				} else if (p_a.size.y < p_b.size.y) {
					return false;
				} else {
					if (p_a.size.x < p_b.size.x) {
						return true;
					} else if (p_a.size.x > p_b.size.x) {
						return false;
					} else {
						uint32_t a_hash = get_hash_for_image(p_a.image);
						uint32_t b_hash = get_hash_for_image(p_b.image);

						if (a_hash >= b_hash) {
							if (a_hash == b_hash) {
								print_error("Hash collision occurred while sorting texture atlas charts.");
							}
							return true;
						} else {
							return false;
						}
					}
				}
			}
		}
	};

	sorted_charts.sort_custom<SortAtlasCharts>();

	int32_t atlas_size = 32;
	Point2i current_position(PADDING_PIXELS, PADDING_PIXELS);
	int32_t max_row_height = 0;

	bool packing_successful = false;
	while (!packing_successful) {
		current_position = Point2i(PADDING_PIXELS, PADDING_PIXELS);
		max_row_height = 0;
		packing_successful = true;

		for (TRSortedTextureAtlasChart& sorted_chart : sorted_charts) {
			Rect2i src_rect = Rect2i(0, 0, sorted_chart.size.x, sorted_chart.size.y);

			if (current_position.x + (src_rect.size.x + PADDING_PIXELS) > atlas_size) {
				current_position.x = PADDING_PIXELS;
				current_position.y += max_row_height + PADDING_PIXELS;
				max_row_height = 0;
			}

			if (current_position.y + src_rect.size.y > atlas_size) {
				atlas_size = (atlas_size << 1);
				packing_successful = false;
				break;
			}

			current_position.x += (src_rect.size.x + PADDING_PIXELS);
			max_row_height = MAX(max_row_height, (src_rect.size.y + PADDING_PIXELS));
		}
	}

	int32_t atlas_width = atlas_size;
	int32_t atlas_height = atlas_size;

	// Reduce the vertical atlas size to as small as we can go.
	current_position.y += max_row_height + PADDING_PIXELS;
	while (current_position.y <= (atlas_height >> 1)) {
		atlas_height = atlas_height >> 1;
	}

	current_position = Point2i(PADDING_PIXELS, PADDING_PIXELS);
	max_row_height = 0;

	Ref<Image> new_atlas = memnew(Image(atlas_width, atlas_height, false, Image::FORMAT_RGBA8));
	ERR_FAIL_COND_V(new_atlas.is_null(), TRMoveableReAtlasResult());

	for (TRSortedTextureAtlasChart& sorted_chart : sorted_charts) {
		Rect2i src_rect = Rect2i(0, 0, sorted_chart.size.x, sorted_chart.size.y);

		if (current_position.x + src_rect.size.x + PADDING_PIXELS > new_atlas->get_width()) {
			current_position.x = PADDING_PIXELS;
			current_position.y += max_row_height + PADDING_PIXELS;
			max_row_height = 0;
		}

		if (current_position.y + src_rect.size.y > new_atlas->get_height()) {
			continue;
		}

		Ref<Image> src_texture = sorted_chart.image;

		new_atlas->blit_rect(src_texture, src_rect, current_position);

		add_pixel_padding(new_atlas, src_texture, src_rect, current_position, PADDING_PIXELS);

		current_position.x += src_rect.size.x + PADDING_PIXELS;
		max_row_height = MAX(max_row_height, src_rect.size.y);
	}

	PackedByteArray pba = new_atlas->get_data();

	result.custom_atlases.push_back(new_atlas);

	String test_atlas_path = String("test_atlas") + String(".png");
	new_atlas->save_png(test_atlas_path);
}

struct TRTextureInfoRemap {
	int32_t remap_index = -1;
	Vector2 uv[4];
};

void remap_room_textures(
	const Vector<TRRoom> p_rooms,
	const Vector<Ref<Image>> p_images,
	const Vector<TRTextureInfo> p_texture_infos,
	const HashMap<String, String> p_room_name_table) {
	
	// Room Texture Remapping
	Vector<uint32_t> valid_room_texture_quad_ids;
	Vector<uint32_t> valid_room_texture_triangle_ids;

	Vector2i largest_room_texture_chart = Vector2i(0, 0);

	for (const TRRoom& room : p_rooms) {
		for (int32_t i = 0; i < room.data.room_quad_count; i++) {
			if (room.data.room_quads[i].tex_info_id < 0) {
				continue;
			}

			if (!valid_room_texture_quad_ids.has(room.data.room_quads[i].tex_info_id)) {
				valid_room_texture_quad_ids.push_back(room.data.room_quads[i].tex_info_id);
			}
		}

		for (int32_t i = 0; i < room.data.room_triangle_count; i++) {
			if (room.data.room_triangles[i].tex_info_id < 0) {
				continue;
			}

			if (!valid_room_texture_triangle_ids.has(room.data.room_triangles[i].tex_info_id)) {
				valid_room_texture_triangle_ids.push_back(room.data.room_triangles[i].tex_info_id);
			}
		}
	}

	Vector<TRTexturePageAtlas> atlases;
	atlases.resize(p_images.size());

	// Get all the quad-based textures.
	uint32_t index = 0;
	for (const uint32_t& id : valid_room_texture_quad_ids) {
		if (id >= p_texture_infos.size()) {
			index++;
			continue;
		}

		TRTextureInfo texture_info = p_texture_infos.get(id);

		if (texture_info.texture_page >= p_images.size()) {
			index++;
			continue;
		}

		Rect2i texture_rect;
		texture_rect.set_position(Point2i(texture_info.uv[0].u, texture_info.uv[0].v));
		texture_rect.expand_to(Point2i(texture_info.uv[1].u, texture_info.uv[1].v));
		texture_rect.expand_to(Point2i(texture_info.uv[2].u, texture_info.uv[2].v));
		texture_rect.expand_to(Point2i(texture_info.uv[3].u, texture_info.uv[3].v));

		if (texture_rect.size.x > largest_room_texture_chart.x) {
			largest_room_texture_chart.x = texture_rect.size.x;
		}

		if (texture_rect.size.y > largest_room_texture_chart.y) {
			largest_room_texture_chart.y = texture_rect.size.y;
		}

		TRTexturePageAtlas atlas = atlases.get(texture_info.texture_page);
		atlas.used_charts.append(texture_rect);
		atlases.set(texture_info.texture_page, atlas);
	}

	// Get all the triangle-based textures.
	for (const uint32_t& id : valid_room_texture_triangle_ids) {
		if (id >= p_texture_infos.size()) {
			index++;
			continue;
		}

		TRTextureInfo texture_info = p_texture_infos.get(id);

		if (texture_info.texture_page >= p_images.size()) {
			index++;
			continue;
		}

		Rect2i texture_rect;
		texture_rect.set_position(Point2i(texture_info.uv[0].u, texture_info.uv[0].v));
		texture_rect.expand_to(Point2i(texture_info.uv[1].u, texture_info.uv[1].v));
		texture_rect.expand_to(Point2i(texture_info.uv[2].u, texture_info.uv[2].v));

		TRTexturePageAtlas atlas = atlases.get(texture_info.texture_page);
		atlas.used_charts.append(texture_rect);
		atlases.set(texture_info.texture_page, atlas);
	}

	Vector<TRTexturePageAtlas> filtered_atlases;
	filtered_atlases.resize(p_images.size());

	// Test for any charts enclosed by other charts.
	for (int32_t i = 0; i < p_images.size(); i++) {
		for (const Rect2i &chart : atlases[i].used_charts) {
			bool skip = false;
			for (int32_t j = 0; j < atlases[i].used_charts.size(); j++) {
				if (atlases[i].used_charts[j].encloses(chart)) {
					if (atlases[i].used_charts[j] != chart) {
						skip = true;
						break;
					}
				}
			}

			if (!skip) {
				TRTexturePageAtlas atlas = filtered_atlases.get(i);
				atlas.used_charts.append(chart);
				filtered_atlases.set(i, atlas);
			}

			// TODO: super aggressive sub-chart detection
			
		}
	}

	for (int32_t image_idx = 0; image_idx < p_images.size(); image_idx++) {
		const TRTexturePageAtlas& atlas = filtered_atlases[image_idx];
		Ref<Image> image_texture = p_images.get(image_idx);

		for (const Rect2i& chart : filtered_atlases[image_idx].used_charts) {
			real_t x = u_fixed_16_to_float(chart.position.x, true);
			real_t y = u_fixed_16_to_float(chart.position.y, true);
			real_t width = u_fixed_16_to_float(chart.size.x, true);
			real_t height = u_fixed_16_to_float(chart.size.y, true);

			Rect2i size_corrected_chart;
			size_corrected_chart.position.x = int32_t(round(x));
			size_corrected_chart.position.y = int32_t(round(y));
			size_corrected_chart.size.width = int32_t(round(width));
			size_corrected_chart.size.height = int32_t(round(height));

			Ref<Image> image_region = image_texture->get_region(size_corrected_chart);

			HashingContext hashing_context;

			hashing_context.start(HashingContext::HASH_SHA1);

			StreamPeerBuffer stream_peer;
			stream_peer.put_32(image_region->get_width());
			stream_peer.put_32(image_region->get_height());
			stream_peer.put_data(image_region->get_data().ptr(), image_region->get_data_size());

			hashing_context.update(stream_peer.get_data_array());
			
			PackedByteArray result = hashing_context.finish();
			String hex_code = String::hex_encode_buffer(result.ptr(), 20);

			String texture_location_path = "res://gdraider/textures/";
			String raw_location_path = texture_location_path + "raw/";

			if (p_room_name_table.has(hex_code)) {
				if (TRFileAccess::exists(raw_location_path + hex_code + ".png")) {
					DirAccess::remove_absolute(raw_location_path + hex_code + ".png");
				}
				if (TRFileAccess::exists(raw_location_path + hex_code + ".png.import")) {
					DirAccess::remove_absolute(raw_location_path + hex_code + ".png.import");
				}

				String value = p_room_name_table[hex_code];

				String save_location_path = texture_location_path;

				int32_t slice_count = value.get_slice_count("/");
				if (slice_count == 0) {
					Ref<DirAccess> dir_access = DirAccess::create_for_path(save_location_path);
					dir_access->make_dir_recursive(save_location_path);

					save_location_path += value;
				} else {
					String texture_file_name = value.get_slice("/", slice_count-1);

					save_location_path += value.trim_suffix(texture_file_name);
					Ref<DirAccess> dir_access = DirAccess::create_for_path(save_location_path);
					dir_access->make_dir_recursive(save_location_path);

					save_location_path += texture_file_name;
				}

				save_location_path += ".png";
				image_region->save_png(save_location_path);
			} else {
				Ref<DirAccess> dir_access = DirAccess::create_for_path(raw_location_path);
				dir_access->make_dir_recursive(raw_location_path);
				image_region->save_png(raw_location_path + hex_code + ".png");
			}

			index++;
		}
	}
}

Ref<Shader> generate_shader(bool p_transparent, int32_t p_stencil) {
	Ref<Shader> shader = memnew(Shader);
	String shader_code = "";

	shader_code = "shader_type spatial;\n";
	if (p_stencil >= 0) {
		shader_code += "render_mode blend_mix, depth_draw_always, cull_back, diffuse_burley, specular_schlick_ggx, ambient_light_disabled;\n";
		shader_code += "stencil_mode read, compare_equal, " + itos(p_stencil) + ";\n\n";
	} else {
		shader_code += "render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx, ambient_light_disabled;\n\n";
	}
	shader_code += "uniform sampler2D texture_albedo : source_color, filter_nearest, repeat_disable;\n\n";
	shader_code += 
			"void fragment() {\n\
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
			ROUGHNESS = 1.0;\n";
	if (p_transparent) {
		shader_code += 
			"\t\tALPHA = albedo_tex.a;\n";
		if (p_stencil < 0) {
			shader_code +=
				"\t\tALPHA_SCISSOR_THRESHOLD = 1.0;\n";
		}
	} else {
		if (p_stencil >= 0) {
			shader_code +=
				"\t\tALPHA = 1.0;\n";
		}
	}
	shader_code += "}";

	shader->set_code(shader_code);

	return shader;
}

Node3D *generate_godot_scene(
	Node *p_root,
	Ref<TRLevelData> p_level_data,
	bool p_lara_only) {

	ERR_FAIL_COND_V(p_level_data.is_null(), nullptr);

	Ref<Shader> level_solid_shader = generate_shader(false, -1);
	Ref<Shader> level_transparent_shader = generate_shader(true, -1);

	Ref<Material> palette_material;
	
	// Palette Texture
	if (!p_level_data->palette.is_empty()) {
		Ref<Image> palette_image = memnew(Image(TR_TEXTILE_SIZE, TR_TEXTILE_SIZE, false, Image::FORMAT_RGBA8));
		for (int32_t x = 0; x < TR_TEXTILE_SIZE; x++) {
			for (int32_t y = 0; y < TR_TEXTILE_SIZE; y++) {
				uint32_t index = ((y / 16) * 16) + (x / 16);

				TRColor3 color = p_level_data->palette.get(index);
				palette_image->set_pixel(x, y, Color(((float)color.r / 255.0f), ((float)color.g / 255.0f), ((float)color.b / 255.0f), 1.0f));
			}
		}
		Ref<ImageTexture> it = ImageTexture::create_from_image(palette_image);
		palette_material = generate_tr_godot_generic_material(it, false);

		String palette_path = String("Pal") + String(".png");
		palette_image->save_png(palette_path);
	}

	Vector<Ref<Image>> images;
	Vector<Ref<ImageTexture>> image_textures;

	for (int32_t i = 0; i < p_level_data->level_textures.size(); i++) {
		PackedByteArray current_texture = p_level_data->level_textures.get(i);
		Ref<Image> image = memnew(Image(TR_TEXTILE_SIZE, TR_TEXTILE_SIZE, false, Image::FORMAT_RGBA8));
		if (p_level_data->texture_type == TR_TEXTURE_TYPE_8_PAL) {
			for (int32_t x = 0; x < TR_TEXTILE_SIZE; x++) {
				for (int32_t y = 0; y < TR_TEXTILE_SIZE; y++) {
					uint8_t index = current_texture.get((y * TR_TEXTILE_SIZE) + x);

					if (index == 0x00) {
						TRColor3 color = p_level_data->palette.get(index);
						image->set_pixel(x, y, Color(0.0f, 0.0f, 0.0f, 0.0f));
					}
					else {
						TRColor3 color = p_level_data->palette.get(index);
						image->set_pixel(x, y, Color(((float)color.r / 255.0f), ((float)color.g / 255.0f), ((float)color.b / 255.0f), 1.0f));
					}
				}
			}
		} else if (p_level_data->texture_type == TR_TEXTURE_TYPE_16) {
			for (int32_t x = 0; x < TR_TEXTILE_SIZE; x++) {
				for (int32_t y = 0; y < TR_TEXTILE_SIZE; y++) {
					uint32_t index = ((y * TR_TEXTILE_SIZE) + x) * sizeof(uint16_t);
					uint16_t pixel = current_texture.get(index + 0) | (uint16_t)current_texture.get(index + 1) << 8;

					image->set_pixel(x, y, Color(
						((float)((pixel & 0x7c00) >> 10) / 31.0f),
						((float)((pixel & 0x03e0) >> 5) / 31.0f),
						((float)((pixel & 0x001f)) / 31.0f),
						((float)((pixel & 0x8000) >> 15) / 1.0f)
					));
				}
			}
		} else if (p_level_data->texture_type == TR_TEXTURE_TYPE_32) {
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

		images.push_back(image);
		image_textures.push_back(ImageTexture::create_from_image(image));
	}

	for (int32_t i = 0; i < p_level_data->entity_textures.size(); i++) {
		PackedByteArray current_texture = p_level_data->entity_textures.get(i);
		Ref<Image> image = memnew(Image(TR_TEXTILE_SIZE, TR_TEXTILE_SIZE, false, Image::FORMAT_RGBA8));
		if (p_level_data->texture_type == TR_TEXTURE_TYPE_8_PAL) {
			for (int32_t x = 0; x < TR_TEXTILE_SIZE; x++) {
				for (int32_t y = 0; y < TR_TEXTILE_SIZE; y++) {
					uint8_t index = current_texture.get((y * TR_TEXTILE_SIZE) + x);

					if (index == 0x00) {
						TRColor3 color = p_level_data->palette.get(index);
						image->set_pixel(x, y, Color(0.0f, 0.0f, 0.0f, 0.0f));
					}
					else {
						TRColor3 color = p_level_data->palette.get(index);
						image->set_pixel(x, y, Color(((float)color.r / 255.0f), ((float)color.g / 255.0f), ((float)color.b / 255.0f), 1.0f));
					}
				}
			}
		} else if (p_level_data->texture_type == TR_TEXTURE_TYPE_16) {
			for (int32_t x = 0; x < TR_TEXTILE_SIZE; x++) {
				for (int32_t y = 0; y < TR_TEXTILE_SIZE; y++) {
					uint32_t index = ((y * TR_TEXTILE_SIZE) + x) * sizeof(uint16_t);
					uint16_t pixel = current_texture.get(index + 0) | (uint16_t) current_texture.get(index + 1) << 8;

					image->set_pixel(x, y, Color(
						((float)((pixel & 0x7c00) >> 10) / 31.0f),
						((float)((pixel & 0x03e0) >> 5) / 31.0f),
						((float)((pixel & 0x001f)) / 31.0f),
						((float)((pixel & 0x8000) >> 15) / 1.0f)
					));
				}
			}
		} else if (p_level_data->texture_type == TR_TEXTURE_TYPE_32) {
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

		images.push_back(image);
		image_textures.push_back(ImageTexture::create_from_image(image));
	}

	TRGodotMaterialTable material_table;

	for (int32_t i = 0; i < image_textures.size(); i++) {
		material_table.materials.level_solid_materials.append(generate_tr_godot_shader_material(image_textures[i], level_solid_shader));
		material_table.materials.level_transparent_materials.append(generate_tr_godot_shader_material(image_textures[i], level_transparent_shader));
		material_table.materials.entity_solid_materials.append(generate_tr_godot_generic_material(image_textures[i], false));
		material_table.materials.entity_transparent_materials.append(generate_tr_godot_generic_material(image_textures[i], true));
	}


	Vector<Ref<ArrayMesh>> meshes;
	for (TRMesh& tr_mesh : p_level_data->types.meshes) {
		Ref<ArrayMesh> mesh = tr_mesh_to_godot_mesh(tr_mesh, material_table.materials.entity_solid_materials, material_table.materials.entity_transparent_materials, palette_material, p_level_data->types.texture_infos);
		meshes.push_back(mesh);
	}

	Vector<Ref<AudioStream>> samples;

	// Audio
	if (!p_level_data->sound_buffer.is_empty()) {
		for (TRSoundInfo tr_sound_info : p_level_data->sound_infos) {
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
				if (p_level_data->sound_indices.size() > sample_id + 1) {
					uint32_t sample_offset = p_level_data->sound_indices.get(sample_id + i);
					Ref<TRFileAccess> buffer_access = TRFileAccess::create_from_buffer(p_level_data->sound_buffer);

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
			p_level_data->types.moveable_info_map,
			p_level_data->types,
			meshes,
			samples,
			p_level_data->format,
			p_level_data->is_using_auxiliary_animation);

		for (Node3D *moveable : moveable_node) {
			rooms_node->add_child(moveable);
			set_owner_recursively(moveable, scene_owner);
			moveable->set_display_folded(true);
		}

		Node3D *entities_node = memnew(Node3D);
		p_root->add_child(entities_node);
		entities_node->set_name("TREntities");
		entities_node->set_owner(scene_owner);

		uint32_t entity_idx = 0;
		for (const TREntity& entity : p_level_data->entities) {
			Node3D *entity_node = memnew(Node3D);

			real_t degrees_rotation = (real_t)((entity.transform.rot.y / 16384.0) * -90.0) + 180.0;
			entity_node->set_transform(Transform3D(Basis().rotated(
				Vector3(0.0, 1.0, 0.0), Math::deg_to_rad(degrees_rotation)), Vector3(
					entity.transform.pos.x * TR_TO_GODOT_SCALE,
					entity.transform.pos.y * -TR_TO_GODOT_SCALE,
					entity.transform.pos.z * -TR_TO_GODOT_SCALE)));

			entities_node->add_child(entity_node);
			entity_node->set_name("Entity_" + itos(entity_idx) + " (" + get_type_info_name(entity.type_id, p_level_data->format) + ")");
			entity_node->set_owner(scene_owner);
			entity_node->set_display_folded(true);

			if (p_level_data->types.moveable_info_map.has(entity.type_id)) {
				Node3D *new_node = create_godot_moveable_model(
					entity.type_id,
					p_level_data->types.moveable_info_map[entity.type_id],
					p_level_data->types,
					meshes,
					samples,
					p_level_data->format,
					p_level_data->is_using_auxiliary_animation,
					false,
					false);
				ERR_FAIL_NULL_V(new_node, nullptr);
				Node3D *type = new_node;
				entity_node->add_child(type);
				set_owner_recursively(type, scene_owner);
				type->set_display_folded(true);
			}
			entity_idx++;
		}

		HashMap<String, String> room_texture_name_table;
		Ref<ConfigFile> cf;
		cf.instantiate();
		if (cf->load("res://addons/gdr_importer/texture_mapping_table.cfg") == OK) {
			PackedStringArray keys = cf->get_section_keys("room_texture_remaps");
			for (int i = 0; i < keys.size(); i++) {
				String value = cf->get_value("room_texture_remaps", keys[i], "");
				if (!value.is_empty()) {
					if (room_texture_name_table.has(keys[i])) {
						print_error("Room texture name table already has a hash entry for " + keys[i]);
						continue;
					}

					room_texture_name_table.insert(keys[i], value);
				}
			}
		}
		remap_room_textures(p_level_data->rooms, images, p_level_data->types.texture_infos, room_texture_name_table);

		HashMap<int32_t, Vector<TRRoomPortal>> dummy_room_portals;

		uint32_t room_idx = 0;
		for (const TRRoom& room : p_level_data->rooms) {
			int32_t current_room_layer = get_room_layer(room_idx);

			if (current_room_layer == 0) {
				Node3D* node_3d = memnew(Node3D);
				if (node_3d) {
					node_3d->set_name(String("Room_") + itos(room_idx));
					node_3d->set_display_folded(true);
					rooms_node->add_child(node_3d);

					Vector3 room_size = Vector3(
						real_t(room.sector_count_z) * TR_SQUARE_SIZE,
						(room.info.y_bottom - room.info.y_top) * TR_TO_GODOT_SCALE,
						real_t(room.sector_count_x) * TR_SQUARE_SIZE
					);
					Vector3 room_offset = room_size / 2.0;

					Vector3 room_position = Vector3(
						room.info.x * TR_TO_GODOT_SCALE + room_offset.x,
						real_t(-room.info.y_bottom) * TR_TO_GODOT_SCALE,
						-(room.info.z * TR_TO_GODOT_SCALE + room_offset.z)
					);

					node_3d->set_position(room_position);

					node_3d->set_owner(rooms_node->get_owner());


					// Room Mesh
					MeshInstance3D* mi = memnew(MeshInstance3D);
					if (mi) {
						mi->set_name(String("RoomMesh_") + itos(room_idx));
						node_3d->add_child(mi);

						Ref<ArrayMesh> mesh = tr_room_data_to_godot_mesh(
							room.data,
							material_table,
							p_level_data->types,
							Vector3(-room_offset.x, -room_position.y, room_offset.z),
							Vector<TRRoomPortal>()
						);

						mi->set_position(Vector3(0.0, 0.0, 0.0));
						mi->set_owner(scene_owner);
						mi->set_mesh(mesh);
						mi->set_layer_mask(1 << 0);
					}

					// Room Collision
					StaticBody3D* static_body = memnew(StaticBody3D);
					if (static_body) {
						static_body->set_name(String("RoomStaticBody3D_") + itos(room_idx));
						node_3d->add_child(static_body);
						static_body->set_owner(scene_owner);
						static_body->set_position(Vector3(0.0, 0.0, 0.0));

						CollisionShape3D* collision_shape = tr_room_to_godot_collision_shape(
							room,
							p_level_data->floor_data,
							p_level_data->rooms,
							Vector3(-room_offset.x, -room_position.y, room_offset.z));

						static_body->add_child(collision_shape);
						collision_shape->set_owner(scene_owner);
						collision_shape->set_position(Vector3());
					}

					// Static Meshes
					uint32_t static_mesh_idx = 0;
					for (const TRRoomStaticMesh& room_static_mesh : room.room_static_meshes) {
						int32_t mesh_static_number = room_static_mesh.mesh_id;

						if (p_level_data->types.static_info_map.has(mesh_static_number)) {
							TRStaticInfo static_info = p_level_data->types.static_info_map[mesh_static_number];
						}

						if (p_level_data->types.static_info_map.has(mesh_static_number)) {
							TRStaticInfo static_info = p_level_data->types.static_info_map[mesh_static_number];
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
						static_mesh_idx++;
					}

					// Room Ambience
					ReflectionProbe* probe = memnew(ReflectionProbe);
					if (probe) {
						probe->set_name("RoomAmbience");
						probe->set_position(Vector3(0.0, room_offset.y, 0.0));
						probe->set_size(Vector3(room_size.x, room_size.y + (TR_SQUARE_SIZE * 2.0), room_size.z));
						probe->set_ambient_color(room.ambient_light);
						probe->set_ambient_mode(ReflectionProbe::AMBIENT_COLOR);
						probe->set_as_interior(true);
						probe->set_cull_mask(0);
						probe->set_reflection_mask((1 << 1));
						probe->set_blend_distance(TR_SQUARE_SIZE);

						node_3d->add_child(probe);
						probe->set_owner(scene_owner);
					}

					// Lights
					int32_t light_idx = 0;
					for (const TRRoomLight& room_light : room.lights) {
						OmniLight3D* light_3d = memnew(OmniLight3D);
						if (probe) {
							light_3d->set_name("RoomLight_" + itos(light_idx));

							Vector3 room_relative_position = Vector3(
								room_light.pos.x * TR_TO_GODOT_SCALE,
								room_light.pos.y * -TR_TO_GODOT_SCALE,
								room_light.pos.z * -TR_TO_GODOT_SCALE
							) - room_position;

							light_3d->set_cull_mask((1 << 1)); // Dynamic
							light_3d->set_position(room_relative_position);
							light_3d->set_color(room_light.color);
							light_3d->set_param(Light3D::PARAM_RANGE, room_light.range);
							light_3d->set_param(Light3D::PARAM_ENERGY, room_light.energy);
							light_3d->set_param(Light3D::PARAM_ATTENUATION, room_light.attenuation);

							node_3d->add_child(light_3d);
							light_3d->set_owner(scene_owner);
						}

						light_idx++;
					}

					int32_t portal_idx = 0;
					for (const TRRoomPortal& room_portal : room.portals) {
						uint32_t adjoining_room = room_portal.adjoining_room;
						int32_t adjoining_room_layer = get_room_layer(room_portal.adjoining_room);
						if (adjoining_room_layer != current_room_layer) {
							if (!dummy_room_portals.has(current_room_layer)) {
								dummy_room_portals[current_room_layer] = Vector<TRRoomPortal>();
							}

							// Generate stencil versions
							if (!material_table.read_stencil_materials.has(adjoining_room_layer)) {
								Ref<Shader> level_solid_shader_read_stencil = generate_shader(false, adjoining_room_layer);
								Ref<Shader> level_transparent_shader_read_stencil = generate_shader(true, adjoining_room_layer);

								TRGodotMaterials materials;
								for (const Ref<Material>& material : material_table.materials.level_solid_materials) {
									Ref<ShaderMaterial> dup_material = material->duplicate();
									dup_material->set_shader(level_solid_shader_read_stencil);
									materials.level_solid_materials.append(dup_material);
								}
								for (const Ref<Material>& material : material_table.materials.level_transparent_materials) {
									Ref<ShaderMaterial> dup_material = material->duplicate();
									dup_material->set_shader(level_transparent_shader_read_stencil);
									materials.level_transparent_materials.append(dup_material);
								}
								for (const Ref<Material>& material : material_table.materials.entity_solid_materials) {
									Ref<Material> dup_material = material->duplicate();
									materials.entity_solid_materials.append(dup_material);
								}
								for (const Ref<Material>& material : material_table.materials.entity_transparent_materials) {
									Ref<Material> dup_material = material->duplicate();
									materials.entity_transparent_materials.append(dup_material);
								}
								material_table.read_stencil_materials.insert(adjoining_room_layer, materials);
							}
							if (!material_table.portal_stencil_materials.has(adjoining_room_layer)) {
								Ref<StandardMaterial3D> stencil_material = memnew(StandardMaterial3D);

								stencil_material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
								stencil_material->set_albedo(Color(1.0, 1.0, 1.0, 0.0));
								stencil_material->set_stencil_mode(BaseMaterial3D::STENCIL_MODE_CUSTOM);
								stencil_material->set_stencil_flags(BaseMaterial3D::STENCIL_FLAG_WRITE);
								stencil_material->set_stencil_compare(BaseMaterial3D::STENCIL_COMPARE_ALWAYS);
								stencil_material->set_stencil_reference(adjoining_room_layer);
								stencil_material->set_render_priority(-1);

								material_table.portal_stencil_materials.insert(adjoining_room_layer, stencil_material);
							}

							TRRoomPortal room_portal_global_space;
							room_portal_global_space.adjoining_room = room_portal.adjoining_room;
							room_portal_global_space.normal = room_portal.normal;

							for (int32_t portal_vert_idx = 0; portal_vert_idx < 4; portal_vert_idx++) {
								room_portal_global_space.vertices[portal_vert_idx] = room_portal.vertices[portal_vert_idx];
								room_portal_global_space.vertices[portal_vert_idx].x -= -room.info.x;
								room_portal_global_space.vertices[portal_vert_idx].z -= -room.info.z;
							}

							// TODO: avoid duplicates!
							dummy_room_portals[current_room_layer].push_back(room_portal_global_space);
						}
					}
				}
			}
			room_idx++;
		}

		Vector<uint32_t> dummy_rooms_for_current_layer;
		HashMap<uint32_t, Vector<TRRoomPortal>> dummy_room_portal_map;

		for (const TRRoomPortal& dummy_room_portal : dummy_room_portals[0]) {
			dummy_room_portal_map[dummy_room_portal.adjoining_room].append(dummy_room_portal);
			if (!(dummy_rooms_for_current_layer.has(dummy_room_portal.adjoining_room))) {
				dummy_rooms_for_current_layer.append(dummy_room_portal.adjoining_room);
			}
		}

		for (const uint32_t& dummy_room_idx : dummy_rooms_for_current_layer) {
			const TRRoom& dummy_room = p_level_data->rooms[dummy_room_idx];

			Node3D* dummy_node_3d = memnew(Node3D);
			if (dummy_node_3d) {
				dummy_node_3d->set_name(String("DummyRoom_") + itos(dummy_room_idx));
				dummy_node_3d->set_display_folded(true);
				rooms_node->add_child(dummy_node_3d);

				Vector3 dummy_room_size = Vector3(
					real_t(dummy_room.sector_count_z) * TR_SQUARE_SIZE,
					(dummy_room.info.y_bottom - dummy_room.info.y_top) * TR_TO_GODOT_SCALE,
					real_t(dummy_room.sector_count_x) * TR_SQUARE_SIZE
				);
				Vector3 dummy_room_offset = dummy_room_size / 2.0;

				Vector3 dummy_room_position = Vector3(
					dummy_room.info.x * TR_TO_GODOT_SCALE + dummy_room_offset.x,
					real_t(-dummy_room.info.y_bottom) * TR_TO_GODOT_SCALE,
					-(dummy_room.info.z * TR_TO_GODOT_SCALE + dummy_room_offset.z)
				);

				dummy_node_3d->set_position(dummy_room_position);

				dummy_node_3d->set_owner(rooms_node->get_owner());


				// Room Mesh
				MeshInstance3D* dummy_mi = memnew(MeshInstance3D);
				if (dummy_mi) {
					dummy_mi->set_name(String("DummyRoomMesh_") + itos(dummy_room_idx));
					dummy_node_3d->add_child(dummy_mi);

					Vector<TRRoomPortal> fixed_room_portals;

					Vector<TRRoomPortal> global_room_portals = dummy_room_portal_map[dummy_room_idx];
					for (const TRRoomPortal& global_room_portal : global_room_portals) {
						TRRoomPortal fixed_room_portal = global_room_portal;
						for (int32_t portal_vert_idx = 0; portal_vert_idx < 4; portal_vert_idx++) {
							fixed_room_portal.vertices[portal_vert_idx].x += -dummy_room.info.x;
							fixed_room_portal.vertices[portal_vert_idx].z += -dummy_room.info.z;
						}
						fixed_room_portals.append(fixed_room_portal);
					}

					Ref<ArrayMesh> dummy_mesh = tr_room_data_to_godot_mesh(
						dummy_room.data,
						material_table,
						p_level_data->types,
						Vector3(-dummy_room_offset.x, -dummy_room_position.y, dummy_room_offset.z),
						fixed_room_portals,
						get_room_layer(dummy_room_idx)
					);

					dummy_mi->set_position(Vector3(0.0, 0.0, 0.0));
					dummy_mi->set_owner(scene_owner);
					dummy_mi->set_mesh(dummy_mesh);
					dummy_mi->set_layer_mask(1 << 0);
				}
			}
		}

		return rooms_node;
	} else {
		if (p_lara_only) {
			//Vector<TRMoveableInfo> moveables;
			//moveables.append(p_level_data->types.moveable_info_map[0]);
			//TRMoveableReAtlasResult result = reatlas_object_group_textures(moveables, p_level_data->types.meshes, p_level_data->types.texture_infos, images);

			//Vector<Ref<ArrayMesh>> reatlased_mesh_group = meshes;
			//for (TRMesh& tr_mesh : p_level_data->types.meshes) {
			//	Ref<ArrayMesh> mesh = tr_mesh_to_godot_mesh(tr_mesh, entity_materials, entity_trans_materials, palette_material, p_level_data->types.texture_infos);
			//	meshes.push_back(mesh);
			//}
		}

		Node3D *new_node = create_godot_moveable_model(
			0,
			p_level_data->types.moveable_info_map[0],
			p_level_data->types,
			meshes,
			samples,
			p_level_data->format,
			p_level_data->is_using_auxiliary_animation,
			true,
			true);
		ERR_FAIL_COND_V(!new_node, nullptr);

		p_root->add_child(new_node);
		new_node->set_owner(scene_owner);
		set_owner_recursively(new_node, scene_owner);
		new_node->set_display_folded(true);

		return new_node;
	}
}
