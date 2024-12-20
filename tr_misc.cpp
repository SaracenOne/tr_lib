#include "tr_misc.hpp"

Ref<AnimationNodeBlendTree> create_lara_root_animation_node(Ref<AnimationNodeStateMachine> p_state_machine) {
	Ref<AnimationNodeBlendTree> blend_tree = memnew(AnimationNodeBlendTree);

	// Standard State Machine
	blend_tree->add_node("StateMachine", p_state_machine, Vector2(0, 560));

	// Left Arm State Machine
	Ref<AnimationNodeStateMachine> left_arm_state_machine = memnew(AnimationNodeStateMachine);
	left_arm_state_machine->set_name("LeftArmStateMachine");
	blend_tree->add_node("LeftArmStateMachine", left_arm_state_machine, Vector2(0, 20));

	// Right Arm State Machine
	Ref<AnimationNodeStateMachine> right_arm_state_machine = memnew(AnimationNodeStateMachine);
	right_arm_state_machine->set_name("RightArmStateMachine");
	blend_tree->add_node("RightArmStateMachine", right_arm_state_machine, Vector2(0, 300));

	// Arm Blending
	Ref<AnimationNodeBlend2> arms_blend_node = memnew(AnimationNodeBlend2);
	blend_tree->add_node("ArmsBlending", arms_blend_node, Vector2(300, -20));
	blend_tree->connect_node("ArmsBlending", 0, "LeftArmStateMachine");
	blend_tree->connect_node("ArmsBlending", 1, "RightArmStateMachine");

	// Right Arm Filtering
	arms_blend_node->set_filter_enabled(true);
	arms_blend_node->set_filter_path(NodePath("%GeneralSkeleton:RightShoulder"), true);
	arms_blend_node->set_filter_path(NodePath("%GeneralSkeleton:RightUpperArm"), true);
	arms_blend_node->set_filter_path(NodePath("%GeneralSkeleton:RightLowerArm"), true);
	arms_blend_node->set_filter_path(NodePath("%GeneralSkeleton:RightHand"), true);

	// Final Blending
	Ref<AnimationNodeBlend2> final_blend_node = memnew(AnimationNodeBlend2);
	blend_tree->add_node("FinalBlending", final_blend_node, Vector2(540, -80));
	blend_tree->connect_node("FinalBlending", 0, "StateMachine");
	blend_tree->connect_node("FinalBlending", 1, "ArmsBlending");

	// Both Arm Filtering
	final_blend_node->set_filter_enabled(true);
	final_blend_node->set_filter_path(NodePath("%GeneralSkeleton:LeftShoulder"), true);
	final_blend_node->set_filter_path(NodePath("%GeneralSkeleton:LeftUpperArm"), true);
	final_blend_node->set_filter_path(NodePath("%GeneralSkeleton:LeftLowerArm"), true);
	final_blend_node->set_filter_path(NodePath("%GeneralSkeleton:LeftHand"), true);
	final_blend_node->set_filter_path(NodePath("%GeneralSkeleton:RightShoulder"), true);
	final_blend_node->set_filter_path(NodePath("%GeneralSkeleton:RightUpperArm"), true);
	final_blend_node->set_filter_path(NodePath("%GeneralSkeleton:RightLowerArm"), true);
	final_blend_node->set_filter_path(NodePath("%GeneralSkeleton:RightHand"), true);

	// Output
	blend_tree->set_node_position("output", Vector2(780, -60));
	blend_tree->connect_node("output", 0, "FinalBlending");

	return blend_tree;
}

const AnimationNodeMetaInfo* get_animation_node_meta_info_for_animation(uint32_t p_type_info_id, StringName p_animation_name, TRLevelFormat p_level_format) {
	if (p_type_info_id == 0) {
		FIND_ANIMATION_META_INFO(lara_animation_node_meta_info, p_animation_name)
	}

	return nullptr;
}

const Ref<AnimationNode> get_animation_tree_root_node_for_object(uint32_t p_type_info_id, TRLevelFormat p_level_format, Ref<AnimationNodeStateMachine> p_state_machine) {
	if (p_type_info_id == 0) {
		return create_lara_root_animation_node(p_state_machine);
	} else {
		return p_state_machine;
	}
}