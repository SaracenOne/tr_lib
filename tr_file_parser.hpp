#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "core/io/file_access.h"

#include "tr_module_extension_abstraction_layer.hpp"

#ifdef IS_MODULE
using namespace godot;
#endif

class TRFileAccess : public RefCounted {
	Ref<FileAccess> file;

	TRFileAccess(Ref<FileAccess> p_file) {
		ERR_FAIL_COND(p_file.is_null());
		file = p_file;
	}

public:
	int8_t get_s8() {
		return static_cast<int8_t>(file->get_8());
	}

	int16_t get_s16() {
		return static_cast<int16_t>(file->get_16());
	}

	int32_t get_s32() {
		return static_cast<int32_t>(file->get_32());
	}

	uint8_t get_u8() {
		return static_cast<uint8_t>(file->get_8());
	}

	uint16_t get_u16() {
		return static_cast<uint16_t>(file->get_16());
	}

	uint32_t get_u32() {
		return static_cast<uint32_t>(file->get_32());
	}

	uint64_t get_position() {
		return file->get_position();
	}

	void seek(uint64_t p_position) {
		file->seek(p_position);
	}

	PackedByteArray get_buffer(uint64_t p_length) {
		PackedByteArray buffer;
		buffer.resize(p_length);
		uint64_t buffer_size = file->get_buffer(buffer.ptrw(), p_length);

		return buffer;
	}

	PackedInt32Array get_buffer_int32(uint64_t p_length) {
		PackedInt32Array buffer;
		buffer.resize(p_length);
		uint64_t buffer_size = file->get_buffer((uint8_t *)buffer.ptrw(), p_length);

		return buffer;
	}

	static Ref<TRFileAccess> open(const String &p_path, int p_mode_flags, Error *r_error) {
		Ref<FileAccess> file_access = FileAccess::open(p_path, p_mode_flags, r_error);
		Ref<TRFileAccess> tr_file_access = memnew(TRFileAccess(file_access));

		return tr_file_access;
	}
};
