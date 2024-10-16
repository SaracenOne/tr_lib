#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "core/io/file_access.h"
#include "core/io/stream_peer.h"

#include "tr_module_extension_abstraction_layer.hpp"

#ifdef IS_MODULE
using namespace godot;
#endif

class TRFileAccess : public RefCounted {
	Ref<StreamPeerBuffer> stream_peer_buffer;

	TRFileAccess(Ref<FileAccess> p_file) {
		ERR_FAIL_COND(p_file.is_null());
		uint64_t file_length = p_file->get_length();
		PackedByteArray pba = p_file->get_buffer(file_length);
		stream_peer_buffer.instantiate();
		stream_peer_buffer->set_data_array(pba);
	}

	TRFileAccess(PackedByteArray p_pba) {
		stream_peer_buffer.instantiate();
		stream_peer_buffer->set_data_array(p_pba);
	}

public:
	int8_t get_s8() {
		int8_t s8 = static_cast<int8_t>(stream_peer_buffer->get_8());
		return s8;
	}

	int16_t get_s16() {
		int16_t s16 = static_cast<int16_t>(stream_peer_buffer->get_16());
		return s16;
	}

	int32_t get_s32() {
		int32_t s32 = static_cast<int32_t>(stream_peer_buffer->get_32());
		return s32;
	}

	uint8_t get_u8() {
		uint8_t u8 = static_cast<uint8_t>(stream_peer_buffer->get_u8());
		return u8;
	}

	uint16_t get_u16() {
		uint16_t u16 = static_cast<uint16_t>(stream_peer_buffer->get_u16());
		return u16;
	}

	uint32_t get_u32() {
		uint32_t u32 = static_cast<uint32_t>(stream_peer_buffer->get_u32());
		return u32;
	}

	float get_float() {
		float flt = static_cast<float>(stream_peer_buffer->get_float());
		return flt;
	}

	uint64_t get_position() {
		return stream_peer_buffer->get_position();
	}

	void seek(uint64_t p_position) {
		stream_peer_buffer->seek(p_position);
	}

	int32_t get_size() {
		return stream_peer_buffer->get_size();
	}

	PackedByteArray get_buffer(uint64_t p_length) {
		PackedByteArray buffer;
		buffer.resize(p_length);
		uint64_t buffer_size = stream_peer_buffer->get_data(buffer.ptrw(), p_length);

		return buffer;
	}

	PackedInt32Array get_buffer_int32(uint64_t p_length) {
		PackedInt32Array buffer;
		buffer.resize(p_length);
		uint64_t buffer_size = stream_peer_buffer->get_data((uint8_t *)buffer.ptrw(), p_length * sizeof(int32_t));

		return buffer;
	}

	String get_fixed_string(uint64_t p_length) {
		PackedByteArray buffer = get_buffer(p_length);
		String string;
		string.parse_utf8((const char*)buffer.ptr(), buffer.size());

		return string;
	}

	static Ref<TRFileAccess> open(const String &p_path, Error *r_error) {
		Ref<FileAccess> file_access = FileAccess::open(p_path, FileAccess::READ, r_error);
		if (*r_error == OK) {
			Ref<TRFileAccess> tr_file_access = memnew(TRFileAccess(file_access));
			return tr_file_access;
		}
		return nullptr;
	}

	static Ref<TRFileAccess> create_from_buffer(const PackedByteArray p_pba) {
		Ref<TRFileAccess> tr_file_access = memnew(TRFileAccess(p_pba));
		return tr_file_access;
	}
};
