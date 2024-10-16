#ifndef TR_LEVEL_EDITOR_PLUGIN_H
#define TR_LEVEL_EDITOR_PLUGIN_H

#include "../tr_level.hpp"

#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "scene/gui/button.h"
#include "scene/gui/box_container.h"
#include "scene/gui/check_box.h"

class TRLevelEditorPlugin : public EditorPlugin {
	GDCLASS(TRLevelEditorPlugin, EditorPlugin);

protected:
	HBoxContainer *container = nullptr;

	TRLevel *level_node = nullptr;
	Button *reload_level_button = nullptr;
	CheckBox *lara_only_toggle = nullptr;

	void _button_pressed() {
		ERR_FAIL_COND(!level_node);

		level_node->clear_level();
		level_node->load_level(lara_only_toggle->is_pressed());
		get_editor_interface()->mark_scene_as_unsaved();
	}
public:
	virtual String get_name() const override { return "TRLevel"; }
	bool has_main_screen() const override { return false; }

	void edit(Object* p_object) {
		level_node = Object::cast_to<TRLevel>(p_object);
	}

	bool handles(Object* p_object) const {
		return p_object->is_class("TRLevel");
	}

	void make_visible(bool p_visible) {
		if (p_visible) {
			container->show();
		}
		else {
			container->hide();
		}
	}

	TRLevelEditorPlugin() {
		container = memnew(HBoxContainer);

		reload_level_button = memnew(Button);
		reload_level_button->set_text("Reload Level");
		reload_level_button->connect("pressed", callable_mp(this, &TRLevelEditorPlugin::_button_pressed));

		lara_only_toggle = memnew(CheckBox);
		lara_only_toggle->set_text("Lara Only");

		container->add_child(reload_level_button);
		container->add_child(lara_only_toggle);

		container->hide();

		Node3DEditor::get_singleton()->add_control_to_menu_panel(container);
	}

	~TRLevelEditorPlugin() {
		Node3DEditor::get_singleton()->remove_control_from_menu_panel(container);

		if (reload_level_button) {
			reload_level_button->queue_free();
			reload_level_button = nullptr;
		}

		if (lara_only_toggle) {
			lara_only_toggle->queue_free();
			lara_only_toggle = nullptr;
		}

		if (container) {
			container->queue_free();
			container = nullptr;
		}
	}

};

#endif // TR_LEVEL_EDITOR_PLUGIN_H