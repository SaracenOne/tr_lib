#ifndef TR_LEVEL_EDITOR_PLUGIN_H
#define TR_LEVEL_EDITOR_PLUGIN_H

#include "../tr_level.hpp"

#include "editor/editor_node.h"
#include "editor/editor_plugin.h"
#include "editor/editor_scale.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "scene/gui/button.h"
#include "scene/gui/box_container.h"

class TRLevelEditorPlugin : public EditorPlugin {
	GDCLASS(TRLevelEditorPlugin, EditorPlugin);

protected:
	TRLevel *level_node = nullptr;
	Button *reload_level_button = nullptr;

	void _button_pressed() {
		ERR_FAIL_COND(!level_node);

		level_node->clear_level();
		level_node->load_level();
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
			reload_level_button->show();
		}
		else {
			reload_level_button->hide();
		}
	}

	TRLevelEditorPlugin() {
		reload_level_button = memnew(Button);
		reload_level_button->set_text("Reload Level");
		reload_level_button->connect("pressed", callable_mp(this, &TRLevelEditorPlugin::_button_pressed));
		reload_level_button->hide();

		Node3DEditor::get_singleton()->add_control_to_menu_panel(reload_level_button);
	}

	~TRLevelEditorPlugin() {
		Node3DEditor::get_singleton()->remove_control_from_menu_panel(reload_level_button);

		memdelete(reload_level_button);
		reload_level_button = nullptr;
	}

};

#endif // TR_LEVEL_EDITOR_PLUGIN_H